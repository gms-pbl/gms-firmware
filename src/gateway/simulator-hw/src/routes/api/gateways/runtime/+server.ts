import { json } from "@sveltejs/kit";
import { cluster } from "$lib/server/cluster";

export async function GET() {
	return json(await cluster.getClusterRuntime());
}
