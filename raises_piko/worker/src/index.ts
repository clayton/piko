const MAX_BODY_BYTES = 64 * 1024;
const MAX_AGE_SECONDS = 300;
const MAX_EVENTS = 12;
const encoder = new TextEncoder();

interface PikoEvent {
  id: string;
  type: string;
  project: string;
  message: string;
  created_at: string;
}

interface PikoState {
  revision: number;
  events: PikoEvent[];
}

function json(value: unknown, status = 200): Response {
  return Response.json(value, {
    status,
    headers: { "cache-control": "no-store" },
  });
}

function bytesFromHex(value: string): Uint8Array | null {
  if (!/^[0-9a-f]{64}$/.test(value)) return null;
  const bytes = value.match(/../g);
  return bytes ? Uint8Array.from(bytes, (byte) => Number.parseInt(byte, 16)) : null;
}

async function validRaisesSignature(request: Request, secret: string, body: string): Promise<boolean> {
  const timestamp = request.headers.get("X-Raises-Timestamp");
  const signature = request.headers.get("X-Raises-Signature");
  if (!timestamp || !signature?.startsWith("v1=")) return false;

  const seconds = Number(timestamp);
  if (!Number.isFinite(seconds) || Math.abs(Date.now() / 1000 - seconds) > MAX_AGE_SECONDS) return false;

  const actual = bytesFromHex(signature.slice(3));
  if (!actual) return false;
  const key = await crypto.subtle.importKey(
    "raw",
    encoder.encode(secret),
    { name: "HMAC", hash: "SHA-256" },
    false,
    ["sign"],
  );
  const expected = new Uint8Array(
    await crypto.subtle.sign("HMAC", key, encoder.encode(`${timestamp}.${body}`)),
  );
  return crypto.subtle.timingSafeEqual(actual, expected);
}

function messageFor(payload: Record<string, unknown>): string {
  const data = payload.data as Record<string, unknown> | undefined;
  const notice = data?.notice as Record<string, unknown> | undefined;
  const error = data?.error as Record<string, unknown> | undefined;
  const issue = data?.github_issue as Record<string, unknown> | undefined;
  return String(notice?.message ?? error?.message ?? error?.error_class ?? issue?.title ?? payload.type ?? "Raises event").slice(0, 90);
}

function compact(payload: Record<string, unknown>): PikoEvent {
  const project = payload.project as Record<string, unknown> | undefined;
  return {
    id: String(payload.id ?? crypto.randomUUID()),
    type: String(payload.type ?? "notice.created"),
    project: String(project?.name ?? "Unknown app").slice(0, 32),
    message: messageFor(payload),
    created_at: String(payload.created_at ?? new Date().toISOString()),
  };
}

async function state(env: Env): Promise<PikoState> {
  return (await env.PIKO.get<PikoState>("state", "json")) ?? { revision: 0, events: [] };
}

export default {
  async fetch(request: Request, env: Env): Promise<Response> {
    const url = new URL(request.url);

    if (request.method === "GET" && url.pathname === "/piko") {
      const token = request.headers.get("Authorization")?.replace(/^Bearer /, "");
      const actual = token ? encoder.encode(token) : new Uint8Array();
      const expected = encoder.encode(env.PIKO_DEVICE_TOKEN);
      if (actual.byteLength !== expected.byteLength || !crypto.subtle.timingSafeEqual(actual, expected)) {
        return json({ error: "unauthorized" }, 401);
      }
      return json(await state(env));
    }

    if (request.method === "POST" && url.pathname === "/raises") {
      const contentLength = Number(request.headers.get("content-length") ?? 0);
      if (contentLength > MAX_BODY_BYTES) return json({ error: "body too large" }, 413);
      const body = await request.text();
      if (encoder.encode(body).byteLength > MAX_BODY_BYTES) return json({ error: "body too large" }, 413);
      if (!(await validRaisesSignature(request, env.RAISES_SIGNING_SECRET, body))) {
        return json({ error: "invalid signature" }, 401);
      }

      let payload: Record<string, unknown>;
      try {
        payload = JSON.parse(body) as Record<string, unknown>;
      } catch {
        return json({ error: "invalid JSON" }, 400);
      }
      if (payload.type !== "webhook.test") {
        const current = await state(env);
        if (!current.events.some((event) => event.id === payload.id)) {
          current.events.unshift(compact(payload));
          current.events = current.events.slice(0, MAX_EVENTS);
          current.revision += 1;
          await env.PIKO.put("state", JSON.stringify(current));
        }
      }
      return json({ accepted: true });
    }

    return new Response("Not found", { status: 404 });
  },
} satisfies ExportedHandler<Env>;
