import { json } from "@sveltejs/kit";
import { cluster } from "$lib/server/cluster";
import { parseGatewayBody } from "$lib/server/request-utils";

export async function POST({ request }) {
	const { payload } = await parseGatewayBody(request);
	const gatewayId = await cluster.addGateway(payload as never);
	return json({ gateway_id: gatewayId, runtime: await cluster.getClusterRuntime() });
}
