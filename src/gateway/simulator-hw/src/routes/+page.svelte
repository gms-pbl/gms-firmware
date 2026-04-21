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
	import type { ClusterRuntime, GatewaySummary } from "$lib/sim/types";

	interface GatewayDraft {
		label: string;
		gateway_id: string;
		tenant_id: string;
		greenhouse_id: string;
		host_mqtt_port: string;
		auto_start: boolean;
		cloud_host: string;
		cloud_port: string;
		cloud_username: string;
		cloud_password: string;
	}

	let runtime = $state<ClusterRuntime | null>(null);
	let loading = $state(true);
	let busy = $state(false);
	let errorMessage = $state("");
	let editingGatewayId = $state("");

	let createDraft = $state<GatewayDraft>({
		label: "",
		gateway_id: "",
		tenant_id: "tenant-demo",
		greenhouse_id: "",
		host_mqtt_port: "",
		auto_start: false,
		cloud_host: "host.docker.internal",
		cloud_port: "1883",
		cloud_username: "",
		cloud_password: ""
	});

	let editDraft = $state<GatewayDraft>({
		label: "",
		gateway_id: "",
		tenant_id: "",
		greenhouse_id: "",
		host_mqtt_port: "",
		auto_start: false,
		cloud_host: "",
		cloud_port: "1883",
		cloud_username: "",
		cloud_password: ""
	});

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
			body: body ? JSON.stringify(body) : JSON.stringify({})
		});
		if (!response.ok) {
			throw new Error(await response.text());
		}
		return response.json();
	};

	async function refreshCluster(): Promise<void> {
		try {
			runtime = (await apiGet("/api/gateways/runtime")) as ClusterRuntime;
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
			await refreshCluster();
		}
	}

	function gatewayState(gateway: GatewaySummary): "running" | "partial" | "stopped" {
		const brokerUp = gateway.broker.running;
		const edgeUp = gateway.edge.running;
		if (brokerUp && edgeUp) {
			return "running";
		}
		if (brokerUp || edgeUp) {
			return "partial";
		}
		return "stopped";
	}

	function gatewayBadgeVariant(gateway: GatewaySummary): "default" | "secondary" {
		return gatewayState(gateway) === "running" ? "default" : "secondary";
	}

	function gatewayStateLabel(gateway: GatewaySummary): string {
		switch (gatewayState(gateway)) {
			case "running":
				return "Running";
			case "partial":
				return "Partial";
			default:
				return "Stopped";
		}
	}

	function startEdit(gateway: GatewaySummary): void {
		editingGatewayId = gateway.config.gateway_id;
		editDraft = {
			label: gateway.config.label,
			gateway_id: gateway.config.gateway_id,
			tenant_id: gateway.config.tenant_id,
			greenhouse_id: gateway.config.greenhouse_id,
			host_mqtt_port: gateway.config.host_mqtt_port ? String(gateway.config.host_mqtt_port) : "",
			auto_start: gateway.config.auto_start,
			cloud_host: gateway.config.cloud_broker.host,
			cloud_port: String(gateway.config.cloud_broker.port),
			cloud_username: gateway.config.cloud_broker.username || "",
			cloud_password: ""
		};
	}

	function cancelEdit(): void {
		editingGatewayId = "";
	}

	function addGateway(): void {
		void runAction(async () => {
			const payload = {
				label: createDraft.label,
				gateway_id: createDraft.gateway_id,
				tenant_id: createDraft.tenant_id,
				greenhouse_id: createDraft.greenhouse_id,
				host_mqtt_port: createDraft.host_mqtt_port ? Number(createDraft.host_mqtt_port) : null,
				auto_start: createDraft.auto_start,
				cloud_broker: {
					host: createDraft.cloud_host,
					port: Number(createDraft.cloud_port || 1883),
					username: createDraft.cloud_username,
					password: createDraft.cloud_password
				}
			};
			await apiPost("/api/gateways/add", payload);

			createDraft = {
				...createDraft,
				label: "",
				gateway_id: "",
				greenhouse_id: "",
				host_mqtt_port: "",
				cloud_password: ""
			};
		});
	}

	function saveGateway(): void {
		if (!editingGatewayId) {
			return;
		}

		void runAction(async () => {
			await apiPost("/api/gateways/update", {
				gateway_id: editingGatewayId,
				label: editDraft.label,
				tenant_id: editDraft.tenant_id,
				greenhouse_id: editDraft.greenhouse_id,
				host_mqtt_port: editDraft.host_mqtt_port ? Number(editDraft.host_mqtt_port) : null,
				auto_start: editDraft.auto_start,
				cloud_broker: {
					host: editDraft.cloud_host,
					port: Number(editDraft.cloud_port || 1883),
					username: editDraft.cloud_username,
					password: editDraft.cloud_password
				}
			});
			editingGatewayId = "";
		});
	}

	function startGateway(gatewayId: string): void {
		void runAction(async () => {
			await apiPost("/api/gateways/start", { gateway_id: gatewayId });
		});
	}

	function stopGateway(gatewayId: string): void {
		void runAction(async () => {
			await apiPost("/api/gateways/stop", { gateway_id: gatewayId });
		});
	}

	function removeGateway(gateway: GatewaySummary): void {
		const confirmed = window.confirm(
			`Remove gateway '${gateway.config.label}' (${gateway.config.gateway_id}) and all local simulator state?`
		);
		if (!confirmed) {
			return;
		}

		void runAction(async () => {
			await apiPost("/api/gateways/remove", { gateway_id: gateway.config.gateway_id });
			if (editingGatewayId === gateway.config.gateway_id) {
				editingGatewayId = "";
			}
		});
	}

	onMount(() => {
		void refreshCluster();
		const timer = setInterval(() => void refreshCluster(), 3000);
		return () => clearInterval(timer);
	});
</script>

<main class="min-h-dvh bg-[radial-gradient(circle_at_top,rgba(193,225,255,0.35),transparent_45%),linear-gradient(180deg,#eef5ff,#f8fbff)] px-3 py-4">
	<div class="mx-auto grid w-full max-w-[1700px] gap-4">
		<header class="flex flex-wrap items-center justify-between gap-2 rounded-xl border border-slate-200/90 bg-white/85 px-3 py-2 shadow-sm">
			<div>
				<h1 class="font-heading text-2xl font-semibold tracking-tight">GMS Gateway Cluster Manager</h1>
				<p class="text-muted-foreground text-sm">Manage simulated greenhouse gateways, brokers, and edge engine containers.</p>
			</div>
			{#if runtime}
				<Badge variant="outline" class="font-mono text-[11px]">
					{runtime.gateways.length} gateway{runtime.gateways.length === 1 ? "" : "s"}
				</Badge>
			{/if}
		</header>

		{#if errorMessage}
			<p class="rounded-md border border-red-200 bg-red-50 px-3 py-2 text-sm text-red-800">{errorMessage}</p>
		{/if}

		<Card class="border-slate-200/90 bg-white/85 shadow-sm">
			<CardHeader class="space-y-1 pb-2">
				<CardTitle class="text-base">Add Simulated Gateway</CardTitle>
				<CardDescription class="text-xs">Creates a dedicated local broker + edge engine container pair.</CardDescription>
			</CardHeader>
			<CardContent class="grid gap-2 text-xs">
				<div class="grid gap-2 md:grid-cols-4">
					<div class="grid gap-1">
						<Label class="text-[11px]">Label</Label>
						<Input class="h-8 text-xs" bind:value={createDraft.label} placeholder="Greenhouse Alpha" />
					</div>
					<div class="grid gap-1">
						<Label class="text-[11px]">Gateway ID (optional)</Label>
						<Input class="h-8 text-xs" bind:value={createDraft.gateway_id} placeholder="gw-alpha" />
					</div>
					<div class="grid gap-1">
						<Label class="text-[11px]">Tenant ID</Label>
						<Input class="h-8 text-xs" bind:value={createDraft.tenant_id} placeholder="tenant-demo" />
					</div>
					<div class="grid gap-1">
						<Label class="text-[11px]">Greenhouse ID</Label>
						<Input class="h-8 text-xs" bind:value={createDraft.greenhouse_id} placeholder="greenhouse-alpha" />
					</div>
				</div>

				<div class="grid gap-2 md:grid-cols-5">
					<div class="grid gap-1 md:col-span-2">
						<Label class="text-[11px]">Cloud Broker Host</Label>
						<Input class="h-8 text-xs" bind:value={createDraft.cloud_host} placeholder="host.docker.internal" />
					</div>
					<div class="grid gap-1">
						<Label class="text-[11px]">Cloud Port</Label>
						<Input class="h-8 text-xs" type="number" bind:value={createDraft.cloud_port} />
					</div>
					<div class="grid gap-1">
						<Label class="text-[11px]">Cloud Username</Label>
						<Input class="h-8 text-xs" bind:value={createDraft.cloud_username} placeholder="optional" />
					</div>
					<div class="grid gap-1">
						<Label class="text-[11px]">Cloud Password</Label>
						<Input class="h-8 text-xs" type="password" bind:value={createDraft.cloud_password} placeholder="optional" />
					</div>
				</div>

				<div class="grid gap-2 md:grid-cols-[minmax(0,1fr)_auto_auto] md:items-end">
					<div class="grid gap-1">
						<Label class="text-[11px]">Expose Local MQTT on Host Port (optional)</Label>
						<Input class="h-8 text-xs" type="number" bind:value={createDraft.host_mqtt_port} placeholder="e.g. 18831" />
					</div>
					<label class="flex h-8 items-center gap-2 text-xs text-slate-700">
						<input type="checkbox" bind:checked={createDraft.auto_start} />
						Auto-start on manager boot
					</label>
					<Button size="sm" disabled={busy} onclick={addGateway}>Add Gateway</Button>
				</div>
			</CardContent>
		</Card>

		{#if loading}
			<div class="rounded-xl border border-slate-200 bg-white/70 p-4 text-sm text-slate-500">Loading gateway cluster...</div>
		{:else if runtime && runtime.gateways.length === 0}
			<div class="rounded-xl border border-slate-200 bg-white/70 p-4 text-sm text-slate-500">No gateways configured yet.</div>
		{:else if runtime}
			<section class="grid gap-3 xl:grid-cols-2">
				{#each runtime.gateways as gateway}
					<Card class="border-slate-200/90 bg-white/85 shadow-sm">
						<CardHeader class="space-y-1 pb-2">
							<div class="flex flex-wrap items-center justify-between gap-2">
								<CardTitle class="text-base">{gateway.config.label}</CardTitle>
								<Badge variant={gatewayBadgeVariant(gateway)}>{gatewayStateLabel(gateway)}</Badge>
							</div>
							<CardDescription class="font-mono text-[11px]">{gateway.config.gateway_id}</CardDescription>
						</CardHeader>
						<CardContent class="grid gap-2 text-xs">
							<div class="grid gap-1 rounded border border-slate-200 bg-slate-50/70 px-2 py-2">
								<p><span class="font-semibold">Tenant:</span> {gateway.config.tenant_id}</p>
								<p><span class="font-semibold">Greenhouse:</span> {gateway.config.greenhouse_id}</p>
								<p><span class="font-semibold">Cloud:</span> {gateway.config.cloud_broker.host}:{gateway.config.cloud_broker.port}</p>
								<p>
									<span class="font-semibold">Local MQTT:</span>
									{gateway.config.host_mqtt_port ? `internal:${gateway.config.host_mqtt_port}` : "internal only"}
								</p>
								<p>
									<span class="font-semibold">Sim Devices:</span>
									{gateway.simulator_connected_instances}/{gateway.simulator_total_instances} connected
								</p>
							</div>

							<div class="flex flex-wrap gap-2">
								<a
									href={`/g/${encodeURIComponent(gateway.config.gateway_id)}`}
									class="inline-flex h-8 items-center rounded border border-slate-300 px-3 text-xs font-semibold uppercase tracking-wide text-slate-700 hover:bg-slate-50"
								>
									Open Simulator
								</a>
								{#if gatewayState(gateway) === "running" || gatewayState(gateway) === "partial"}
									<Button variant="outline" size="sm" disabled={busy} onclick={() => stopGateway(gateway.config.gateway_id)}>Stop</Button>
								{:else}
									<Button size="sm" disabled={busy} onclick={() => startGateway(gateway.config.gateway_id)}>Start</Button>
								{/if}
								<Button variant="outline" size="sm" disabled={busy} onclick={() => startEdit(gateway)}>
									Edit
								</Button>
								<Button variant="destructive" size="sm" disabled={busy} onclick={() => removeGateway(gateway)}>
									Remove
								</Button>
							</div>

							{#if editingGatewayId === gateway.config.gateway_id}
								<Separator />
								<div class="grid gap-2">
									<p class="text-[11px] font-semibold uppercase tracking-wide text-slate-600">Edit Gateway</p>
									<div class="grid gap-2 md:grid-cols-2">
										<div class="grid gap-1"><Label class="text-[11px]">Label</Label><Input class="h-8 text-xs" bind:value={editDraft.label} /></div>
										<div class="grid gap-1"><Label class="text-[11px]">Tenant ID</Label><Input class="h-8 text-xs" bind:value={editDraft.tenant_id} /></div>
										<div class="grid gap-1"><Label class="text-[11px]">Greenhouse ID</Label><Input class="h-8 text-xs" bind:value={editDraft.greenhouse_id} /></div>
										<div class="grid gap-1"><Label class="text-[11px]">Local MQTT host port</Label><Input class="h-8 text-xs" type="number" bind:value={editDraft.host_mqtt_port} placeholder="internal only" /></div>
										<div class="grid gap-1 md:col-span-2"><Label class="text-[11px]">Cloud Host</Label><Input class="h-8 text-xs" bind:value={editDraft.cloud_host} /></div>
										<div class="grid gap-1"><Label class="text-[11px]">Cloud Port</Label><Input class="h-8 text-xs" type="number" bind:value={editDraft.cloud_port} /></div>
										<div class="grid gap-1"><Label class="text-[11px]">Cloud Username</Label><Input class="h-8 text-xs" bind:value={editDraft.cloud_username} /></div>
										<div class="grid gap-1 md:col-span-2"><Label class="text-[11px]">Cloud Password (leave blank to keep current)</Label><Input class="h-8 text-xs" type="password" bind:value={editDraft.cloud_password} /></div>
									</div>
									<label class="flex items-center gap-2 text-xs text-slate-700">
										<input type="checkbox" bind:checked={editDraft.auto_start} />
										Auto-start on manager boot
									</label>
									<div class="flex gap-2">
										<Button size="sm" disabled={busy} onclick={saveGateway}>Save Changes</Button>
										<Button variant="outline" size="sm" disabled={busy} onclick={cancelEdit}>Cancel</Button>
									</div>
								</div>
							{/if}
						</CardContent>
					</Card>
				{/each}
			</section>

			<Card class="min-h-[12rem] overflow-hidden border-slate-200/90 bg-white/85 shadow-sm">
				<CardHeader class="space-y-1 pb-2">
					<CardTitle class="text-base">Cluster Events</CardTitle>
					<CardDescription class="text-xs">Recent orchestration events and state transitions.</CardDescription>
				</CardHeader>
				<CardContent>
					<div class="grid max-h-56 gap-1 overflow-auto pr-1">
						{#if runtime.events.length === 0}
							<p class="text-muted-foreground text-xs">No events yet.</p>
						{:else}
							{#each runtime.events as event}
								<p class="rounded border border-sky-300 bg-sky-50 px-2 py-1 font-mono text-[11px] text-sky-900">
									{event}
								</p>
							{/each}
						{/if}
					</div>
				</CardContent>
			</Card>
		{/if}
	</div>
</main>
