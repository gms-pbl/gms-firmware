import { json } from "@sveltejs/kit";
import { simulator } from "$lib/server/simulator";

export async function POST({ request }) {
	await simulator.init();

	const body = (await request.json()) as { label?: string };
	await simulator.addInstance(body.label);

	const profiles = await simulator.listProfiles();
	return json(simulator.getRuntime(profiles));
}
