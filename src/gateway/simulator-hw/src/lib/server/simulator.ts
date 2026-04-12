import { randomUUID } from "node:crypto";
import { mkdir, readdir, readFile, rm, unlink, writeFile } from "node:fs/promises";
import path from "node:path";
import mqtt, { type MqttClient } from "mqtt";
import {
	DEFAULT_BROKER,
	DEFAULT_STATE,
	type BinaryValue,
	type BrokerConfig,
	type ConnectionStatus,
	type DigitalInputState,
	type DigitalOutputState,
	type SimulationState,
	type SimulatorInstanceSummary,
	type SimulatorRuntime
} from "$lib/sim/types";

type StatePatch = Partial<{
	sensors: Partial<SimulationState["sensors"]>;
	digital_inputs: Partial<DigitalInputState>;
	digital_outputs: Partial<DigitalOutputState>;
}>;

interface VirtualInstance {
	device_id: string;
	label: string;
	zone_id: string | null;
	zone_name: string | null;
	state: SimulationState;
	status: ConnectionStatus;
	client: MqttClient | null;
}

interface PersistedInstance {
	device_id: string;
	label: string;
	zone_id?: string | null;
	zone_name?: string | null;
	state?: Partial<SimulationState>;
}

interface PersistedInstancesFile {
	active_device_id?: string | null;
	instances?: PersistedInstance[];
}

const EVENT_LIMIT = 160;

class SimulatorService {
	private broker: BrokerConfig = this.readBrokerDefaults();
	private instances = new Map<string, VirtualInstance>();
	private activeDeviceId: string | null = null;
	private events: string[] = [];
	private initialized = false;
	private publishTimer: ReturnType<typeof setInterval> | null = null;

	private readonly dataDir = process.env.SIM_DATA_DIR ?? path.resolve(process.cwd(), "sim-data");
	private readonly profilesDir = path.join(this.dataDir, "profiles");
	private readonly brokerPath = path.join(this.dataDir, "connection.json");
	private readonly instancesPath = path.join(this.dataDir, "instances.json");

	public async init(): Promise<void> {
		if (this.initialized) {
			return;
		}

		await mkdir(this.profilesDir, { recursive: true });
		await this.loadPersistedBroker();
		await this.loadPersistedInstances();
		this.ensureAtLeastOneInstance();

		this.initialized = true;
		this.log("Simulator initialized.");

		if (this.broker.auto_connect) {
			await this.connect();
		} else {
			this.refreshPublishTimer();
		}
	}

	public getRuntime(profiles: string[]): SimulatorRuntime {
		const active = this.getActiveInstance();
		const instanceSummaries: SimulatorInstanceSummary[] = [...this.instances.values()]
			.sort((a, b) => a.label.localeCompare(b.label))
			.map((instance) => ({
				device_id: instance.device_id,
				label: instance.label,
				zone_id: instance.zone_id,
				zone_name: instance.zone_name,
				status: { ...instance.status }
			}));

		return {
			state: active ? structuredClone(active.state) : null,
			broker: {
				...this.broker,
				password: this.broker.password ? "********" : ""
			},
			active_device_id: this.activeDeviceId,
			instances: instanceSummaries,
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

	public async selectInstance(deviceId: string): Promise<void> {
		if (!this.instances.has(deviceId)) {
			throw new Error(`Instance '${deviceId}' was not found.`);
		}
		this.activeDeviceId = deviceId;
		await this.persistInstances();
		this.log(`Selected instance '${deviceId}'.`);
	}

	public async addInstance(label?: string): Promise<string> {
		const deviceId = randomUUID();
		const next: VirtualInstance = {
			device_id: deviceId,
			label: this.makeLabel(label),
			zone_id: null,
			zone_name: null,
			state: structuredClone(DEFAULT_STATE),
			status: this.disconnectedStatus(),
			client: null
		};

		this.instances.set(deviceId, next);
		this.activeDeviceId = deviceId;
		await this.persistInstances();
		this.log(`Added virtual Portenta '${next.label}' (${deviceId}).`);

		if (this.broker.auto_connect) {
			await this.connectInstance(next);
		}

		this.refreshPublishTimer();
		return deviceId;
	}

	public async removeInstance(deviceId: string): Promise<void> {
		const instance = this.instances.get(deviceId);
		if (!instance) {
			throw new Error(`Instance '${deviceId}' was not found.`);
		}

		await this.publishRemoval(instance);
		await this.disconnectInstance(instance, false);
		this.instances.delete(deviceId);

		if (this.activeDeviceId === deviceId) {
			const [first] = this.instances.keys();
			this.activeDeviceId = first ?? null;
		}

		this.ensureAtLeastOneInstance();
		await this.persistInstances();
		this.log(`Removed virtual Portenta '${deviceId}'.`);
		this.refreshPublishTimer();
	}

	public async updateState(patch: StatePatch): Promise<void> {
		const active = this.getRequiredActiveInstance();

		if (patch.sensors) {
			for (const [key, value] of Object.entries(patch.sensors)) {
				if (key in active.state.sensors && typeof value === "number" && Number.isFinite(value)) {
					active.state.sensors[key as keyof SimulationState["sensors"]] = value;
				}
			}
		}

		if (patch.digital_inputs) {
			this.applyBinaryPatch(active.state.digital_inputs, patch.digital_inputs);
		}

		if (patch.digital_outputs) {
			this.applyBinaryPatch(active.state.digital_outputs, patch.digital_outputs);
		}

		await this.persistInstances();
		this.log(`State updated for ${active.device_id}.`);
	}

	public async setBroker(patch: Partial<BrokerConfig>): Promise<void> {
		const next = {
			...this.broker,
			...patch
		};

		next.host = String(next.host || DEFAULT_BROKER.host).trim();
		next.greenhouse_id = String(next.greenhouse_id || DEFAULT_BROKER.greenhouse_id).trim();
		next.port = this.clampInteger(next.port, 1, 65535, DEFAULT_BROKER.port);
		next.publish_interval_ms = this.clampInteger(next.publish_interval_ms, 200, 3_600_000, DEFAULT_BROKER.publish_interval_ms);
		next.auto_connect = Boolean(next.auto_connect);

		const shouldReconnect = this.hasConnectedInstances();
		this.broker = next;
		await this.persistBroker();
		this.log("Broker settings updated.");

		if (shouldReconnect || this.broker.auto_connect) {
			await this.connect();
		} else {
			this.refreshPublishTimer();
		}
	}

	public async connect(): Promise<void> {
		for (const instance of this.instances.values()) {
			await this.connectInstance(instance);
		}
		this.refreshPublishTimer();
	}

	public async disconnect(logMessage = true): Promise<void> {
		for (const instance of this.instances.values()) {
			await this.disconnectInstance(instance, false);
		}
		if (logMessage) {
			this.log("Disconnected all instances by user.");
		}
		this.refreshPublishTimer();
	}

	public async saveProfile(name: string): Promise<void> {
		const active = this.getRequiredActiveInstance();
		const safeName = this.sanitizeProfileName(name);
		const targetPath = path.join(this.profilesDir, `${safeName}.json`);
		await writeFile(targetPath, JSON.stringify(active.state, null, 2), "utf8");
		this.log(`Saved profile '${safeName}' for ${active.device_id}.`);
	}

	public async loadProfile(name: string): Promise<void> {
		const active = this.getRequiredActiveInstance();
		const safeName = this.sanitizeProfileName(name);
		const targetPath = path.join(this.profilesDir, `${safeName}.json`);
		const data = await readFile(targetPath, "utf8");
		const parsed = JSON.parse(data) as SimulationState;

		active.state = this.mergeState(DEFAULT_STATE, parsed);
		await this.persistInstances();
		this.log(`Loaded profile '${safeName}' into ${active.device_id}.`);
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
		const active = this.getRequiredActiveInstance();
		active.state = structuredClone(DEFAULT_STATE);
		await this.persistInstances();
		this.log(`State reset for ${active.device_id}.`);
	}

	private async connectInstance(instance: VirtualInstance): Promise<void> {
		await this.disconnectInstance(instance, false);

		const url = `mqtt://${this.broker.host}:${this.broker.port}`;
		const clientId = `virtual-portenta-${instance.device_id.slice(0, 8)}`;
		instance.client = mqtt.connect(url, {
			clientId,
			username: this.broker.username || undefined,
			password: this.broker.password || undefined,
			reconnectPeriod: 3000,
			connectTimeout: 5000,
			clean: true
		});

		instance.client.on("connect", () => {
			instance.status.connected = true;
			instance.status.last_error = null;
			instance.status.last_connect_at = new Date().toISOString();
			this.log(`MQTT connected (${instance.device_id}).`);

			if (!instance.client) {
				return;
			}

			const commandTopic = this.commandTopic(instance.device_id);
			const configTopic = this.configTopic(instance.device_id);

			instance.client.subscribe(commandTopic, (error) => {
				if (error) {
					this.log(`Subscribe failed (${instance.device_id}): ${error.message}`);
					return;
				}
				this.log(`Subscribed ${instance.device_id} -> ${commandTopic}`);
			});

			instance.client.subscribe(configTopic, (error) => {
				if (error) {
					this.log(`Subscribe failed (${instance.device_id}): ${error.message}`);
					return;
				}
				this.log(`Subscribed ${instance.device_id} -> ${configTopic}`);
			});

			void this.publishAnnounce(instance);
			void this.publishTelemetry(instance);
			this.refreshPublishTimer();
		});

		instance.client.on("message", async (topic, payload) => {
			const text = payload.toString();

			if (topic === this.commandTopic(instance.device_id)) {
				try {
					const body = JSON.parse(text) as { channel?: number; state?: number };
					if (typeof body.channel !== "number" || !Number.isInteger(body.channel) || body.channel < 0 || body.channel > 7) {
						throw new Error("Invalid channel");
					}

					const key = this.outputKeyFromChannel(instance, body.channel);
					if (!key) {
						throw new Error("Unknown output channel");
					}

					instance.state.digital_outputs[key] = this.normalizeBinary(body.state);
					await this.persistInstances();
					this.log(`Applied output ${instance.device_id}: ${key}=${instance.state.digital_outputs[key]}`);
				} catch (error) {
					this.log(`Command parse error (${instance.device_id}): ${error instanceof Error ? error.message : String(error)}`);
				}
				return;
			}

			if (topic === this.configTopic(instance.device_id)) {
				try {
					const body = JSON.parse(text) as { zone_id?: string; zone_name?: string };
					instance.zone_id = body.zone_id ? String(body.zone_id) : null;
					instance.zone_name = body.zone_name ? String(body.zone_name) : null;
					await this.persistInstances();
					this.log(`Applied config ${instance.device_id}: zone=${instance.zone_id || "unassigned"}`);
				} catch (error) {
					this.log(`Config parse error (${instance.device_id}): ${error instanceof Error ? error.message : String(error)}`);
				}
			}
		});

		instance.client.on("error", (error) => {
			instance.status.last_error = error.message;
			this.log(`MQTT error (${instance.device_id}): ${error.message}`);
		});

		instance.client.on("close", () => {
			instance.status.connected = false;
			this.log(`MQTT disconnected (${instance.device_id}).`);
			this.refreshPublishTimer();
		});
	}

	private async disconnectInstance(instance: VirtualInstance, logMessage = true): Promise<void> {
		if (instance.client) {
			await new Promise<void>((resolve) => {
				instance.client?.end(false, {}, () => resolve());
			});
			instance.client = null;
		}

		instance.status.connected = false;
		if (logMessage) {
			this.log(`Disconnected ${instance.device_id}.`);
		}
	}

	private refreshPublishTimer(): void {
		if (this.publishTimer) {
			clearInterval(this.publishTimer);
			this.publishTimer = null;
		}

		if (!this.hasConnectedInstances()) {
			return;
		}

		this.publishTimer = setInterval(() => {
			for (const instance of this.instances.values()) {
				void this.publishAnnounce(instance);
				void this.publishTelemetry(instance);
			}
		}, this.broker.publish_interval_ms);
	}

	private async publishTelemetry(instance: VirtualInstance): Promise<void> {
		if (!instance.client || !instance.status.connected) {
			return;
		}

		const payload = {
			...instance.state.sensors,
			...instance.state.digital_inputs,
			...instance.state.digital_outputs
		};

		instance.client.publish(this.telemetryTopic(instance.device_id), JSON.stringify(payload));
		this.log(`Telemetry ${instance.device_id} -> ${this.telemetryTopic(instance.device_id)}`);
	}

	private async publishAnnounce(instance: VirtualInstance): Promise<void> {
		if (!instance.client || !instance.status.connected) {
			return;
		}

		const metadata: Record<string, string> = {
			label: instance.label
		};

		if (instance.zone_id) {
			metadata.zone_id = instance.zone_id;
		}

		if (instance.zone_name) {
			metadata.zone_name = instance.zone_name;
		}

		const payload = {
			device_id: instance.device_id,
			firmware_version: "sim-v2",
			metadata
		};

		instance.client.publish(this.announceTopic(instance.device_id), JSON.stringify(payload));
		this.log(`Announce ${instance.device_id} -> ${this.announceTopic(instance.device_id)}`);
	}

	private async publishRemoval(instance: VirtualInstance): Promise<void> {
		if (!instance.client || !instance.status.connected) {
			return;
		}

		const payload = {
			type: "DEVICE_REMOVED",
			device_id: instance.device_id,
			firmware_version: "sim-v2",
			metadata: {
				label: instance.label
			}
		};

		await new Promise<void>((resolve) => {
			let settled = false;
			const finish = () => {
				if (!settled) {
					settled = true;
					resolve();
				}
			};

			instance.client?.publish(
				this.announceTopic(instance.device_id),
				JSON.stringify(payload),
				{ qos: 1 },
				(error) => {
					if (error) {
						this.log(`Removal announce publish failed (${instance.device_id}): ${error.message}`);
					}
					finish();
				}
			);

			setTimeout(finish, 1500);
		});

		this.log(`Removal announce ${instance.device_id} -> ${this.announceTopic(instance.device_id)}`);
	}

	private announceTopic(deviceId: string): string {
		return `edge/${this.broker.greenhouse_id}/zone/${deviceId}/registry/announce`;
	}

	private telemetryTopic(deviceId: string): string {
		return `edge/${this.broker.greenhouse_id}/zone/${deviceId}/telemetry/raw`;
	}

	private commandTopic(deviceId: string): string {
		return `edge/${this.broker.greenhouse_id}/zone/${deviceId}/command/output`;
	}

	private configTopic(deviceId: string): string {
		return `edge/${this.broker.greenhouse_id}/zone/${deviceId}/config`;
	}

	private hasConnectedInstances(): boolean {
		for (const instance of this.instances.values()) {
			if (instance.status.connected) {
				return true;
			}
		}
		return false;
	}

	private getActiveInstance(): VirtualInstance | null {
		if (!this.activeDeviceId) {
			return null;
		}
		return this.instances.get(this.activeDeviceId) ?? null;
	}

	private getRequiredActiveInstance(): VirtualInstance {
		const instance = this.getActiveInstance();
		if (!instance) {
			throw new Error("No active simulator instance is selected.");
		}
		return instance;
	}

	private ensureAtLeastOneInstance(): void {
		if (this.instances.size > 0) {
			if (!this.activeDeviceId || !this.instances.has(this.activeDeviceId)) {
				const [first] = this.instances.keys();
				this.activeDeviceId = first ?? null;
			}
			return;
		}

		const deviceId = randomUUID();
		this.instances.set(deviceId, {
			device_id: deviceId,
			label: "Virtual Portenta 1",
			zone_id: null,
			zone_name: null,
			state: structuredClone(DEFAULT_STATE),
			status: this.disconnectedStatus(),
			client: null
		});
		this.activeDeviceId = deviceId;
	}

	private disconnectedStatus(): ConnectionStatus {
		return {
			connected: false,
			last_error: null,
			last_connect_at: null
		};
	}

	private makeLabel(label?: string): string {
		const trimmed = label?.trim();
		if (trimmed) {
			return trimmed;
		}

		const nextIndex = this.instances.size + 1;
		return `Virtual Portenta ${nextIndex}`;
	}

	private async loadPersistedBroker(): Promise<void> {
		try {
			const data = await readFile(this.brokerPath, "utf8");
			const parsed = JSON.parse(data) as Partial<BrokerConfig> & {
				telemetry_topic?: string;
				command_topic?: string;
				client_id?: string;
			};

			this.broker = {
				...this.broker,
				...parsed,
				greenhouse_id: parsed.greenhouse_id ?? this.broker.greenhouse_id
			};
			await this.persistBroker();
			this.log("Loaded persisted broker settings.");
		} catch {
			await this.persistBroker();
		}
	}

	private async loadPersistedInstances(): Promise<void> {
		try {
			const data = await readFile(this.instancesPath, "utf8");
			const parsed = JSON.parse(data) as PersistedInstancesFile;

			this.instances.clear();
			for (const item of parsed.instances ?? []) {
				const deviceId = String(item.device_id || "").trim();
				if (!deviceId) {
					continue;
				}

				this.instances.set(deviceId, {
					device_id: deviceId,
					label: item.label?.trim() || `Virtual Portenta ${this.instances.size + 1}`,
					zone_id: item.zone_id ?? null,
					zone_name: item.zone_name ?? null,
					state: this.mergeState(DEFAULT_STATE, item.state ?? {}),
					status: this.disconnectedStatus(),
					client: null
				});
			}

			this.activeDeviceId = parsed.active_device_id ?? null;
			this.log("Loaded persisted instances.");
		} catch {
			await this.persistInstances();
		}
	}

	private async persistBroker(): Promise<void> {
		await writeFile(this.brokerPath, JSON.stringify(this.broker, null, 2), "utf8");
	}

	private async persistInstances(): Promise<void> {
		const payload: PersistedInstancesFile = {
			active_device_id: this.activeDeviceId,
			instances: [...this.instances.values()].map((instance) => ({
				device_id: instance.device_id,
				label: instance.label,
				zone_id: instance.zone_id,
				zone_name: instance.zone_name,
				state: instance.state
			}))
		};

		await writeFile(this.instancesPath, JSON.stringify(payload, null, 2), "utf8");
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
				(target as unknown as Record<string, BinaryValue>)[key] = this.normalizeBinary(value);
			}
		}
	}

	private normalizeBinary(value: unknown): BinaryValue {
		return Number(value) >= 1 ? 1 : 0;
	}

	private outputKeyFromChannel(instance: VirtualInstance, channel: number): keyof DigitalOutputState | null {
		const key = `dout_${String(channel).padStart(2, "0")}` as keyof DigitalOutputState;
		return key in instance.state.digital_outputs ? key : null;
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

	private readBrokerDefaults(): BrokerConfig {
		return {
			...DEFAULT_BROKER,
			host: process.env.MQTT_HOST ?? DEFAULT_BROKER.host,
			port: this.clampInteger(process.env.MQTT_PORT, 1, 65535, DEFAULT_BROKER.port),
			greenhouse_id: process.env.GREENHOUSE_ID ?? DEFAULT_BROKER.greenhouse_id,
			publish_interval_ms: this.clampInteger(
				process.env.PUBLISH_INTERVAL_MS,
				200,
				3_600_000,
				DEFAULT_BROKER.publish_interval_ms
			),
			username: process.env.MQTT_USERNAME || undefined,
			password: process.env.MQTT_PASSWORD || undefined,
			auto_connect: (process.env.AUTO_CONNECT ?? "true").toLowerCase() !== "false"
		};
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
		await this.persistBroker();
		await this.persistInstances();
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
