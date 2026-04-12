import { json } from "@sveltejs/kit";
import { simulator } from "$lib/server/simulator";

export async function POST({ request }) {
	await simulator.init();

	const body = (await request.json()) as { device_id?: string };
	if (!body.device_id) {
		return json({ error: "device_id is required." }, { status: 400 });
	}

	await simulator.selectInstance(body.device_id);

	const profiles = await simulator.listProfiles();
	return json(simulator.getRuntime(profiles));
}
