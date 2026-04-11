import { json } from "@sveltejs/kit";
import { simulator } from "$lib/server/simulator";

export async function POST() {
	await simulator.init();
	await simulator.clearProfiles();

	const profiles = await simulator.listProfiles();
	return json(simulator.getRuntime(profiles));
}
