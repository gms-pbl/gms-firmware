import { json } from "@sveltejs/kit";
import { cluster } from "$lib/server/cluster";
import { parseGatewayBody } from "$lib/server/request-utils";

export async function POST({ request }) {
	const { gatewayId, payload } = await parseGatewayBody(request);
	if (typeof payload.device_id !== "string" || !payload.device_id.trim()) {
		return json({ error: "device_id is required." }, { status: 400 });
	}

	const { simulator } = await cluster.getGatewaySimulator(gatewayId);
	await simulator.removeInstance(payload.device_id.trim());
	return json(await cluster.getGatewayRuntime(gatewayId));
}
