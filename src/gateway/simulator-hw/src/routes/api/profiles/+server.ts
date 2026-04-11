import { json } from "@sveltejs/kit";
import { simulator } from "$lib/server/simulator";

export async function GET() {
	await simulator.init();
	return json({ profiles: await simulator.listProfiles() });
}
