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

export interface ConnectionConfig {
	host: string;
	port: number;
	telemetry_topic: string;
	command_topic: string;
	publish_interval_ms: number;
	client_id: string;
	username?: string;
	password?: string;
	auto_connect: boolean;
}

export interface ConnectionStatus {
	connected: boolean;
	last_error: string | null;
	last_connect_at: string | null;
}

export interface SimulatorRuntime {
	state: SimulationState;
	connection: ConnectionConfig;
	status: ConnectionStatus;
	profiles: string[];
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

export const DEFAULT_CONNECTION: ConnectionConfig = {
	host: "127.0.0.1",
	port: 1883,
	telemetry_topic: "telemetry/zone1",
	command_topic: "commands/zone1/output",
	publish_interval_ms: 10000,
	client_id: "virtual-portenta-zone1",
	auto_connect: true
};
