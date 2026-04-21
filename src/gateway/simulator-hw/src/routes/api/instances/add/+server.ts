import { json } from "@sveltejs/kit";
import { cluster } from "$lib/server/cluster";
import { parseGatewayBody } from "$lib/server/request-utils";

export async function POST({ request }) {
	const { gatewayId, payload } = await parseGatewayBody(request);
	const { simulator } = await cluster.getGatewaySimulator(gatewayId);
	await simulator.addInstance(typeof payload.label === "string" ? payload.label : undefined);
	return json(await cluster.getGatewayRuntime(gatewayId));
}
