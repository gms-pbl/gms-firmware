import { json } from "@sveltejs/kit";
import { cluster } from "$lib/server/cluster";
import { gatewayIdFromUrl } from "$lib/server/request-utils";

export async function GET({ url }) {
	const gatewayId = gatewayIdFromUrl(url);
	const { simulator } = await cluster.getGatewaySimulator(gatewayId, false);
	return json({ profiles: await simulator.listProfiles() });
}
