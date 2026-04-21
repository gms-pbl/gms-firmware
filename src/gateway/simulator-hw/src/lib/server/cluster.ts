import { randomUUID } from "node:crypto";
import { mkdir, readFile, rm, writeFile } from "node:fs/promises";
import path from "node:path";
import {
	DEFAULT_BROKER,
	type BrokerConfig,
	type ClusterRuntime,
	type GatewayConfig,
	type GatewaySimulatorRuntime,
	type GatewaySummary
} from "$lib/sim/types";
import { DockerOrchestrator } from "$lib/server/docker-orchestrator";
import { SimulatorService } from "$lib/server/simulator";

interface PersistedClusterState {
	active_gateway_id?: string | null;
	gateways?: GatewayConfig[];
}

interface GatewayMutationPayload {
	gateway_id?: string;
	label?: string;
	tenant_id?: string;
	greenhouse_id?: string;
	host_mqtt_port?: number | string | null;
	auto_start?: boolean;
	start_immediately?: boolean;
	cloud_broker?: {
		host?: string;
		port?: number | string;
		username?: string;
		password?: string;
	};
	clear_cloud_password?: boolean;
}

const CLUSTER_EVENT_LIMIT = 240;
const DEFAULT_GATEWAY_HOST_MQTT_PORT_BASE = 18831;

class SimulatorClusterService {
	private readonly orchestrator = new DockerOrchestrator();
	private readonly dataDir = process.env.SIM_DATA_DIR ?? path.resolve(process.cwd(), ".sim-data");
	private readonly gatewaysDir = path.join(this.dataDir, "gateways");
	private readonly clusterPath = path.join(this.dataDir, "gateways.json");

	private gateways = new Map<string, GatewayConfig>();
	private simulators = new Map<string, SimulatorService>();
	private activeGatewayId: string | null = null;
	private events: string[] = [];
	private initialized = false;
	private initPromise: Promise<void> | null = null;

	public async init(): Promise<void> {
		if (this.initialized) {
			return;
		}
		if (this.initPromise) {
			await this.initPromise;
			return;
		}

		this.initPromise = this.initializeInternal();
		try {
			await this.initPromise;
		} finally {
			this.initPromise = null;
		}
	}

	public async getClusterRuntime(): Promise<ClusterRuntime> {
		await this.init();

		const summaries = await this.buildGatewaySummaries();
		return {
			gateways: summaries,
			active_gateway_id: this.activeGatewayId,
			events: [...this.events]
		};
	}

	public async getGatewayRuntime(gatewayIdCandidate?: string | null): Promise<GatewaySimulatorRuntime> {
		await this.init();
		const gatewayId = await this.resolveGatewayId(gatewayIdCandidate ?? undefined, true);
		const gateway = this.requireGateway(gatewayId);

		const inspection = await this.orchestrator.inspectGateway(gatewayId);
		const simulator = await this.ensureSimulator(gateway);
		await this.syncSimulatorBroker(simulator, gateway, inspection.broker);

		const profiles = await simulator.listProfiles();
		const runtime = simulator.getRuntime(profiles);
		const summary = this.toGatewaySummary(gateway, inspection, runtime.instances.length, runtime.instances);

		return {
			...runtime,
			gateway: summary
		};
	}

	public async getGatewaySimulator(
		gatewayIdCandidate?: string | null,
		persistSelection = true
	): Promise<{ gatewayId: string; simulator: SimulatorService }> {
		await this.init();
		const gatewayId = await this.resolveGatewayId(gatewayIdCandidate ?? undefined, persistSelection);
		const gateway = this.requireGateway(gatewayId);
		const simulator = await this.ensureSimulator(gateway);
		return { gatewayId, simulator };
	}

	public async addGateway(payload: GatewayMutationPayload): Promise<string> {
		await this.init();

		const prepared = this.prepareGatewayPayload(payload);
		const gatewayIdSeed =
			typeof payload.gateway_id === "string" && payload.gateway_id.trim()
				? payload.gateway_id
				: typeof payload.greenhouse_id === "string" && payload.greenhouse_id.trim()
					? payload.greenhouse_id
					: typeof payload.label === "string" && payload.label.trim()
						? payload.label
						: "gateway";
		const gatewayIdBase = this.toGatewayId(gatewayIdSeed);
		const gatewayId = this.makeUniqueGatewayId(gatewayIdBase);

		const next: GatewayConfig = {
			gateway_id: gatewayId,
			label: prepared.label,
			tenant_id: prepared.tenant_id,
			greenhouse_id: prepared.greenhouse_id,
			host_mqtt_port: prepared.host_mqtt_port,
			auto_start: prepared.auto_start,
			cloud_broker: prepared.cloud_broker
		};

		this.gateways.set(gatewayId, next);
		this.activeGatewayId = gatewayId;
		await this.persistClusterState();

		await this.ensureSimulator(next);
		this.log(`Gateway '${gatewayId}' added.`);

		if (prepared.start_immediately || next.auto_start) {
			await this.startGateway(gatewayId);
		}

		return gatewayId;
	}

	public async updateGateway(gatewayId: string, payload: GatewayMutationPayload): Promise<void> {
		await this.init();
		const existing = this.requireGateway(gatewayId);
		const prepared = this.prepareGatewayPayload(payload, existing);
		const inspectionBefore = await this.orchestrator.inspectGateway(gatewayId);
		const wasRunning = inspectionBefore.broker.running || inspectionBefore.edge.running;

		const next: GatewayConfig = {
			...existing,
			label: prepared.label,
			tenant_id: prepared.tenant_id,
			greenhouse_id: prepared.greenhouse_id,
			host_mqtt_port: prepared.host_mqtt_port,
			auto_start: prepared.auto_start,
			cloud_broker: prepared.cloud_broker
		};

		this.gateways.set(gatewayId, next);
		await this.persistClusterState();

		if (wasRunning) {
			await this.orchestrator.recreateGateway(next);
		}

		const simulator = await this.ensureSimulator(next);
		const inspectionAfter = await this.orchestrator.inspectGateway(gatewayId);
		await this.syncSimulatorBroker(simulator, next, inspectionAfter.broker);

		this.log(`Gateway '${gatewayId}' updated.`);
	}

	public async startGateway(gatewayId: string): Promise<void> {
		await this.init();
		const gateway = this.requireGateway(gatewayId);
		await this.orchestrator.ensureGatewayStarted(gateway);
		let inspection = await this.orchestrator.inspectGateway(gatewayId);

		if (this.shouldRecreateGatewayForHostPort(gateway, inspection)) {
			await this.orchestrator.recreateGateway(gateway);
			inspection = await this.orchestrator.inspectGateway(gatewayId);
		}

		const simulator = await this.ensureSimulator(gateway);
		await this.syncSimulatorBroker(simulator, gateway, inspection.broker);

		this.log(`Gateway '${gatewayId}' started.`);
	}

	public async stopGateway(gatewayId: string): Promise<void> {
		await this.init();
		const gateway = this.requireGateway(gatewayId);
		const simulator = await this.ensureSimulator(gateway);

		await this.syncSimulatorBroker(simulator, gateway, {
			running: false,
			network_ip: null
		});
		await simulator.disconnect(false);

		await this.orchestrator.stopGateway(gatewayId);

		this.log(`Gateway '${gatewayId}' stopped.`);
	}

	public async removeGateway(gatewayId: string): Promise<void> {
		await this.init();
		this.requireGateway(gatewayId);

		await this.orchestrator.removeGateway(gatewayId, true);

		const simulator = this.simulators.get(gatewayId);
		if (simulator) {
			await simulator.disconnect(false);
			await simulator.clearPersistedData();
			this.simulators.delete(gatewayId);
		}

		await rm(path.join(this.gatewaysDir, gatewayId), { recursive: true, force: true });
		this.gateways.delete(gatewayId);

		if (this.activeGatewayId === gatewayId) {
			this.activeGatewayId = this.gateways.keys().next().value ?? null;
		}

		await this.persistClusterState();
		this.log(`Gateway '${gatewayId}' removed.`);
	}

	public async selectGateway(gatewayId: string): Promise<void> {
		await this.resolveGatewayId(gatewayId, true);
	}

	private async initializeInternal(): Promise<void> {
		await mkdir(this.gatewaysDir, { recursive: true });
		await this.loadPersistedClusterState();

		if (this.gateways.size === 0) {
			const fallback = this.defaultGatewayConfig();
			this.gateways.set(fallback.gateway_id, fallback);
			this.activeGatewayId = fallback.gateway_id;
			this.log(`Created default gateway '${fallback.gateway_id}'.`);
			await this.persistClusterState();
		}

		for (const gateway of this.gateways.values()) {
			const simulator = await this.ensureSimulator(gateway);
			let inspection = await this.orchestrator.inspectGateway(gateway.gateway_id);

			if (this.shouldRecreateGatewayForHostPort(gateway, inspection) && (inspection.broker.running || inspection.edge.running)) {
				await this.orchestrator.recreateGateway(gateway);
				inspection = await this.orchestrator.inspectGateway(gateway.gateway_id);
				this.log(`Gateway '${gateway.gateway_id}' recreated to apply host MQTT port ${gateway.host_mqtt_port}.`);
			}

			await this.syncSimulatorBroker(simulator, gateway, inspection.broker);
		}

		this.initialized = true;
		this.log("Cluster service initialized.");
	}

	private async buildGatewaySummaries(): Promise<GatewaySummary[]> {
		const summaries: GatewaySummary[] = [];

		for (const gateway of this.sortedGateways()) {
			const inspection = await this.orchestrator.inspectGateway(gateway.gateway_id);
			const simulator = await this.ensureSimulator(gateway);
			await this.syncSimulatorBroker(simulator, gateway, inspection.broker);

			const profiles = await simulator.listProfiles();
			const runtime = simulator.getRuntime(profiles);
			summaries.push(this.toGatewaySummary(gateway, inspection, runtime.instances.length, runtime.instances));
		}

		return summaries;
	}

	private async ensureSimulator(gateway: GatewayConfig): Promise<SimulatorService> {
		const existing = this.simulators.get(gateway.gateway_id);
		if (existing) {
			return existing;
		}

		const simulator = new SimulatorService({
			dataDir: path.join(this.gatewaysDir, gateway.gateway_id),
			defaults: {
				...DEFAULT_BROKER,
				host: this.orchestrator.getBrokerContainerHost(gateway.gateway_id),
				port: 1883,
				greenhouse_id: gateway.greenhouse_id,
				auto_connect: false
			}
		});
		await simulator.init();
		this.simulators.set(gateway.gateway_id, simulator);
		return simulator;
	}

	private async syncSimulatorBroker(
		simulator: SimulatorService,
		gateway: GatewayConfig,
		broker: Pick<GatewaySummary["broker"], "running" | "network_ip">
	): Promise<void> {
		const current = simulator.getBrokerConfig();
		const desiredHost = this.resolveBrokerHost(gateway.gateway_id, broker);

		const patch: Partial<BrokerConfig> = {};
		if (current.host !== desiredHost) {
			patch.host = desiredHost;
		}
		if (current.port !== 1883) {
			patch.port = 1883;
		}
		if (current.greenhouse_id !== gateway.greenhouse_id) {
			patch.greenhouse_id = gateway.greenhouse_id;
		}
		if (current.auto_connect !== broker.running) {
			patch.auto_connect = broker.running;
		}

		if (Object.keys(patch).length > 0) {
			await simulator.setBroker(patch);
		}
	}

	private resolveBrokerHost(
		gatewayId: string,
		broker: Pick<GatewaySummary["broker"], "running" | "network_ip">
	): string {
		if (broker.running) {
			const networkIp = String(broker.network_ip ?? "").trim();
			if (networkIp) {
				return networkIp;
			}
		}

		return this.orchestrator.getBrokerContainerHost(gatewayId);
	}

	private toGatewaySummary(
		gateway: GatewayConfig,
		inspection: { broker: GatewaySummary["broker"]; edge: GatewaySummary["edge"] },
		totalInstances: number,
		instances: Array<{ status: { connected: boolean } }>
	): GatewaySummary {
		const connected = instances.filter((instance) => instance.status.connected).length;

		return {
			config: this.toPublicGatewayConfig(gateway),
			broker: inspection.broker,
			edge: inspection.edge,
			simulator_total_instances: totalInstances,
			simulator_connected_instances: connected
		};
	}

	private toPublicGatewayConfig(gateway: GatewayConfig): GatewayConfig {
		return {
			...gateway,
			cloud_broker: {
				...gateway.cloud_broker,
				password: gateway.cloud_broker.password ? "********" : ""
			}
		};
	}

	private shouldRecreateGatewayForHostPort(
		gateway: GatewayConfig,
		inspection: { broker: Pick<GatewaySummary["broker"], "exists" | "host_port"> }
	): boolean {
		if (!inspection.broker.exists) {
			return false;
		}

		return gateway.host_mqtt_port !== inspection.broker.host_port;
	}

	private sortedGateways(): GatewayConfig[] {
		return [...this.gateways.values()].sort((a, b) => a.label.localeCompare(b.label));
	}

	private requireGateway(gatewayId: string): GatewayConfig {
		const gateway = this.gateways.get(gatewayId);
		if (!gateway) {
			throw new Error(`Gateway '${gatewayId}' was not found.`);
		}
		return gateway;
	}

	private async resolveGatewayId(candidate: string | undefined, persistSelection: boolean): Promise<string> {
		await this.init();

		if (candidate) {
			const normalized = this.toGatewayId(candidate);
			this.requireGateway(normalized);
			if (persistSelection && this.activeGatewayId !== normalized) {
				this.activeGatewayId = normalized;
				await this.persistClusterState();
			}
			return normalized;
		}

		if (this.activeGatewayId && this.gateways.has(this.activeGatewayId)) {
			return this.activeGatewayId;
		}

		const firstGateway = this.gateways.keys().next().value;
		if (!firstGateway) {
			throw new Error("No gateway instances configured.");
		}

		if (persistSelection) {
			this.activeGatewayId = firstGateway;
			await this.persistClusterState();
		}

		return firstGateway;
	}

	private async loadPersistedClusterState(): Promise<void> {
		try {
			const data = await readFile(this.clusterPath, "utf8");
			const parsed = JSON.parse(data) as PersistedClusterState;
			let shouldPersist = false;

			this.gateways.clear();
			for (const item of parsed.gateways ?? []) {
				const payload = item as GatewayMutationPayload;
				const persistedHostPort = this.normalizeOptionalPort(payload.host_mqtt_port, null);
				const normalized = this.prepareGatewayPayload(payload, undefined, {
					conflictPolicy: "reassign"
				});
				const gatewayId = this.toGatewayId(item.gateway_id || normalized.gateway_id || randomUUID());
				if (normalized.host_mqtt_port !== persistedHostPort) {
					shouldPersist = true;
				}

				this.gateways.set(gatewayId, {
					gateway_id: gatewayId,
					label: normalized.label,
					tenant_id: normalized.tenant_id,
					greenhouse_id: normalized.greenhouse_id,
					host_mqtt_port: normalized.host_mqtt_port,
					auto_start: normalized.auto_start,
					cloud_broker: normalized.cloud_broker
				});
			}

			const persistedActive = parsed.active_gateway_id
				? this.toGatewayId(parsed.active_gateway_id)
				: null;
			this.activeGatewayId =
				persistedActive && this.gateways.has(persistedActive)
					? persistedActive
					: this.gateways.keys().next().value ?? null;

			if (shouldPersist) {
				await this.persistClusterState();
			}
		} catch {
			await this.persistClusterState();
		}
	}

	private async persistClusterState(): Promise<void> {
		const payload: PersistedClusterState = {
			active_gateway_id: this.activeGatewayId,
			gateways: this.sortedGateways()
		};

		await writeFile(this.clusterPath, JSON.stringify(payload, null, 2), "utf8");
	}

	private prepareGatewayPayload(
		payload: GatewayMutationPayload,
		fallback?: GatewayConfig,
		options?: { conflictPolicy?: "throw" | "reassign" }
	): {
		gateway_id: string;
		label: string;
		tenant_id: string;
		greenhouse_id: string;
		host_mqtt_port: number | null;
		auto_start: boolean;
		start_immediately: boolean;
		cloud_broker: GatewayConfig["cloud_broker"];
	} {
		const gatewayId = this.toGatewayId(payload.gateway_id || fallback?.gateway_id || randomUUID());
		const label = this.pickText(payload.label, fallback?.label, `Gateway ${gatewayId}`);
		const tenantId = this.pickText(payload.tenant_id, fallback?.tenant_id, "tenant-demo");
		const greenhouseId = this.pickText(payload.greenhouse_id, fallback?.greenhouse_id, gatewayId);

		const requestedHostPort = this.normalizeOptionalPort(payload.host_mqtt_port, fallback?.host_mqtt_port ?? null);
		const hostPort = this.resolveHostMqttPort(
			requestedHostPort,
			gatewayId,
			fallback?.gateway_id,
			options?.conflictPolicy ?? "throw"
		);
		const autoStart = typeof payload.auto_start === "boolean" ? payload.auto_start : (fallback?.auto_start ?? false);
		const startImmediately = Boolean(payload.start_immediately);

		const cloudHost = this.pickText(
			payload.cloud_broker?.host,
			fallback?.cloud_broker.host,
			process.env.CLOUD_BROKER_HOST ?? "host.docker.internal"
		);
		const cloudPort = this.normalizePort(
			payload.cloud_broker?.port,
			fallback?.cloud_broker.port ?? Number(process.env.CLOUD_BROKER_PORT ?? 1883)
		);
		const cloudUsername = this.pickOptionalText(payload.cloud_broker?.username, fallback?.cloud_broker.username);

		let cloudPassword = fallback?.cloud_broker.password;
		if (payload.clear_cloud_password) {
			cloudPassword = undefined;
		} else if (payload.cloud_broker && "password" in payload.cloud_broker) {
			const nextPassword = String(payload.cloud_broker.password ?? "").trim();
			if (nextPassword && nextPassword !== "********") {
				cloudPassword = nextPassword;
			}
		}

		return {
			gateway_id: gatewayId,
			label,
			tenant_id: tenantId,
			greenhouse_id: greenhouseId,
			host_mqtt_port: hostPort,
			auto_start: autoStart,
			start_immediately: startImmediately,
			cloud_broker: {
				host: cloudHost,
				port: cloudPort,
				username: cloudUsername,
				password: cloudPassword
			}
		};
	}

	private defaultGatewayConfig(): GatewayConfig {
		const seedId = this.toGatewayId(process.env.GATEWAY_ID ?? process.env.GREENHOUSE_ID ?? "greenhouse-demo");
		const requestedHostPort = this.normalizeOptionalPort(process.env.DEFAULT_GATEWAY_HOST_MQTT_PORT, null);

		return {
			gateway_id: seedId,
			label: this.pickText(process.env.GATEWAY_LABEL, undefined, `Gateway ${seedId}`),
			tenant_id: this.pickText(process.env.TENANT_ID, undefined, "tenant-demo"),
			greenhouse_id: this.pickText(process.env.GREENHOUSE_ID, undefined, seedId),
			host_mqtt_port: this.resolveHostMqttPort(requestedHostPort, seedId, undefined, "reassign"),
			auto_start: false,
			cloud_broker: {
				host: this.pickText(process.env.CLOUD_BROKER_HOST, undefined, "host.docker.internal"),
				port: this.normalizePort(process.env.CLOUD_BROKER_PORT, 1883),
				username: this.pickOptionalText(process.env.CLOUD_USERNAME, undefined),
				password: this.pickOptionalText(process.env.CLOUD_PASSWORD, undefined)
			}
		};
	}

	private pickText(value: unknown, fallback: string | undefined, defaultValue: string): string {
		const candidate = String(value ?? fallback ?? defaultValue).trim();
		return candidate || defaultValue;
	}

	private pickOptionalText(value: unknown, fallback: string | undefined): string | undefined {
		const raw = value ?? fallback;
		if (raw === undefined || raw === null) {
			return undefined;
		}

		const trimmed = String(raw).trim();
		return trimmed || undefined;
	}

	private normalizePort(value: unknown, fallback: number): number {
		const parsed = Number(value);
		if (!Number.isInteger(parsed)) {
			return fallback;
		}
		return Math.min(65535, Math.max(1, parsed));
	}

	private normalizeOptionalPort(value: unknown, fallback: number | null): number | null {
		if (value === null || value === undefined || value === "") {
			return fallback;
		}

		const parsed = Number(value);
		if (!Number.isInteger(parsed)) {
			return fallback;
		}
		return Math.min(65535, Math.max(1, parsed));
	}

	private resolveHostMqttPort(
		requestedPort: number | null,
		gatewayId: string,
		excludeGatewayId?: string,
		conflictPolicy: "throw" | "reassign" = "throw"
	): number {
		if (requestedPort !== null) {
			if (!this.isHostMqttPortUsed(requestedPort, excludeGatewayId ?? gatewayId)) {
				return requestedPort;
			}

			if (conflictPolicy === "throw") {
				throw new Error(`Host MQTT port ${requestedPort} is already used by another gateway.`);
			}
		}

		return this.allocateUniqueHostMqttPort(excludeGatewayId ?? gatewayId);
	}

	private allocateUniqueHostMqttPort(excludeGatewayId?: string): number {
		const base = this.normalizePort(process.env.GATEWAY_HOST_MQTT_PORT_BASE, DEFAULT_GATEWAY_HOST_MQTT_PORT_BASE);
		for (let port = base; port <= 65535; port += 1) {
			if (!this.isHostMqttPortUsed(port, excludeGatewayId)) {
				return port;
			}
		}

		throw new Error("Unable to allocate unique host MQTT port: no free ports available.");
	}

	private isHostMqttPortUsed(port: number, excludeGatewayId?: string): boolean {
		for (const gateway of this.gateways.values()) {
			if (excludeGatewayId && gateway.gateway_id === excludeGatewayId) {
				continue;
			}
			if (gateway.host_mqtt_port === port) {
				return true;
			}
		}

		return false;
	}

	private toGatewayId(value: string): string {
		const cleaned = value
			.toLowerCase()
			.replace(/[^a-z0-9-]/g, "-")
			.replace(/-+/g, "-")
			.replace(/^-|-$/g, "")
			.slice(0, 48);

		return cleaned || `gateway-${randomUUID().slice(0, 8)}`;
	}

	private makeUniqueGatewayId(baseId: string): string {
		if (!this.gateways.has(baseId)) {
			return baseId;
		}

		let index = 2;
		while (this.gateways.has(`${baseId}-${index}`)) {
			index += 1;
		}
		return `${baseId}-${index}`;
	}

	private log(message: string): void {
		const line = `${new Date().toISOString()} ${message}`;
		this.events.unshift(line);
		if (this.events.length > CLUSTER_EVENT_LIMIT) {
			this.events.length = CLUSTER_EVENT_LIMIT;
		}
	}
}

const globalKey = "__gms_simulator_cluster_service__";
const globalStore = globalThis as typeof globalThis & {
	[globalKey]?: SimulatorClusterService;
};

export const cluster = globalStore[globalKey] ?? new SimulatorClusterService();

if (!globalStore[globalKey]) {
	globalStore[globalKey] = cluster;
}
