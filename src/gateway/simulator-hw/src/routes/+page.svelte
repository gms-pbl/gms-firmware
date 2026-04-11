<script lang="ts">
	import { onMount } from "svelte";
	import { Button } from "$lib/components/ui/button";
	import { Badge } from "$lib/components/ui/badge";
	import { Input } from "$lib/components/ui/input";
	import { Label } from "$lib/components/ui/label";
	import { Separator } from "$lib/components/ui/separator";
	import {
		Card,
		CardContent,
		CardDescription,
		CardHeader,
		CardTitle
	} from "$lib/components/ui/card";
	import {
		type ConnectionConfig,
		type DigitalInputState,
		type DigitalOutputState,
		type SensorState,
		type SimulatorRuntime
	} from "$lib/sim/types";

	const sensorKeys: (keyof SensorState)[] = [
		"air_temp",
		"air_hum",
		"soil_moist",
		"soil_temp",
		"soil_cond",
		"soil_ph",
		"soil_n",
		"soil_p",
		"soil_k",
		"soil_salinity",
		"soil_tds"
	];

	const sensorLabels: Record<keyof SensorState, string> = {
		air_temp: "Air Temp",
		air_hum: "Air Hum",
		soil_moist: "Soil Moist",
		soil_temp: "Soil Temp",
		soil_cond: "Soil Cond",
		soil_ph: "Soil pH",
		soil_n: "Soil N",
		soil_p: "Soil P",
		soil_k: "Soil K",
		soil_salinity: "Salinity",
		soil_tds: "Soil TDS"
	};

	const dinKeys: (keyof DigitalInputState)[] = ["din_00", "din_01", "din_02", "din_03"];
	const doutRelayKeys: (keyof DigitalOutputState)[] = ["dout_00", "dout_01", "dout_02", "dout_03"];
	const doutRailKeys: (keyof DigitalOutputState)[] = ["dout_04", "dout_05", "dout_06", "dout_07"];

	let runtime = $state<SimulatorRuntime | null>(null);
	let loading = $state(true);
	let busy = $state(false);
	let errorMessage = $state("");
	let profileName = $state("default");
	let connectionDraftDirty = $state(false);
	let sensorDraftDirty = $state(false);
	let draftsInitialized = $state(false);

	let connectionDraft = $state<ConnectionConfig>({
		host: "127.0.0.1",
		port: 1883,
		telemetry_topic: "telemetry/zone1",
		command_topic: "commands/zone1/output",
		publish_interval_ms: 10000,
		client_id: "virtual-portenta-zone1",
		username: "",
		password: "",
		auto_connect: true
	});

	let sensorDraft = $state<Record<string, string>>({});

	const apiGet = async (url: string) => {
		const response = await fetch(url);
		if (!response.ok) {
			throw new Error(await response.text());
		}
		return response.json();
	};

	const apiPost = async (url: string, body?: unknown) => {
		const response = await fetch(url, {
			method: "POST",
			headers: { "content-type": "application/json" },
			body: body ? JSON.stringify(body) : undefined
		});
		if (!response.ok) {
			throw new Error(await response.text());
		}
		return response.json();
	};

	function hydrateFromRuntime(data: SimulatorRuntime): void {
		runtime = data;

		if (!connectionDraftDirty) {
			connectionDraft = {
				...connectionDraft,
				...data.connection,
				password: ""
			};
		}

		if (!sensorDraftDirty || !draftsInitialized) {
			for (const key of sensorKeys) {
				sensorDraft[key] = String(data.state.sensors[key]);
			}
			draftsInitialized = true;
		}
	}

	async function refreshRuntime(): Promise<void> {
		try {
			const data = (await apiGet("/api/runtime")) as SimulatorRuntime;
			hydrateFromRuntime(data);
			errorMessage = "";
		} catch (error) {
			errorMessage = error instanceof Error ? error.message : String(error);
		} finally {
			loading = false;
		}
	}

	async function runAction(action: () => Promise<void>): Promise<void> {
		if (busy) {
			return;
		}

		busy = true;
		try {
			await action();
			errorMessage = "";
		} catch (error) {
			errorMessage = error instanceof Error ? error.message : String(error);
		} finally {
			busy = false;
			await refreshRuntime();
		}
	}

	function isHigh(value: number): boolean {
		return Number(value) === 1;
	}

	function sensorLabel(key: keyof SensorState): string {
		return sensorLabels[key];
	}

	function ioLabel(key: keyof DigitalInputState | keyof DigitalOutputState): string {
		const [prefix, idx] = key.split("_");
		return `[${prefix === "din" ? "IN" : "OUT"}_${idx}]`;
	}

	function ioButtonClass(active: boolean): string {
		return active
			? "border-emerald-500 bg-emerald-500 text-white hover:bg-emerald-400"
			: "border-slate-300 bg-white text-slate-700 hover:bg-slate-50";
	}

	function eventTone(event: string): "error" | "warn" | "success" | "info" {
		const lower = event.toLowerCase();
		if (lower.includes("error") || lower.includes("failed") || lower.includes("invalid")) return "error";
		if (lower.includes("disconnect") || lower.includes("offline") || lower.includes("reconnect")) return "warn";
		if (
			lower.includes("connected") ||
			lower.includes("subscribed") ||
			lower.includes("saved") ||
			lower.includes("loaded") ||
			lower.includes("updated") ||
			lower.includes("applied") ||
			lower.includes("reset") ||
			lower.includes("cleared")
		)
			return "success";
		return "info";
	}

	function eventRowClass(event: string): string {
		switch (eventTone(event)) {
			case "error":
				return "border-red-300 bg-red-50 text-red-900";
			case "warn":
				return "border-amber-300 bg-amber-50 text-amber-900";
			case "success":
				return "border-emerald-300 bg-emerald-50 text-emerald-900";
			default:
				return "border-sky-300 bg-sky-50 text-sky-900";
		}
	}

	function eventDotClass(event: string): string {
		switch (eventTone(event)) {
			case "error":
				return "bg-red-500";
			case "warn":
				return "bg-amber-500";
			case "success":
				return "bg-emerald-500";
			default:
				return "bg-sky-500";
		}
	}

	function applySensorDraft(): void {
		void runAction(async () => {
			const sensors = Object.fromEntries(sensorKeys.map((key) => [key, Number(sensorDraft[key] ?? 0)]));
			await apiPost("/api/state", { sensors });
			sensorDraftDirty = false;
		});
	}

	function toggleDin(key: keyof DigitalInputState): void {
		const current = runtime;
		if (!current) return;
		void runAction(async () => {
			await apiPost("/api/state", {
				digital_inputs: { [key]: isHigh(current.state.digital_inputs[key]) ? 0 : 1 }
			});
		});
	}

	function toggleDout(key: keyof DigitalOutputState): void {
		const current = runtime;
		if (!current) return;
		void runAction(async () => {
			await apiPost("/api/state", {
				digital_outputs: { [key]: isHigh(current.state.digital_outputs[key]) ? 0 : 1 }
			});
		});
	}

	onMount(() => {
		void refreshRuntime();
		const timer = setInterval(() => void refreshRuntime(), 2000);
		return () => clearInterval(timer);
	});
</script>

<main class="h-dvh overflow-hidden bg-[radial-gradient(circle_at_top,rgba(214,235,255,0.35),transparent_45%),linear-gradient(180deg,#f3f8ff,#f8fbff)] p-3">
	<div class="mx-auto grid h-full w-full max-w-[1600px] grid-rows-[auto_minmax(0,1fr)_12rem] gap-3">
		<header class="flex flex-wrap items-center justify-between gap-2 rounded-xl border border-slate-200/90 bg-white/85 px-3 py-2 shadow-sm">
			<div class="min-w-0">
				<h1 class="font-heading text-2xl font-semibold tracking-tight">GMS Virtual Hardware Simulator</h1>
				<p class="text-muted-foreground text-sm">Compact control panel for sensors, IN_IO, OUT_IO, and MQTT state.</p>
			</div>
			{#if runtime}
				<div class="flex flex-wrap items-center gap-2">
					<Badge variant={runtime.status.connected ? "default" : "secondary"}>{runtime.status.connected ? "MQTT Connected" : "MQTT Disconnected"}</Badge>
					<Badge variant="outline" class="font-mono text-[11px]">{runtime.connection.client_id}@{runtime.connection.host}:{runtime.connection.port}</Badge>
				</div>
			{/if}
		</header>

		{#if loading}
			<div class="flex items-center justify-center rounded-xl border border-slate-200 bg-white/70 text-sm text-slate-500">Loading simulator runtime...</div>
		{:else if runtime}
			<section class="grid min-h-0 gap-3 xl:grid-cols-3">
				<Card class="min-h-0 border-slate-200/90 bg-white/85 shadow-sm">
					<CardHeader class="space-y-1 pb-2">
						<CardTitle class="text-base">Session + Connection</CardTitle>
						<CardDescription class="text-xs">UI settings override env defaults and persist across restarts.</CardDescription>
					</CardHeader>
					<CardContent class="grid gap-2 text-xs">
						{#if errorMessage}
							<p class="rounded-md border border-red-200 bg-red-50 px-2 py-1 text-red-700">{errorMessage}</p>
						{/if}
						<div class="grid gap-2 md:grid-cols-2">
							<div class="grid gap-1"><Label class="text-[11px]" for="mqtt-host">Host</Label><Input class="h-8 text-xs" id="mqtt-host" bind:value={connectionDraft.host} oninput={() => (connectionDraftDirty = true)} /></div>
							<div class="grid gap-1"><Label class="text-[11px]" for="mqtt-port">Port</Label><Input class="h-8 text-xs" id="mqtt-port" type="number" bind:value={connectionDraft.port} oninput={() => (connectionDraftDirty = true)} /></div>
							<div class="grid gap-1"><Label class="text-[11px]" for="client-id">Client ID</Label><Input class="h-8 text-xs" id="client-id" bind:value={connectionDraft.client_id} oninput={() => (connectionDraftDirty = true)} /></div>
							<div class="grid gap-1"><Label class="text-[11px]" for="publish-interval">Interval (ms)</Label><Input class="h-8 text-xs" id="publish-interval" type="number" bind:value={connectionDraft.publish_interval_ms} oninput={() => (connectionDraftDirty = true)} /></div>
							<div class="grid gap-1"><Label class="text-[11px]" for="telemetry-topic">Telemetry Topic</Label><Input class="h-8 text-xs" id="telemetry-topic" bind:value={connectionDraft.telemetry_topic} oninput={() => (connectionDraftDirty = true)} /></div>
							<div class="grid gap-1"><Label class="text-[11px]" for="command-topic">Command Topic</Label><Input class="h-8 text-xs" id="command-topic" bind:value={connectionDraft.command_topic} oninput={() => (connectionDraftDirty = true)} /></div>
						</div>
						<div class="flex flex-wrap gap-2">
							<Button size="sm" disabled={busy} onclick={() => runAction(async () => { await apiPost('/api/connection', connectionDraft); connectionDraftDirty = false; })}>Save Connection</Button>
							<Button variant="outline" size="sm" disabled={busy} onclick={() => runAction(() => apiPost('/api/connect'))}>Connect</Button>
							<Button variant="outline" size="sm" disabled={busy} onclick={() => runAction(() => apiPost('/api/disconnect'))}>Disconnect</Button>
						</div>
						<Separator />
						<div class="grid gap-2 md:grid-cols-[minmax(0,1fr)_auto]">
							<Input class="h-8 text-xs" bind:value={profileName} placeholder="profile name" />
							<div class="flex flex-wrap gap-1.5">
								<Button size="xs" disabled={busy} onclick={() => runAction(() => apiPost('/api/profiles/save', { name: profileName }))}>Save</Button>
								<Button variant="outline" size="xs" disabled={busy} onclick={() => runAction(async () => { sensorDraftDirty = false; await apiPost('/api/profiles/load', { name: profileName }); })}>Load</Button>
								<Button variant="outline" size="xs" disabled={busy} onclick={() => runAction(() => apiPost('/api/profiles/delete', { name: profileName }))}>Delete</Button>
								<Button variant="destructive" size="xs" disabled={busy} onclick={() => runAction(() => apiPost('/api/profiles/clear'))}>Clear Saves</Button>
								<Button variant="outline" size="xs" disabled={busy} onclick={() => runAction(async () => { sensorDraftDirty = false; await apiPost('/api/state/reset'); })}>Reset</Button>
							</div>
						</div>
					</CardContent>
				</Card>

				<Card class="min-h-0 border-slate-200/90 bg-white/85 shadow-sm">
					<CardHeader class="space-y-1 pb-2">
						<CardTitle class="text-base">Sensors</CardTitle>
						<CardDescription class="text-xs">All sensors visible in one compact grid.</CardDescription>
					</CardHeader>
					<CardContent class="grid h-full grid-rows-[1fr_auto] gap-2">
						<div class="grid content-start gap-2 sm:grid-cols-2 lg:grid-cols-3">
							{#each sensorKeys as key}
								<label class="grid gap-1 rounded-md border border-slate-200 bg-slate-50/70 px-2 py-1" for={key}>
									<span class="text-muted-foreground text-[10px] uppercase tracking-wide">{sensorLabel(key)}</span>
									<Input class="h-8 border-slate-300 bg-white font-mono text-xs" id={key} type="number" step="0.1" bind:value={sensorDraft[key]} oninput={() => (sensorDraftDirty = true)} />
								</label>
							{/each}
						</div>
						<div class="flex items-center justify-between gap-2 border-t border-slate-200 pt-2">
							<p class="text-muted-foreground text-xs">Apply updates to runtime state.</p>
							<Button size="sm" disabled={busy} onclick={applySensorDraft}>Apply Sensors</Button>
						</div>
					</CardContent>
				</Card>

				<Card class="min-h-0 border-slate-200/90 bg-white/85 shadow-sm">
					<CardHeader class="space-y-1 pb-2">
						<CardTitle class="text-base">IN_IO + OUT_IO</CardTitle>
						<CardDescription class="text-xs">Compact toggles: green = HIGH, neutral = LOW.</CardDescription>
					</CardHeader>
					<CardContent class="grid gap-2">
						<div class="space-y-1.5">
							<p class="text-muted-foreground text-[11px] uppercase tracking-wide">IN_IO</p>
							<div class="grid gap-2 sm:grid-cols-2">
								{#each dinKeys as key}
									<Button variant="outline" size="sm" disabled={busy} class={`h-9 justify-start font-mono text-xs ${ioButtonClass(isHigh(runtime.state.digital_inputs[key]))}`} onclick={() => toggleDin(key)}>{ioLabel(key)}</Button>
								{/each}
							</div>
						</div>
						<div class="space-y-1.5">
							<p class="text-muted-foreground text-[11px] uppercase tracking-wide">OUT_IO Relay</p>
							<div class="grid gap-2 sm:grid-cols-2">
								{#each doutRelayKeys as key}
									<Button variant="outline" size="sm" disabled={busy} class={`h-9 justify-start font-mono text-xs ${ioButtonClass(isHigh(runtime.state.digital_outputs[key]))}`} onclick={() => toggleDout(key)}>{ioLabel(key)}</Button>
								{/each}
							</div>
						</div>
						<div class="space-y-1.5">
							<p class="text-muted-foreground text-[11px] uppercase tracking-wide">OUT_IO Rail</p>
							<div class="grid gap-2 sm:grid-cols-2">
								{#each doutRailKeys as key}
									<Button variant="outline" size="sm" disabled={busy} class={`h-9 justify-start font-mono text-xs ${ioButtonClass(isHigh(runtime.state.digital_outputs[key]))}`} onclick={() => toggleDout(key)}>{ioLabel(key)}</Button>
								{/each}
							</div>
						</div>
					</CardContent>
				</Card>
			</section>
		{/if}

		{#if runtime}
			<Card class="min-h-0 overflow-hidden border-slate-200/90 bg-white/85 shadow-sm">
				<CardContent class="h-full min-h-0 pt-3 pb-3">
					<div class="grid h-full gap-1 overflow-auto pr-1">
						{#if runtime.events.length === 0}
							<p class="text-muted-foreground text-xs">No events yet.</p>
						{:else}
							{#each runtime.events as event}
								<p class={`flex items-start gap-2 rounded-md border px-2 py-1 font-mono text-[11px] ${eventRowClass(event)}`}>
									<span class={`mt-1 h-1.5 w-1.5 shrink-0 rounded-full ${eventDotClass(event)}`}></span>
									<span class="break-all">{event}</span>
								</p>
							{/each}
						{/if}
					</div>
				</CardContent>
			</Card>
		{/if}
	</div>
</main>
