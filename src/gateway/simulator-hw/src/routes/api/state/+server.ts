import { json } from "@sveltejs/kit";
import { simulator } from "$lib/server/simulator";

export async function POST({ request }) {
	await simulator.init();

	const payload = (await request.json()) as Record<string, unknown>;
	await simulator.updateState(payload as never);

	const profiles = await simulator.listProfiles();
	return json(simulator.getRuntime(profiles));
}
