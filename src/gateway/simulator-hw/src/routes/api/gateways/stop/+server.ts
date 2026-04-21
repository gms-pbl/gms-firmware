import { json } from "@sveltejs/kit";
import { cluster } from "$lib/server/cluster";
import { parseGatewayBody } from "$lib/server/request-utils";

export async function POST({ request }) {
	const { gatewayId } = await parseGatewayBody(request);
	if (!gatewayId) {
		return json({ error: "gateway_id is required." }, { status: 400 });
	}

	await cluster.stopGateway(gatewayId);
	return json(await cluster.getClusterRuntime());
}
