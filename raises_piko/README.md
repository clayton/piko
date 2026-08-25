# Raises Piko

Piko becomes a tiny incident creature. Raises webhooks change his mood, while the ESP32 polls a compact private feed from a Cloudflare Worker.

## Moods

- No events: green, relaxed, "ALL CLEAR"
- `notice.created`: yellow, curious, "A NOTE ARRIVED"
- New errors and GitHub issues: orange, worried, "I FOUND A BUG"
- Regressions and reopened issues: red, shaking, "OH NO, IT CAME BACK"

The top button cycles recent events. A short press of the bottom button returns to the newest event. The AXP2101 keeps its native long-press power-off behavior.

## Bridge

Raises requires a public HTTPS webhook and signs every delivery. The Worker:

1. verifies the timestamped HMAC-SHA256 signature;
2. keeps the newest 12 compact events in KV;
3. exposes them to Piko behind a separate bearer token.

It does not expose backtraces or full notice payloads to the device.

### Deploy the Worker

```sh
cd raises_piko/worker
npm install
npx wrangler kv namespace create PIKO
```

Replace `PLACEHOLDER` in `wrangler.jsonc` with the returned KV ID. Then set both secrets interactively:

```sh
npx wrangler secret put RAISES_SIGNING_SECRET
npx wrangler secret put PIKO_DEVICE_TOKEN
npm run deploy
```

Create a Raises webhook endpoint for:

```text
https://piko-raises-bridge.<your-workers-subdomain>.workers.dev/raises
```

Subscribe to `notice.created`, `error.created`, `error.regressed`, `github_issue.opened`, and `github_issue.reopened`. Store the one-time Raises signing secret as `RAISES_SIGNING_SECRET`.

### Flash Piko

```sh
cp raises_piko/config.h.example raises_piko/config.h
# Edit Wi-Fi, Worker URL, and the matching device token.
make raises-flash
```

`raises_piko/config.h` is ignored by Git.
