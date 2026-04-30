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
		type SensorThreshold,
		type ZoneThresholdDebugConfig,
		type GatewaySimulatorRuntime
	} from "$lib/sim/types";

	type ThresholdLevel = "normal" | "warn" | "critical";
	type ThresholdRow = {
		key: keyof SensorState;
		label: string;
		threshold: SensorThreshold;
	};

	let { data } = $props();
	const gatewayId = $derived(String(data?.gatewayId ?? ""));

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

	let runtime = $state<GatewaySimulatorRuntime | null>(null);
	let loading = $state(true);
	let busy = $state(false);
	let errorMessage = $state("");
	let profileName = $state("default");
	let newInstanceLabel = $state("");
	let brokerDraftDirty = $state(false);
	let sensorDraftDirty = $state(false);
	let draftsInitialized = $state(false);
	let thresholdDialogOpen = $state(false);

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
		const payload = {
			...(body && typeof body === "object" ? (body as Record<string, unknown>) : {}),
			gateway_id: gatewayId
		};

		const response = await fetch(url, {
			method: "POST",
			headers: { "content-type": "application/json" },
			body: JSON.stringify(payload)
		});
		if (!response.ok) {
			throw new Error(await response.text());
		}
		return response.json();
	};

	function hydrateFromRuntime(data: GatewaySimulatorRuntime): void {
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
			const data = (await apiGet(`/api/runtime?gateway_id=${encodeURIComponent(gatewayId)}`)) as GatewaySimulatorRuntime;
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

	function thresholdConfigCount(): number {
		return visibleThresholdConfigs().length;
	}

	function activeThresholdZoneId(): string | null {
		return activeInstance()?.zone_id ?? null;
	}

	function visibleThresholdConfigs() {
		const configs = runtime?.threshold_debug?.configs ?? [];
		const selectedZoneId = activeThresholdZoneId();
		if (!selectedZoneId) {
			return configs;
		}
		const matches = configs.filter((config) => config.zone_id === selectedZoneId);
		return matches.length > 0 ? matches : [];
	}

	function thresholdSummary(): string {
		const debug = runtime?.threshold_debug;
		if (!debug) {
			return "Threshold debug data has not loaded yet.";
		}

		if (!debug.available) {
			return debug.reason || "Threshold debug data is unavailable.";
		}

		if (debug.configs.length === 0) {
			return "No threshold configs are cached on the edge gateway yet.";
		}

		const selectedZoneId = activeThresholdZoneId();
		if (selectedZoneId && visibleThresholdConfigs().length === 0) {
			return `No cached threshold config for the selected Portenta zone ${selectedZoneId}.`;
		}

		const count = visibleThresholdConfigs().length;
		if (selectedZoneId) {
			return `${count} zone threshold config${count === 1 ? "" : "s"} cached for the selected Portenta zone.`;
		}

		return `${count} zone threshold config${count === 1 ? "" : "s"} cached on edge.`;
	}

	function thresholdBadgeVariant(): "default" | "secondary" | "outline" {
		const debug = runtime?.threshold_debug;
		if (!debug) return "secondary";
		if (!debug.available) return "secondary";
		return thresholdConfigCount() > 0 ? "default" : "outline";
	}

	function thresholdRows(config: ZoneThresholdDebugConfig): ThresholdRow[] {
		return sensorKeys
			.map((key) => {
				const threshold = config.thresholds[key];
				return threshold ? { key, label: sensorLabel(key), threshold } : null;
			})
			.filter((row): row is ThresholdRow => row !== null);
	}

	function formatBound(value: number | null | undefined): string {
		if (value === null || value === undefined) {
			return "none";
		}

		return Number(value).toLocaleString(undefined, { maximumFractionDigits: 2 });
	}

	function formatThresholdRange(threshold: SensorThreshold, level: ThresholdLevel): string {
		const bounds = threshold[level];
		return `${formatBound(bounds?.min)} .. ${formatBound(bounds?.max)}`;
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

	function resetActiveState(): void {
		if (!runtime?.state) {
			return;
		}

		void runAction(async () => {
			sensorDraftDirty = false;
			await apiPost("/api/state/reset");
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
				<p class="text-muted-foreground text-sm">
					Gateway `{runtime?.gateway?.config.gateway_id ?? gatewayId}` · Greenhouse `{runtime?.gateway?.config.greenhouse_id ?? runtime?.broker.greenhouse_id}`
				</p>
			</div>
			{#if runtime}
				<div class="flex flex-wrap items-center gap-2">
					<a href="/" class="rounded border border-slate-300 px-2 py-1 text-xs font-semibold uppercase tracking-wide text-slate-700 hover:bg-slate-50">Gateway Manager</a>
					<Button variant="outline" size="sm" onclick={() => (thresholdDialogOpen = true)}>Thresholds</Button>
					<Badge variant={connectedCount() > 0 ? "default" : "secondary"}>
						{connectedCount()} / {runtime.instances.length} Connected
					</Badge>
					<Badge variant={thresholdBadgeVariant()}>
						{thresholdConfigCount()} Threshold Zones
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
						<div class="rounded-md border border-slate-200 bg-white px-2 py-2">
							<div class="flex flex-wrap items-start justify-between gap-2">
								<div class="min-w-0">
									<p class="text-xs font-semibold text-slate-900">Threshold debug</p>
									<p class="text-muted-foreground mt-0.5 text-[11px]">{thresholdSummary()}</p>
								</div>
								<div class="flex gap-1.5">
									<Button variant="outline" size="xs" disabled={busy} onclick={() => void refreshRuntime()}>Refresh</Button>
									<Button size="xs" onclick={() => (thresholdDialogOpen = true)}>Inspect</Button>
								</div>
							</div>
						</div>
						<Separator />
						<div class="grid gap-2 md:grid-cols-[minmax(0,1fr)_auto]">
							<div class="grid gap-1">
								<Label class="text-[11px]" for="profile-name">State profile name</Label>
								<Input class="h-8 text-xs" id="profile-name" bind:value={profileName} aria-describedby="profile-help" placeholder="default" />
								<p id="profile-help" class="text-muted-foreground text-[11px]">Save/load the active virtual Portenta sensor state.</p>
							</div>
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
							<div class="flex flex-wrap items-center justify-between gap-2 rounded border border-slate-200 bg-slate-50/70 px-2 py-1.5">
								<p class="text-muted-foreground text-[11px]">
									{sensorDraftDirty ? "You have unapplied sensor edits." : "Sensor values are in sync."}
								</p>
								<div class="flex gap-2">
									<Button size="sm" disabled={busy || !sensorDraftDirty} onclick={applySensorDraft}>Save Sensors</Button>
									<Button variant="outline" size="sm" disabled={busy} onclick={resetActiveState}>Reset State</Button>
								</div>
							</div>
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
								<div class="flex gap-2">
									<Button size="sm" disabled={busy || !sensorDraftDirty} onclick={applySensorDraft}>Save Sensors</Button>
									<Button variant="outline" size="sm" disabled={busy} onclick={resetActiveState}>Reset State</Button>
								</div>
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
					Tip: check `docker logs gms_sim_cluster_manager` for details.
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

		{#if runtime && thresholdDialogOpen}
			<div class="fixed inset-0 z-50 p-4 sm:p-6">
				<button
					type="button"
					class="absolute inset-0 bg-slate-950/45"
					aria-label="Close threshold inspector"
					onclick={() => (thresholdDialogOpen = false)}
				></button>
				<div
					class="relative mx-auto grid max-h-[90dvh] w-full max-w-5xl grid-rows-[auto_minmax(0,1fr)] overflow-hidden rounded-lg border border-slate-200 bg-white shadow-sm"
					role="dialog"
					aria-modal="true"
					aria-labelledby="threshold-dialog-title"
				>
					<header class="flex flex-wrap items-start justify-between gap-3 border-b border-slate-200 px-4 py-3">
						<div class="min-w-0">
							<h2 id="threshold-dialog-title" class="text-base font-semibold text-slate-950">Edge threshold configs</h2>
							<p class="text-muted-foreground mt-1 text-xs">{thresholdSummary()}</p>
							<p class="text-muted-foreground mt-1 break-all font-mono text-[11px]">Container: {runtime.threshold_debug?.container_name}</p>
						</div>
						<div class="flex gap-2">
							<Button variant="outline" size="sm" disabled={busy} onclick={() => void refreshRuntime()}>Refresh</Button>
							<Button size="sm" onclick={() => (thresholdDialogOpen = false)}>Close</Button>
						</div>
					</header>
					<div class="min-h-0 overflow-auto px-4 py-3">
						{#if !runtime.threshold_debug?.available}
							<p class="rounded-md border border-amber-200 bg-amber-50 px-3 py-2 text-sm text-amber-900">
								{runtime.threshold_debug?.reason || "Threshold debug data is unavailable."}
							</p>
						{:else if visibleThresholdConfigs().length === 0}
							<p class="rounded-md border border-slate-200 bg-slate-50 px-3 py-2 text-sm text-slate-700">
								{activeThresholdZoneId()
									? `No threshold config is currently cached for the selected Portenta zone ${activeThresholdZoneId()}.`
									: 'No threshold configs are currently stored on this edge gateway. Save thresholds from the frontend, then refresh this panel.'}
							</p>
						{:else}
							<div class="grid gap-3">
								{#each visibleThresholdConfigs() as config (config.zone_id)}
									<article class="overflow-hidden rounded-md border border-slate-200">
										<div class="flex flex-wrap items-center justify-between gap-2 border-b border-slate-200 bg-slate-50 px-3 py-2">
											<div class="min-w-0">
												<p class="truncate text-sm font-semibold text-slate-950">Zone {config.zone_id}</p>
												<p class="text-muted-foreground font-mono text-[11px]">tenant={config.tenant_id} greenhouse={config.greenhouse_id}</p>
											</div>
											<div class="flex flex-wrap gap-1.5">
												<Badge variant="outline">v{config.config_version}</Badge>
												<Badge variant="secondary">{config.applied_at || "applied_at unknown"}</Badge>
											</div>
										</div>
										<div class="grid gap-0 divide-y divide-slate-100">
											{#each thresholdRows(config) as row (row.key)}
												<div class="grid gap-2 px-3 py-2 text-xs md:grid-cols-[10rem_repeat(3,minmax(0,1fr))]">
													<p class="font-medium text-slate-900">{row.label}</p>
													<p><span class="text-muted-foreground">Normal</span> <span class="font-mono">{formatThresholdRange(row.threshold, "normal")}</span></p>
													<p><span class="text-muted-foreground">Warn</span> <span class="font-mono">{formatThresholdRange(row.threshold, "warn")}</span></p>
													<p><span class="text-muted-foreground">Critical</span> <span class="font-mono">{formatThresholdRange(row.threshold, "critical")}</span></p>
												</div>
											{/each}
										</div>
									</article>
								{/each}
							</div>
						{/if}

						<div class="mt-4 rounded-md border border-slate-200">
							<div class="border-b border-slate-200 bg-slate-50 px-3 py-2">
								<p class="text-sm font-semibold text-slate-950">Recent threshold log lines</p>
							</div>
							<div class="grid max-h-56 gap-1 overflow-auto p-2">
								{#if runtime.threshold_debug?.events.length}
									{#each runtime.threshold_debug.events as event}
										<p class="rounded border border-slate-200 bg-white px-2 py-1 font-mono text-[11px] text-slate-800">{event}</p>
									{/each}
								{:else}
									<p class="text-muted-foreground px-2 py-1 text-xs">No `[THRESHOLD]` lines found in recent edge logs.</p>
								{/if}
							</div>
						</div>
					</div>
				</div>
			</div>
		{/if}
	</div>
</main>
