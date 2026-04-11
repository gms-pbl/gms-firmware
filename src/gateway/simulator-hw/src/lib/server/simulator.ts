import { mkdir, readdir, readFile, rm, unlink, writeFile } from "node:fs/promises";
import os from "node:os";
import path from "node:path";
import mqtt, { type MqttClient } from "mqtt";
import {
	DEFAULT_CONNECTION,
	DEFAULT_STATE,
	type BinaryValue,
	type ConnectionConfig,
	type ConnectionStatus,
	type DigitalInputState,
	type DigitalOutputState,
	type SimulationState,
	type SimulatorRuntime
} from "$lib/sim/types";

type StatePatch = Partial<{
	sensors: Partial<SimulationState["sensors"]>;
	digital_inputs: Partial<DigitalInputState>;
	digital_outputs: Partial<DigitalOutputState>;
}>;

const EVENT_LIMIT = 120;
const LEGACY_STATIC_CLIENT_ID = "virtual-portenta-zone1";

class SimulatorService {
	private state: SimulationState = structuredClone(DEFAULT_STATE);
	private connection: ConnectionConfig = this.readConnectionDefaults();
	private status: ConnectionStatus = {
		connected: false,
		last_error: null,
		last_connect_at: null
	};
	private events: string[] = [];
	private client: MqttClient | null = null;
	private publishTimer: ReturnType<typeof setInterval> | null = null;
	private initialized = false;
	private readonly generatedClientId = this.generateClientId();

	private readonly dataDir = process.env.SIM_DATA_DIR ?? path.resolve(process.cwd(), "sim-data");
	private readonly profilesDir = path.join(this.dataDir, "profiles");
	private readonly statePath = path.join(this.dataDir, "current-state.json");
	private readonly connectionPath = path.join(this.dataDir, "connection.json");

	public async init(): Promise<void> {
		if (this.initialized) {
			return;
		}

		await mkdir(this.profilesDir, { recursive: true });
		await this.loadPersistedConnection();
		await this.loadPersistedState();

		this.initialized = true;
		this.log("Simulator initialized.");

		if (this.connection.auto_connect) {
			await this.connect();
		}
	}

	public getRuntime(profiles: string[]): SimulatorRuntime {
		return {
			state: structuredClone(this.state),
			connection: { ...this.connection, password: this.connection.password ? "********" : "" },
			status: { ...this.status },
			profiles,
			events: [...this.events]
		};
	}

	public async listProfiles(): Promise<string[]> {
		const entries = await readdir(this.profilesDir, { withFileTypes: true });
		return entries
			.filter((entry) => entry.isFile() && entry.name.endsWith(".json"))
			.map((entry) => entry.name.replace(/\.json$/, ""))
			.sort((a, b) => a.localeCompare(b));
	}

	public async updateState(patch: StatePatch): Promise<void> {
		if (patch.sensors) {
			for (const [key, value] of Object.entries(patch.sensors)) {
				if (key in this.state.sensors && typeof value === "number" && Number.isFinite(value)) {
					this.state.sensors[key as keyof SimulationState["sensors"]] = value;
				}
			}
		}

		if (patch.digital_inputs) {
			this.applyBinaryPatch(this.state.digital_inputs, patch.digital_inputs);
		}

		if (patch.digital_outputs) {
			this.applyBinaryPatch(this.state.digital_outputs, patch.digital_outputs);
		}

		await this.persistState();
		this.log("State updated from UI/API.");
	}

	public async setConnection(patch: Partial<ConnectionConfig>): Promise<void> {
		const next = {
			...this.connection,
			...patch
		};

		next.host = String(next.host || "127.0.0.1").trim();
		next.telemetry_topic = String(next.telemetry_topic || "telemetry/zone1").trim();
		next.command_topic = String(next.command_topic || "commands/zone1/output").trim();
		next.client_id = String(next.client_id || "virtual-portenta-zone1").trim();
		next.port = this.clampInteger(next.port, 1, 65535, 1883);
		next.publish_interval_ms = this.clampInteger(next.publish_interval_ms, 200, 3_600_000, 10_000);
		next.auto_connect = Boolean(next.auto_connect);

		this.connection = next;
		await this.persistConnection();
		this.log("Connection settings updated.");

		if (this.status.connected) {
			await this.connect();
		} else {
			this.refreshPublishTimer();
		}
	}

	public async connect(): Promise<void> {
		await this.disconnect(false);

		const url = `mqtt://${this.connection.host}:${this.connection.port}`;
		this.log(`Connecting to ${url} ...`);

		this.client = mqtt.connect(url, {
			clientId: this.connection.client_id,
			username: this.connection.username || undefined,
			password: this.connection.password || undefined,
			reconnectPeriod: 3000,
			connectTimeout: 5000,
			clean: true
		});

		this.client.on("connect", () => {
			this.status.connected = true;
			this.status.last_error = null;
			this.status.last_connect_at = new Date().toISOString();
			this.log("MQTT connected.");

			if (this.client) {
				this.client.subscribe(this.connection.command_topic, (error) => {
					if (error) {
						this.log(`Subscribe failed: ${error.message}`);
						return;
					}
					this.log(`Subscribed to ${this.connection.command_topic}`);
				});
			}

			this.refreshPublishTimer();
			void this.publishTelemetry();
		});

		this.client.on("message", async (topic, payload) => {
			if (topic !== this.connection.command_topic) {
				return;
			}

			const text = payload.toString();
			this.log(`MQTT command <- ${text}`);

			try {
				const body = JSON.parse(text) as { channel?: number; state?: number };
				if (
					typeof body.channel !== "number" ||
					!Number.isInteger(body.channel) ||
					body.channel < 0 ||
					body.channel > 7
				) {
					throw new Error("Invalid channel");
				}

				const key = this.outputKeyFromChannel(body.channel);
				if (!key) {
					throw new Error("Unknown output channel");
				}

				this.state.digital_outputs[key] = this.normalizeBinary(body.state);
				await this.persistState();
				this.log(`Applied output command: ${key}=${this.state.digital_outputs[key]}`);
			} catch (error) {
				this.log(`Command parse error: ${error instanceof Error ? error.message : String(error)}`);
			}
		});

		this.client.on("error", (error) => {
			this.status.last_error = error.message;
			this.log(`MQTT error: ${error.message}`);
		});

		this.client.on("reconnect", () => {
			this.log("MQTT reconnecting...");
		});

		this.client.on("offline", () => {
			this.log("MQTT offline.");
		});

		this.client.on("end", () => {
			this.log("MQTT client ended.");
		});

		this.client.on("close", () => {
			this.status.connected = false;
			this.log("MQTT disconnected.");
			this.refreshPublishTimer();
		});
	}

	public async disconnect(logMessage = true): Promise<void> {
		if (this.client) {
			await new Promise<void>((resolve) => {
				this.client?.end(true, {}, () => resolve());
			});
			this.client = null;
		}

		this.status.connected = false;
		this.refreshPublishTimer();

		if (logMessage) {
			this.log("Disconnected by user.");
		}
	}

	public async saveProfile(name: string): Promise<void> {
		const safeName = this.sanitizeProfileName(name);
		const targetPath = path.join(this.profilesDir, `${safeName}.json`);
		await writeFile(targetPath, JSON.stringify(this.state, null, 2), "utf8");
		this.log(`Saved profile '${safeName}'.`);
	}

	public async loadProfile(name: string): Promise<void> {
		const safeName = this.sanitizeProfileName(name);
		const targetPath = path.join(this.profilesDir, `${safeName}.json`);
		const data = await readFile(targetPath, "utf8");
		const parsed = JSON.parse(data) as SimulationState;

		this.state = this.mergeState(DEFAULT_STATE, parsed);
		await this.persistState();
		this.log(`Loaded profile '${safeName}'.`);
	}

	public async deleteProfile(name: string): Promise<void> {
		const safeName = this.sanitizeProfileName(name);
		const targetPath = path.join(this.profilesDir, `${safeName}.json`);
		await unlink(targetPath);
		this.log(`Deleted profile '${safeName}'.`);
	}

	public async clearProfiles(): Promise<void> {
		const entries = await readdir(this.profilesDir, { withFileTypes: true });
		for (const entry of entries) {
			if (entry.isFile() && entry.name.endsWith(".json")) {
				await unlink(path.join(this.profilesDir, entry.name));
			}
		}
		this.log("Cleared all saved profiles.");
	}

	public async resetState(): Promise<void> {
		this.state = structuredClone(DEFAULT_STATE);
		await this.persistState();
		this.log("State reset to defaults.");
	}

	private refreshPublishTimer(): void {
		if (this.publishTimer) {
			clearInterval(this.publishTimer);
			this.publishTimer = null;
		}

		if (!this.status.connected) {
			return;
		}

		this.publishTimer = setInterval(() => {
			void this.publishTelemetry();
		}, this.connection.publish_interval_ms);
	}

	private async publishTelemetry(): Promise<void> {
		if (!this.client || !this.status.connected) {
			return;
		}

		const payload = {
			...this.state.sensors,
			...this.state.digital_inputs
		};

		const data = JSON.stringify(payload);
		this.client.publish(this.connection.telemetry_topic, data);
		this.log(`MQTT telemetry -> ${this.connection.telemetry_topic}`);
	}

	private async loadPersistedState(): Promise<void> {
		try {
			const data = await readFile(this.statePath, "utf8");
			const parsed = JSON.parse(data) as SimulationState;
			this.state = this.mergeState(DEFAULT_STATE, parsed);
			this.log("Loaded persisted runtime state.");
		} catch {
			await this.persistState();
		}
	}

	private async loadPersistedConnection(): Promise<void> {
		try {
			const data = await readFile(this.connectionPath, "utf8");
			const parsed = JSON.parse(data) as Partial<ConnectionConfig>;
			this.connection = {
				...this.connection,
				...parsed
			};

			if (!this.connection.client_id || this.connection.client_id === LEGACY_STATIC_CLIENT_ID) {
				this.connection.client_id = this.generatedClientId;
				await this.persistConnection();
				this.log(`Upgraded legacy MQTT client_id to '${this.connection.client_id}'.`);
			}
			this.log("Loaded persisted connection settings.");
		} catch {
			await this.persistConnection();
		}
	}

	private async persistState(): Promise<void> {
		await writeFile(this.statePath, JSON.stringify(this.state, null, 2), "utf8");
	}

	private async persistConnection(): Promise<void> {
		await writeFile(this.connectionPath, JSON.stringify(this.connection, null, 2), "utf8");
	}

	private mergeState(base: SimulationState, incoming: Partial<SimulationState>): SimulationState {
		return {
			sensors: {
				...base.sensors,
				...(incoming.sensors ?? {})
			},
			digital_inputs: {
				...base.digital_inputs,
				...(incoming.digital_inputs ?? {})
			},
			digital_outputs: {
				...base.digital_outputs,
				...(incoming.digital_outputs ?? {})
			}
		};
	}

	private applyBinaryPatch(
		target: DigitalInputState | DigitalOutputState,
		patch: Partial<DigitalInputState> | Partial<DigitalOutputState>
	): void {
		for (const [key, value] of Object.entries(patch)) {
			if (key in target) {
				(target as any)[key] = this.normalizeBinary(value);
			}
		}
	}

	private normalizeBinary(value: unknown): BinaryValue {
		return Number(value) >= 1 ? 1 : 0;
	}

	private outputKeyFromChannel(channel: number): keyof DigitalOutputState | null {
		const key = `dout_${String(channel).padStart(2, "0")}` as keyof DigitalOutputState;
		return key in this.state.digital_outputs ? key : null;
	}

	private sanitizeProfileName(name: string): string {
		const sanitized = name.trim().replace(/[^a-zA-Z0-9-_]/g, "_");
		if (!sanitized) {
			throw new Error("Profile name cannot be empty.");
		}
		return sanitized;
	}

	private log(message: string): void {
		const line = `${new Date().toISOString()} ${message}`;
		this.events.unshift(line);
		if (this.events.length > EVENT_LIMIT) {
			this.events.length = EVENT_LIMIT;
		}
	}

	private readConnectionDefaults(): ConnectionConfig {
		return {
			...DEFAULT_CONNECTION,
			host: process.env.MQTT_HOST ?? DEFAULT_CONNECTION.host,
			port: this.clampInteger(process.env.MQTT_PORT, 1, 65535, DEFAULT_CONNECTION.port),
			telemetry_topic: process.env.TELEMETRY_TOPIC ?? DEFAULT_CONNECTION.telemetry_topic,
			command_topic: process.env.COMMAND_TOPIC ?? DEFAULT_CONNECTION.command_topic,
			publish_interval_ms: this.clampInteger(
				process.env.PUBLISH_INTERVAL_MS,
				200,
				3_600_000,
				DEFAULT_CONNECTION.publish_interval_ms
			),
			client_id: process.env.MQTT_CLIENT_ID ?? this.generatedClientId,
			username: process.env.MQTT_USERNAME || undefined,
			password: process.env.MQTT_PASSWORD || undefined,
			auto_connect: (process.env.AUTO_CONNECT ?? "true").toLowerCase() !== "false"
		};
	}

	private generateClientId(): string {
		const safeHost = os
			.hostname()
			.toLowerCase()
			.replace(/[^a-z0-9-]/g, "-")
			.slice(0, 24);
		const suffix = `${process.pid}-${Math.random().toString(36).slice(2, 8)}`;
		return `virtual-portenta-${safeHost}-${suffix}`;
	}

	private clampInteger(value: unknown, min: number, max: number, fallback: number): number {
		const parsed = Number(value);
		if (!Number.isInteger(parsed)) {
			return fallback;
		}
		return Math.min(max, Math.max(min, parsed));
	}

	public async clearPersistedData(): Promise<void> {
		await rm(this.dataDir, { recursive: true, force: true });
		await mkdir(this.profilesDir, { recursive: true });
		await this.persistConnection();
		await this.persistState();
	}
}

const globalKey = "__gms_simulator_service__";
const globalStore = globalThis as typeof globalThis & {
	[globalKey]?: SimulatorService;
};

export const simulator = globalStore[globalKey] ?? new SimulatorService();

if (!globalStore[globalKey]) {
	globalStore[globalKey] = simulator;
}
