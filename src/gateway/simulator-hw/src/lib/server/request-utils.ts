export function gatewayIdFromUrl(url: URL): string | undefined {
	const raw = url.searchParams.get("gateway_id");
	if (!raw) {
		return undefined;
	}
	const trimmed = raw.trim();
	return trimmed || undefined;
}

export async function parseGatewayBody(
	request: Request
): Promise<{ gatewayId: string | undefined; payload: Record<string, unknown> }> {
	let body: Record<string, unknown> = {};

	try {
		const parsed = (await request.json()) as unknown;
		if (parsed && typeof parsed === "object" && !Array.isArray(parsed)) {
			body = parsed as Record<string, unknown>;
		}
	} catch {
		body = {};
	}

	const gatewayCandidate = typeof body.gateway_id === "string" ? body.gateway_id : undefined;
	const gatewayId = gatewayCandidate?.trim() || undefined;

	const { gateway_id: _gatewayIgnored, ...payload } = body;
	void _gatewayIgnored;

	return { gatewayId, payload };
}
