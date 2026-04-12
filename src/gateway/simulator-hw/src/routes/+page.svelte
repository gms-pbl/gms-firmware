<script lang="ts">
	import { onMount } from "svelte";
	import { Badge } from "$lib/components/ui/badge";
	import { Button } from "$lib/components/ui/button";
	import {
		Card,
		CardContent,
		CardDescription,
		CardHeader,
		CardTitle
	} from "$lib/components/ui/card";
	import { Input } from "$lib/components/ui/input";
	import { Label } from "$lib/components/ui/label";
	import { Separator } from "$lib/components/ui/separator";
	import {
		type BrokerConfig,
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
	let newInstanceLabel = $state("");
	let brokerDraftDirty = $state(false);
	let sensorDraftDirty = $state(false);
	let draftsInitialized = $state(false);

	let brokerDraft = $state<BrokerConfig>({
		host: "127.0.0.1",
		port: 1883,
		greenhouse_id: "greenhouse-demo",
		publish_interval_ms: 10000,
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

		if (!brokerDraftDirty) {
			brokerDraft = {
				...brokerDraft,
				...data.broker,
				password: ""
			};
		}

		if (!sensorDraftDirty && data.state) {
			for (const key of sensorKeys) {
				sensorDraft[key] = String(data.state.sensors[key]);
			}
			draftsInitialized = true;
		}

		if (!draftsInitialized && data.state) {
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

	function activeInstance() {
		if (!runtime?.active_device_id) {
			return null;
		}
		return runtime.instances.find((instance) => instance.device_id === runtime?.active_device_id) ?? null;
	}

	function connectedCount(): number {
		if (!runtime) {
			return 0;
		}

		return runtime.instances.filter((instance) => instance.status.connected).length;
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

	function instanceStatusTone(connected: boolean): "default" | "secondary" {
		return connected ? "default" : "secondary";
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
			lower.includes("cleared") ||
			lower.includes("added") ||
			lower.includes("selected") ||
			lower.includes("removed")
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
		if (!runtime?.state) {
			return;
		}

		void runAction(async () => {
			const sensors = Object.fromEntries(sensorKeys.map((key) => [key, Number(sensorDraft[key] ?? 0)]));
			await apiPost("/api/state", { sensors });
			sensorDraftDirty = false;
		});
	}

	function toggleDin(key: keyof DigitalInputState): void {
		const current = runtime;
		if (!current?.state) return;
		const activeState = current.state;
		void runAction(async () => {
			await apiPost("/api/state", {
				digital_inputs: { [key]: isHigh(activeState.digital_inputs[key]) ? 0 : 1 }
			});
		});
	}

	function toggleDout(key: keyof DigitalOutputState): void {
		const current = runtime;
		if (!current?.state) return;
		const activeState = current.state;
		void runAction(async () => {
			await apiPost("/api/state", {
				digital_outputs: { [key]: isHigh(activeState.digital_outputs[key]) ? 0 : 1 }
			});
		});
	}

	function addInstance(): void {
		void runAction(async () => {
			await apiPost("/api/instances/add", { label: newInstanceLabel.trim() || undefined });
			newInstanceLabel = "";
			sensorDraftDirty = false;
		});
	}

	function removeActiveInstance(): void {
		if (!runtime?.active_device_id) {
			return;
		}

		void runAction(async () => {
			await apiPost("/api/instances/remove", { device_id: runtime?.active_device_id });
			sensorDraftDirty = false;
		});
	}

	function selectInstance(deviceId: string): void {
		void runAction(async () => {
			await apiPost("/api/instances/select", { device_id: deviceId });
			sensorDraftDirty = false;
		});
	}

	onMount(() => {
		void refreshRuntime();
		const timer = setInterval(() => void refreshRuntime(), 2000);
		return () => clearInterval(timer);
	});
</script>

<main class="h-dvh overflow-hidden bg-[radial-gradient(circle_at_top,rgba(214,235,255,0.35),transparent_45%),linear-gradient(180deg,#f3f8ff,#f8fbff)] p-3">
	<div class="mx-auto grid h-full w-full max-w-[1700px] grid-rows-[auto_minmax(0,1fr)_12rem] gap-3">
		<header class="flex flex-wrap items-center justify-between gap-2 rounded-xl border border-slate-200/90 bg-white/85 px-3 py-2 shadow-sm">
			<div class="min-w-0">
				<h1 class="font-heading text-2xl font-semibold tracking-tight">GMS Virtual Hardware Simulator</h1>
				<p class="text-muted-foreground text-sm">Multi-zone Portenta simulator with GUID-based device identities.</p>
			</div>
			{#if runtime}
				<div class="flex flex-wrap items-center gap-2">
					<Badge variant={connectedCount() > 0 ? "default" : "secondary"}>
						{connectedCount()} / {runtime.instances.length} Connected
					</Badge>
					<Badge variant="outline" class="font-mono text-[11px]">
						GH: {runtime.broker.greenhouse_id}
					</Badge>
				</div>
			{/if}
		</header>

		{#if loading}
			<div class="flex items-center justify-center rounded-xl border border-slate-200 bg-white/70 text-sm text-slate-500">
				Loading simulator runtime...
			</div>
		{:else if runtime}
			<section class="grid min-h-0 gap-3 xl:grid-cols-[1.05fr_1fr_1.25fr]">
				<Card class="min-h-0 border-slate-200/90 bg-white/85 shadow-sm">
					<CardHeader class="space-y-1 pb-2">
						<CardTitle class="text-base">Instances</CardTitle>
						<CardDescription class="text-xs">Create/remove/select virtual Portentas (GUID device IDs).</CardDescription>
					</CardHeader>
					<CardContent class="grid h-full grid-rows-[auto_minmax(0,1fr)] gap-2">
						<div class="grid gap-2 md:grid-cols-[minmax(0,1fr)_auto_auto]">
							<Input class="h-8 text-xs" bind:value={newInstanceLabel} placeholder="new instance label" />
							<Button size="sm" disabled={busy} onclick={addInstance}>Add</Button>
							<Button variant="destructive" size="sm" disabled={busy || runtime.instances.length <= 1} onclick={removeActiveInstance}>Remove</Button>
						</div>
						<div class="grid min-h-0 content-start gap-2 overflow-auto rounded-md border border-slate-200 p-2">
							{#each runtime.instances as instance}
								<button
									type="button"
									disabled={busy}
									onclick={() => selectInstance(instance.device_id)}
									class={`grid gap-1 rounded-md border px-2 py-1 text-left transition-colors ${runtime.active_device_id === instance.device_id ? "border-sky-400 bg-sky-50" : "border-slate-200 bg-white hover:bg-slate-50"}`}
								>
									<div class="flex items-center justify-between gap-2">
										<p class="truncate text-xs font-semibold">{instance.label}</p>
										<Badge variant={instanceStatusTone(instance.status.connected)}>{instance.status.connected ? "Online" : "Offline"}</Badge>
									</div>
									<p class="text-muted-foreground truncate font-mono text-[11px]">{instance.device_id}</p>
									<p class="text-muted-foreground truncate text-[11px]">
										Zone: {instance.zone_name ? `${instance.zone_name} (${instance.zone_id})` : "unassigned"}
									</p>
								</button>
							{/each}
						</div>
					</CardContent>
				</Card>

				<Card class="min-h-0 border-slate-200/90 bg-white/85 shadow-sm">
					<CardHeader class="space-y-1 pb-2">
						<CardTitle class="text-base">Broker + Session</CardTitle>
						<CardDescription class="text-xs">One broker config shared by all instances.</CardDescription>
					</CardHeader>
					<CardContent class="grid gap-2 text-xs">
						{#if errorMessage}
							<p class="rounded-md border border-red-200 bg-red-50 px-2 py-1 text-red-700">{errorMessage}</p>
						{/if}
						<div class="grid gap-2 md:grid-cols-2">
							<div class="grid gap-1"><Label class="text-[11px]" for="mqtt-host">Host</Label><Input class="h-8 text-xs" id="mqtt-host" bind:value={brokerDraft.host} oninput={() => (brokerDraftDirty = true)} /></div>
							<div class="grid gap-1"><Label class="text-[11px]" for="mqtt-port">Port</Label><Input class="h-8 text-xs" id="mqtt-port" type="number" bind:value={brokerDraft.port} oninput={() => (brokerDraftDirty = true)} /></div>
							<div class="grid gap-1"><Label class="text-[11px]" for="greenhouse-id">Greenhouse ID</Label><Input class="h-8 text-xs" id="greenhouse-id" bind:value={brokerDraft.greenhouse_id} oninput={() => (brokerDraftDirty = true)} /></div>
							<div class="grid gap-1"><Label class="text-[11px]" for="publish-interval">Interval (ms)</Label><Input class="h-8 text-xs" id="publish-interval" type="number" bind:value={brokerDraft.publish_interval_ms} oninput={() => (brokerDraftDirty = true)} /></div>
						</div>
						<div class="flex flex-wrap gap-2">
							<Button size="sm" disabled={busy} onclick={() => runAction(async () => { await apiPost("/api/connection", brokerDraft); brokerDraftDirty = false; })}>Save Broker</Button>
							<Button variant="outline" size="sm" disabled={busy} onclick={() => runAction(() => apiPost("/api/connect"))}>Connect All</Button>
							<Button variant="outline" size="sm" disabled={busy} onclick={() => runAction(() => apiPost("/api/disconnect"))}>Disconnect All</Button>
						</div>
						<Separator />
						<div class="grid gap-2 md:grid-cols-[minmax(0,1fr)_auto]">
							<Input class="h-8 text-xs" bind:value={profileName} placeholder="profile name (active instance)" />
							<div class="flex flex-wrap gap-1.5">
								<Button size="xs" disabled={busy} onclick={() => runAction(() => apiPost("/api/profiles/save", { name: profileName }))}>Save</Button>
								<Button variant="outline" size="xs" disabled={busy} onclick={() => runAction(async () => { sensorDraftDirty = false; await apiPost("/api/profiles/load", { name: profileName }); })}>Load</Button>
								<Button variant="outline" size="xs" disabled={busy} onclick={() => runAction(() => apiPost("/api/profiles/delete", { name: profileName }))}>Delete</Button>
								<Button variant="destructive" size="xs" disabled={busy} onclick={() => runAction(() => apiPost("/api/profiles/clear"))}>Clear Saves</Button>
								<Button variant="outline" size="xs" disabled={busy} onclick={() => runAction(async () => { sensorDraftDirty = false; await apiPost("/api/state/reset"); })}>Reset</Button>
							</div>
						</div>
					</CardContent>
				</Card>

				<Card class="min-h-0 border-slate-200/90 bg-white/85 shadow-sm">
					<CardHeader class="space-y-1 pb-2">
						<CardTitle class="text-base">Active Instance Controls</CardTitle>
						<CardDescription class="text-xs">
							{#if activeInstance()}
								Editing: {activeInstance()?.label} ({activeInstance()?.device_id})
							{:else}
								Select an instance to edit state.
							{/if}
						</CardDescription>
					</CardHeader>
					<CardContent class="grid h-full grid-rows-[1fr_auto] gap-2">
						{#if runtime.state}
							<div class="grid min-h-0 content-start gap-2 overflow-auto">
								<div class="grid gap-2 sm:grid-cols-2 lg:grid-cols-3">
									{#each sensorKeys as key}
										<label class="grid gap-1 rounded-md border border-slate-200 bg-slate-50/70 px-2 py-1" for={key}>
											<span class="text-muted-foreground text-[10px] uppercase tracking-wide">{sensorLabel(key)}</span>
											<Input class="h-8 border-slate-300 bg-white font-mono text-xs" id={key} type="number" step="0.1" bind:value={sensorDraft[key]} oninput={() => (sensorDraftDirty = true)} />
										</label>
									{/each}
								</div>
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
							</div>
							<div class="flex items-center justify-between gap-2 border-t border-slate-200 pt-2">
								<p class="text-muted-foreground text-xs">Apply updates to active instance state.</p>
								<Button size="sm" disabled={busy} onclick={applySensorDraft}>Apply Sensors</Button>
							</div>
						{:else}
							<div class="text-muted-foreground flex items-center justify-center rounded-md border border-slate-200 bg-slate-50 text-xs">
								No active instance selected.
							</div>
						{/if}
					</CardContent>
				</Card>
			</section>
		{:else}
			<div class="rounded-xl border border-red-200 bg-red-50/85 p-4 text-sm text-red-800">
				<p class="font-semibold">Simulator runtime failed to load.</p>
				<p class="mt-1 break-all">{errorMessage || "Unknown error"}</p>
				<p class="mt-2 text-xs text-red-700">
					Tip: check `docker logs gms_simulator_hw` for details.
				</p>
			</div>
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
