import { json } from "@sveltejs/kit";
import { simulator } from "$lib/server/simulator";

export async function GET() {
	await simulator.init();
	const profiles = await simulator.listProfiles();
	return json(simulator.getRuntime(profiles));
}
