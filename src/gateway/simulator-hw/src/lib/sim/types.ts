export type BinaryValue = 0 | 1;

export interface SensorState {
	air_temp: number;
	air_hum: number;
	soil_moist: number;
	soil_temp: number;
	soil_cond: number;
	soil_ph: number;
	soil_n: number;
	soil_p: number;
	soil_k: number;
	soil_salinity: number;
	soil_tds: number;
}

export interface ThresholdBounds {
	min: number | null;
	max: number | null;
}

export interface SensorThreshold {
	normal: ThresholdBounds;
	warn: ThresholdBounds;
	critical: ThresholdBounds;
}

export interface ZoneThresholdDebugConfig {
	tenant_id: string;
	greenhouse_id: string;
	zone_id: string;
	config_version: number;
	applied_at: string | null;
	thresholds: Partial<Record<keyof SensorState, SensorThreshold>>;
}

export interface GatewayThresholdDebugStatus {
	available: boolean;
	reason: string | null;
	container_name: string;
	configs: ZoneThresholdDebugConfig[];
	events: string[];
}

export interface DigitalInputState {
	din_00: BinaryValue;
	din_01: BinaryValue;
	din_02: BinaryValue;
	din_03: BinaryValue;
}

export interface DigitalOutputState {
	dout_00: BinaryValue;
	dout_01: BinaryValue;
	dout_02: BinaryValue;
	dout_03: BinaryValue;
	dout_04: BinaryValue;
	dout_05: BinaryValue;
	dout_06: BinaryValue;
	dout_07: BinaryValue;
}

export interface SimulationState {
	sensors: SensorState;
	digital_inputs: DigitalInputState;
	digital_outputs: DigitalOutputState;
}

export interface BrokerConfig {
	host: string;
	port: number;
	greenhouse_id: string;
	publish_interval_ms: number;
	username?: string;
	password?: string;
	auto_connect: boolean;
}

export interface ConnectionStatus {
	connected: boolean;
	last_error: string | null;
	last_connect_at: string | null;
}

export interface SimulatorInstanceSummary {
	device_id: string;
	label: string;
	zone_id: string | null;
	zone_name: string | null;
	status: ConnectionStatus;
}

export interface SimulatorRuntime {
	state: SimulationState | null;
	broker: BrokerConfig;
	active_device_id: string | null;
	instances: SimulatorInstanceSummary[];
	profiles: string[];
	events: string[];
}

export interface CloudBrokerConfig {
	host: string;
	port: number;
	username?: string;
	password?: string;
}

export interface GatewayConfig {
	gateway_id: string;
	label: string;
	tenant_id: string;
	greenhouse_id: string;
	host_mqtt_port: number | null;
	cloud_broker: CloudBrokerConfig;
	auto_start: boolean;
}

export interface GatewayContainerStatus {
	exists: boolean;
	running: boolean;
	status: string;
	container_name: string;
	container_id: string | null;
	image_id: string | null;
	host_port: number | null;
	network_ip: string | null;
}

export interface GatewaySummary {
	config: GatewayConfig;
	broker: GatewayContainerStatus;
	edge: GatewayContainerStatus;
	simulator_total_instances: number;
	simulator_connected_instances: number;
}

export interface GatewaySimulatorRuntime extends SimulatorRuntime {
	gateway: GatewaySummary;
	threshold_debug: GatewayThresholdDebugStatus;
}

export interface ClusterRuntime {
	gateways: GatewaySummary[];
	active_gateway_id: string | null;
	events: string[];
}

export const DEFAULT_STATE: SimulationState = {
	sensors: {
		air_temp: 24.5,
		air_hum: 55.2,
		soil_moist: 35,
		soil_temp: 18.4,
		soil_cond: 1.2,
		soil_ph: 6.8,
		soil_n: 45,
		soil_p: 20,
		soil_k: 35,
		soil_salinity: 0.5,
		soil_tds: 250
	},
	digital_inputs: {
		din_00: 0,
		din_01: 0,
		din_02: 1,
		din_03: 0
	},
	digital_outputs: {
		dout_00: 0,
		dout_01: 0,
		dout_02: 0,
		dout_03: 0,
		dout_04: 0,
		dout_05: 0,
		dout_06: 0,
		dout_07: 0
	}
};

export const DEFAULT_BROKER: BrokerConfig = {
	host: "127.0.0.1",
	port: 1883,
	greenhouse_id: "greenhouse-demo",
	publish_interval_ms: 10000,
	auto_connect: true
};
