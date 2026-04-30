import { execFile } from "node:child_process";
import { promisify } from "node:util";
import type { GatewayConfig, GatewayContainerStatus, GatewayThresholdDebugStatus } from "$lib/sim/types";

export interface GatewayContainerInspection {
	broker: GatewayContainerStatus;
	edge: GatewayContainerStatus;
}

interface GatewayContainerNames {
	broker: string;
	edge: string;
	brokerVolume: string;
	edgeVolume: string;
}

interface DockerInspectContainer {
	Id: string;
	Image?: string;
	State?: {
		Running?: boolean;
		Status?: string;
	};
	NetworkSettings?: {
		Ports?: Record<string, Array<{ HostIp?: string; HostPort?: string }> | null>;
		Networks?: Record<string, { IPAddress?: string }>;
	};
}

const BROKER_PORT_KEY = "1883/tcp";
const execFileAsync = promisify(execFile);

export class DockerOrchestrator {
	private readonly networkName: string;
	private readonly edgeImage: string;
	private readonly brokerImage: string;

	public constructor() {
		this.networkName = process.env.SIM_CLUSTER_NETWORK ?? "gms-sim-cluster";
		this.edgeImage = process.env.SIM_EDGE_IMAGE ?? "gms-edge-engine:cluster";
		this.brokerImage = process.env.SIM_BROKER_IMAGE ?? "eclipse-mosquitto:2.0";
	}

	public getBrokerContainerHost(gatewayId: string): string {
		return this.names(gatewayId).broker;
	}

	public async ensureGatewayStarted(config: GatewayConfig): Promise<void> {
		await this.ensureNetwork();
		await this.ensureBrokerImage();
		await this.ensureEdgeImage();
		await this.ensureBrokerContainer(config);
		await this.ensureEdgeContainer(config);
		await this.startContainer(this.names(config.gateway_id).broker);
		await this.startContainer(this.names(config.gateway_id).edge);
	}

	public async reconcileRunningGateway(config: GatewayConfig): Promise<boolean> {
		const names = this.names(config.gateway_id);
		const broker = await this.inspectContainer(names.broker, BROKER_PORT_KEY);
		const edge = await this.inspectContainer(names.edge);

		if (!broker.running && !edge.running) {
			return false;
		}

		await this.ensureNetwork();
		await this.ensureBrokerImage();
		await this.ensureEdgeImage();
		await this.ensureBrokerContainer(config);
		const edgeChanged = await this.ensureEdgeContainer(config);
		await this.startContainer(names.broker);
		await this.startContainer(names.edge);
		return edgeChanged;
	}

	public async recreateGateway(config: GatewayConfig): Promise<void> {
		await this.removeGateway(config.gateway_id, false);
		await this.ensureGatewayStarted(config);
	}

	public async stopGateway(gatewayId: string): Promise<void> {
		const names = this.names(gatewayId);
		await this.stopContainerIfRunning(names.edge);
		await this.stopContainerIfRunning(names.broker);
	}

	public async removeGateway(gatewayId: string, removeVolumes: boolean): Promise<void> {
		const names = this.names(gatewayId);
		await this.removeContainer(names.edge);
		await this.removeContainer(names.broker);

		if (!removeVolumes) {
			return;
		}

		await this.removeVolume(names.edgeVolume);
		await this.removeVolume(names.brokerVolume);
	}

	public async inspectGateway(gatewayId: string): Promise<GatewayContainerInspection> {
		const names = this.names(gatewayId);

		try {
			return {
				broker: await this.inspectContainer(names.broker, BROKER_PORT_KEY),
				edge: await this.inspectContainer(names.edge)
			};
		} catch (error) {
			if (!this.isDockerUnavailable(error)) {
				throw error;
			}

			const status = "docker_unavailable";
			return {
				broker: {
					exists: false,
					running: false,
					status,
					container_name: names.broker,
					container_id: null,
					image_id: null,
					host_port: null,
					network_ip: null
				},
				edge: {
					exists: false,
					running: false,
					status,
					container_name: names.edge,
					container_id: null,
					image_id: null,
					host_port: null,
					network_ip: null
				}
			};
		}
	}

	public async readThresholdDebug(gatewayId: string): Promise<GatewayThresholdDebugStatus> {
		const names = this.names(gatewayId);
		let events: string[] = [];

		try {
			const edge = await this.inspectContainer(names.edge);
			events = edge.exists ? await this.readThresholdLogLines(names.edge) : [];
			const unavailable = (reason: string): GatewayThresholdDebugStatus => ({
				available: false,
				reason,
				container_name: names.edge,
				configs: [],
				events
			});

			if (!edge.exists) {
				return unavailable("Edge container is missing.");
			}

			if (!edge.running) {
				return unavailable("Edge container is not running.");
			}

			const output = await this.runDocker([
				"exec",
				names.edge,
				"python",
				"-c",
				this.thresholdDebugScript()
			]);

			const result = JSON.parse(output) as
				| GatewayThresholdDebugStatus["configs"]
				| { available?: boolean; reason?: string | null; configs?: GatewayThresholdDebugStatus["configs"] };
			const configs = Array.isArray(result) ? result : (result.configs ?? []);
			return {
				available: Array.isArray(result) ? true : Boolean(result.available),
				reason: Array.isArray(result) ? null : (result.reason ?? null),
				container_name: names.edge,
				configs,
				events
			};
		} catch (error) {
			if (this.isDockerUnavailable(error)) {
				return {
					available: false,
					reason: "Docker daemon is unavailable.",
					container_name: names.edge,
					configs: [],
					events
				};
			}

			return {
				available: false,
				reason: error instanceof Error ? error.message : String(error),
				container_name: names.edge,
				configs: [],
				events
			};
		}
	}

	private async ensureNetwork(): Promise<void> {
		try {
			await this.runDocker(["network", "inspect", this.networkName]);
			return;
		} catch (error) {
			if (this.isDockerUnavailable(error)) {
				throw new Error("Docker daemon is unavailable. Ensure Docker is running and /var/run/docker.sock is mounted.");
			}
			if (!this.isNotFound(error)) {
				throw error;
			}
		}

		await this.runDocker(["network", "create", this.networkName]);
	}

	private async ensureBrokerImage(): Promise<void> {
		try {
			await this.runDocker(["image", "inspect", this.brokerImage]);
			return;
		} catch (error) {
			if (!this.isNotFound(error)) {
				throw error;
			}
		}

		await this.runDocker(["pull", this.brokerImage]);
	}

	private async ensureEdgeImage(): Promise<void> {
		try {
			await this.runDocker(["image", "inspect", this.edgeImage]);
		} catch (error) {
			if (this.isNotFound(error)) {
				throw new Error(`Edge image '${this.edgeImage}' is missing. Build it first (try ./scripts/up-cluster.sh).`);
			}
			throw error;
		}
	}

	private async ensureBrokerContainer(config: GatewayConfig): Promise<void> {
		const names = this.names(config.gateway_id);
		const inspection = await this.inspectContainer(names.broker, BROKER_PORT_KEY);
		if (inspection.exists) {
			return;
		}

		const args = [
			"create",
			"--name",
			names.broker,
			"--network",
			this.networkName,
			"--restart",
			"unless-stopped",
			"--label",
			"gms.sim.managed=true",
			"--label",
			`gms.sim.gateway_id=${config.gateway_id}`,
			"--label",
			"gms.sim.role=broker",
			"--mount",
			`type=volume,src=${names.brokerVolume},dst=/mosquitto/data`
		];

		if (config.host_mqtt_port !== null) {
			args.push("-p", `${config.host_mqtt_port}:1883`);
		}

		args.push(
			this.brokerImage,
			"/bin/sh",
			"-c",
			"cat <<'EOF' >/tmp/mosquitto.conf\nlistener 1883\nallow_anonymous true\npersistence true\npersistence_location /mosquitto/data/\nlog_dest stdout\nEOF\nexec mosquitto -c /tmp/mosquitto.conf"
		);

		await this.runDocker(args);
	}

	private async ensureEdgeContainer(config: GatewayConfig): Promise<boolean> {
		const names = this.names(config.gateway_id);
		const inspection = await this.inspectContainer(names.edge);
		if (inspection.exists) {
			if (!(await this.isStaleContainerImage(inspection, this.edgeImage))) {
				return false;
			}

			await this.removeContainer(names.edge);
		}

		const args = [
			"create",
			"--name",
			names.edge,
			"--network",
			this.networkName,
			"--restart",
			"unless-stopped",
			"--add-host",
			"host.docker.internal:host-gateway",
			"--label",
			"gms.sim.managed=true",
			"--label",
			`gms.sim.gateway_id=${config.gateway_id}`,
			"--label",
			"gms.sim.role=edge",
			"--mount",
			`type=volume,src=${names.edgeVolume},dst=/app/data`,
			"-e",
			`LOCAL_BROKER_HOST=${names.broker}`,
			"-e",
			"LOCAL_BROKER_PORT=1883",
			"-e",
			`CLOUD_BROKER_HOST=${config.cloud_broker.host}`,
			"-e",
			`CLOUD_BROKER_PORT=${config.cloud_broker.port}`,
			"-e",
			`CLOUD_USERNAME=${config.cloud_broker.username ?? ""}`,
			"-e",
			`CLOUD_PASSWORD=${config.cloud_broker.password ?? ""}`,
			"-e",
			`TENANT_ID=${config.tenant_id}`,
			"-e",
			`GREENHOUSE_ID=${config.greenhouse_id}`,
			"-e",
			`GATEWAY_ID=${config.gateway_id}`,
			this.edgeImage
		];

		await this.runDocker(args);
		return true;
	}

	private async startContainer(name: string): Promise<void> {
		const inspection = await this.inspectContainer(name);
		if (!inspection.exists || inspection.running) {
			return;
		}
		await this.runDocker(["start", name]);
	}

	private async stopContainerIfRunning(name: string): Promise<void> {
		const inspection = await this.inspectContainer(name);
		if (!inspection.exists || !inspection.running) {
			return;
		}
		await this.runDocker(["stop", "-t", "8", name]);
	}

	private async removeContainer(name: string): Promise<void> {
		const inspection = await this.inspectContainer(name);
		if (!inspection.exists) {
			return;
		}
		await this.runDocker(["rm", "-f", name]);
	}

	private async inspectContainer(name: string, portKey?: string): Promise<GatewayContainerStatus> {
		try {
			const output = await this.runDocker(["inspect", name]);
			const parsed = JSON.parse(output) as DockerInspectContainer[];
			const inspection = parsed[0];
			if (!inspection) {
				return this.missingStatus(name);
			}

			const hostPortRaw = portKey
				? inspection.NetworkSettings?.Ports?.[portKey]?.[0]?.HostPort ?? null
				: null;
			const hostPortNumber = hostPortRaw !== null ? Number(hostPortRaw) : null;
			const networks = inspection.NetworkSettings?.Networks ?? {};
			const preferredNetwork = networks[this.networkName];
			const fallbackNetwork = Object.values(networks).find((network) => {
				const ip = String(network?.IPAddress ?? "").trim();
				return ip.length > 0;
			});
			const networkIp = String(preferredNetwork?.IPAddress ?? fallbackNetwork?.IPAddress ?? "").trim() || null;

			return {
				exists: true,
				running: Boolean(inspection.State?.Running),
				status: inspection.State?.Status ?? "unknown",
				container_name: name,
				container_id: inspection.Id ?? null,
				image_id: inspection.Image ?? null,
				host_port: Number.isFinite(hostPortNumber) ? hostPortNumber : null,
				network_ip: networkIp
			};
		} catch (error) {
			if (this.isNotFound(error)) {
				return this.missingStatus(name);
			}
			throw error;
		}
	}

	private missingStatus(name: string): GatewayContainerStatus {
		return {
			exists: false,
			running: false,
			status: "missing",
			container_name: name,
			container_id: null,
			image_id: null,
			host_port: null,
			network_ip: null
		};
	}

	private async isStaleContainerImage(status: GatewayContainerStatus, image: string): Promise<boolean> {
		if (!status.image_id) {
			return false;
		}

		try {
			const imageId = (await this.runDocker(["image", "inspect", "--format", "{{.Id}}", image])).trim();
			return Boolean(imageId && imageId !== status.image_id);
		} catch (error) {
			if (this.isNotFound(error)) {
				throw new Error(`Edge image '${image}' is missing. Build it first (try ./scripts/up-cluster.sh).`);
			}
			throw error;
		}
	}

	private async readThresholdLogLines(containerName: string): Promise<string[]> {
		try {
			const output = await this.runDocker(["logs", "--tail", "160", containerName]);
			return output
				.split(/\r?\n/)
				.map((line) => line.trim())
				.filter((line) => line.includes("[THRESHOLD]"))
				.slice(-60)
				.reverse();
		} catch {
			return [];
		}
	}

	private thresholdDebugScript(): string {
		return `import json, os, sqlite3
path = os.environ.get("EDGE_DB_PATH", "/app/data/offline_buffer.db")
tenant_id = os.environ.get("TENANT_ID", "")
greenhouse_id = os.environ.get("GREENHOUSE_ID", "")
conn = sqlite3.connect(path)
conn.row_factory = sqlite3.Row
table_exists = conn.execute("SELECT 1 FROM sqlite_master WHERE type = 'table' AND name = 'threshold_config'").fetchone()
if not table_exists:
    print(json.dumps({
        "available": False,
        "reason": "Edge DB schema is missing threshold_config. Recreate/start this gateway with the current edge image.",
        "configs": []
    }))
    raise SystemExit(0)
rows = conn.execute("""
SELECT tenant_id, greenhouse_id, zone_id, config_version, thresholds_json, applied_at
FROM threshold_config tc
WHERE tenant_id = ?
  AND greenhouse_id = ?
  AND config_version = (
    SELECT MAX(config_version)
    FROM threshold_config
    WHERE tenant_id = tc.tenant_id
      AND greenhouse_id = tc.greenhouse_id
      AND zone_id = tc.zone_id
  )
ORDER BY zone_id
""", (tenant_id, greenhouse_id)).fetchall()
print(json.dumps({
    "available": True,
    "reason": None,
    "configs": [
        {
            "tenant_id": row["tenant_id"],
            "greenhouse_id": row["greenhouse_id"],
            "zone_id": row["zone_id"],
            "config_version": int(row["config_version"]),
            "applied_at": row["applied_at"],
            "thresholds": json.loads(row["thresholds_json"] or "{}"),
        }
        for row in rows
    ]
}))`;
	}

	private async removeVolume(name: string): Promise<void> {
		try {
			await this.runDocker(["volume", "rm", "-f", name]);
		} catch (error) {
			if (this.isNotFound(error)) {
				return;
			}
			throw error;
		}
	}

	private names(gatewayId: string): GatewayContainerNames {
		const key = this.sanitize(gatewayId);
		return {
			broker: `gms_sim_${key}_broker`,
			edge: `gms_sim_${key}_edge`,
			brokerVolume: `gms_sim_${key}_broker_data`,
			edgeVolume: `gms_sim_${key}_edge_data`
		};
	}

	private sanitize(value: string): string {
		return value
			.toLowerCase()
			.replace(/[^a-z0-9-]/g, "-")
			.replace(/-+/g, "-")
			.replace(/^-|-$/g, "")
			.slice(0, 48);
	}

	private async runDocker(args: string[]): Promise<string> {
		try {
			const { stdout } = await execFileAsync("docker", args, {
				encoding: "utf8",
				maxBuffer: 1024 * 1024 * 5
			});
			return stdout;
		} catch (error) {
			throw this.normalizeError(error);
		}
	}

	private normalizeError(error: unknown): Error {
		if (error instanceof Error) {
			return error;
		}

		const wrapped = new Error("Unknown docker command failure");
		(wrapped as { cause?: unknown }).cause = error;
		return wrapped;
	}

	private isNotFound(error: unknown): boolean {
		if (!(error instanceof Error)) {
			return false;
		}

		const text = error.message.toLowerCase();
		return text.includes("no such") || text.includes("not found");
	}

	private isDockerUnavailable(error: unknown): boolean {
		if (!(error instanceof Error)) {
			return false;
		}

		const text = error.message.toLowerCase();
		return (
			text.includes("cannot connect to the docker daemon") ||
			text.includes("failedtoopensocket") ||
			text.includes("permission denied") && text.includes("docker.sock") ||
			text.includes("enoent")
		);
	}
}
