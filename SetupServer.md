# Setting Up a Frozen Bubble Multiplayer Server

This guide walks through running a public server that supports both:

- **Native clients** (desktop, Android) — connect via raw TCP on port **1511**
- **Browser clients** (itch.io / WASM) — connect via secure WebSocket (`wss://`) on port **443**

---

## Requirements

| What | Why |
|---|---|
| A domain name pointing to your server | **Required for browser clients.** Browsers block unencrypted WebSocket connections (`ws://`) from HTTPS pages. You need a real domain so Let's Encrypt can issue a valid TLS certificate. A plain IP address will not work for browser players. |
| A Linux VPS | Any provider (DigitalOcean, Linode, Hetzner, etc.) |
| Ports 80, 443, 1511 open | In your VPS firewall / security group |
| Docker + Docker Compose installed | [Install Docker](https://docs.docker.com/engine/install/) |

> **Browser clients only work with a valid SSL certificate on a real domain.**
> Self-signed certificates will be rejected by browsers. Native desktop and Android
> clients are not affected — they connect on port 1511 without SSL.

---

## Setup

### 1. Clone the repo

```bash
git clone https://github.com/dchau360/frozen-bubble-sdl2.git
cd frozen-bubble-sdl2
```

### 2. Get a free SSL certificate

Run certbot **before** starting Docker so port 80 is free:

```bash
sudo apt install certbot
sudo certbot certonly --standalone -d yourdomain.com
```

Replace `yourdomain.com` with your actual domain. Certbot will verify ownership
over port 80 and write the certificate files to
`/etc/letsencrypt/live/yourdomain.com/`.

### 3. Copy the certificates into place

```bash
sudo cp /etc/letsencrypt/live/yourdomain.com/fullchain.pem docker/ssl/fullchain.pem
sudo cp /etc/letsencrypt/live/yourdomain.com/privkey.pem   docker/ssl/privkey.pem
```

### 4. Start the server

```bash
cd docker
./setup.sh
```

This builds fb-server from source and starts both the game server and the nginx
TLS proxy. To run in the background:

```bash
./setup.sh -d
```

To stop:

```bash
docker compose down
```

---

## Renewing the Certificate

Let's Encrypt certificates expire after 90 days. To renew:

```bash
docker compose down                    # free port 80
sudo certbot renew
sudo cp /etc/letsencrypt/live/yourdomain.com/fullchain.pem docker/ssl/fullchain.pem
sudo cp /etc/letsencrypt/live/yourdomain.com/privkey.pem   docker/ssl/privkey.pem
cd docker && ./setup.sh -d
```

---

## How Players Connect

| Client | Host | Port |
|---|---|---|
| Desktop / Android (native) | `yourdomain.com` | `1511` |
| Browser (itch.io / WASM) | `yourdomain.com` | `443` |

In the game: **Net Game** → enter the host and port above → **Connect**.

---

## Adding Your Server to the Public List

The game fetches a community server list from
[github.com/dchau360/frozen-bubble-servers](https://github.com/dchau360/frozen-bubble-servers)
at startup. Submit a pull request to add your server so players can find it
automatically.

The list supports one entry per port, so a server with both native TCP and
browser WebSocket support should be listed twice — once per port:

```
# host:port  Display name
yourdomain.com:1511  Your Server Name (desktop/Android)
yourdomain.com:443   Your Server Name (browser)
```

Native clients will show both entries; browser clients currently need to enter
the host and port manually (public server list auto-detection for browser clients
is not yet implemented).

To submit:

1. Fork [dchau360/frozen-bubble-servers](https://github.com/dchau360/frozen-bubble-servers)
2. Add your entries to `serverlist-1`
3. Open a pull request with your server's hostname and a brief description

---

## Local Testing (No Domain)

The `setup.sh` script generates a self-signed certificate automatically if no
valid certificate is found in `docker/ssl/`. This lets you verify the server is
running, but browser clients will reject the self-signed cert.

To test locally with native clients only:

```bash
./build/server/fb-server -q -l -z    # port 1511, no Docker needed
```
