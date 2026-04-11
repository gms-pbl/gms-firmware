import { json } from "@sveltejs/kit";
import { simulator } from "$lib/server/simulator";

export async function POST({ request }) {
	await simulator.init();

	const body = (await request.json()) as { name?: string };
	if (!body.name) {
		return json({ error: "Profile name is required." }, { status: 400 });
	}

	await simulator.saveProfile(body.name);
	const profiles = await simulator.listProfiles();
	return json(simulator.getRuntime(profiles));
}
