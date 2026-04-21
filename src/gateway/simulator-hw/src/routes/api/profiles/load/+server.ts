import { json } from "@sveltejs/kit";
import { cluster } from "$lib/server/cluster";
import { parseGatewayBody } from "$lib/server/request-utils";

export async function POST({ request }) {
	const { gatewayId, payload } = await parseGatewayBody(request);
	if (typeof payload.name !== "string" || !payload.name.trim()) {
		return json({ error: "Profile name is required." }, { status: 400 });
	}

	const { simulator } = await cluster.getGatewaySimulator(gatewayId);
	await simulator.loadProfile(payload.name.trim());
	return json(await cluster.getGatewayRuntime(gatewayId));
}
