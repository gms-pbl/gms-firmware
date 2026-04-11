import { json } from "@sveltejs/kit";
import { simulator } from "$lib/server/simulator";

export async function POST() {
	await simulator.init();
	await simulator.resetState();

	const profiles = await simulator.listProfiles();
	return json(simulator.getRuntime(profiles));
}
