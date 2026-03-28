# Setting Up a Frozen Bubble Multiplayer Server

This guide walks through running a public server that supports both:

- **Native clients** (desktop, Android) — raw TCP on port **1511**
- **Browser clients** (itch.io / WASM) — secure WebSocket (`wss://`) on port **443**

> **Browser clients require a valid SSL certificate on a real domain name.**
> A plain IP address or self-signed certificate will be rejected by browsers.
> Native clients are unaffected — they connect on port 1511 without SSL.

The free path covered here uses **Oracle Cloud** (free VPS) + **No-IP** (free domain) +
**Let's Encrypt** (free SSL certificate).

---

## Step 1 — Create a Server (Oracle Cloud Free Tier)

Oracle Cloud's Always Free tier includes 2 AMD VMs (1 OCPU, 1 GB RAM) — more than
enough for fb-server.

1. Sign up at [cloud.oracle.com](https://cloud.oracle.com)
2. Create an instance: **Compute → Instances → Create Instance**
   - Image: **Ubuntu 22.04**
   - Shape: **VM.Standard.E2.1.Micro** (Always Free)
3. Note the instance's **Public IP address** — you'll need it in Step 2

**Open ports in the VCN security list:**

Go to **Networking → Virtual Cloud Networks → your VCN → Security Lists → Default**
and add ingress rules for TCP ports **80**, **443**, and **1511**.

**Also open ports in the OS firewall** (Oracle images block ports at the OS level
even after the security list is updated — this catches a lot of people):

```bash
sudo iptables -I INPUT -p tcp --dport 80 -j ACCEPT
sudo iptables -I INPUT -p tcp --dport 443 -j ACCEPT
sudo iptables -I INPUT -p tcp --dport 1511 -j ACCEPT
sudo netfilter-persistent save
```

> Using a different VPS provider? Just ensure ports 80, 443, and 1511 are open in
> your provider's firewall / security group and skip the iptables step unless your
> OS firewall also blocks them.

---

## Step 2 — Get a Free Domain (No-IP)

Now that you have your server's public IP, point a domain at it.
[No-IP](https://www.noip.com) offers free dynamic DNS hostnames (e.g. `myfbserver.ddns.net`)
that work with Let's Encrypt.

1. Create a free account at [noip.com](https://www.noip.com)
2. Go to **Dynamic DNS → No-IP Hostnames → Create Hostname**
3. Choose a hostname and enter your server's public IP from Step 1
4. Install the No-IP Dynamic Update Client on your server so the hostname stays
   current if your IP ever changes:

```bash
sudo apt install noip2
sudo noip2 -C          # enter your No-IP credentials when prompted
sudo systemctl enable noip2 --now
```

> Free No-IP hostnames require confirmation every 30 days to stay active —
> you'll receive an email reminder.

Use your No-IP hostname (e.g. `myfbserver.ddns.net`) anywhere this guide
refers to `yourdomain.com`.

> Already have a paid domain? Skip this step and point your DNS A record at
> the server's public IP instead.

---

## Step 3 — Install Docker

```bash
sudo apt install -y ca-certificates curl
sudo install -m 0755 -d /etc/apt/keyrings
curl -fsSL https://download.docker.com/linux/ubuntu/gpg | sudo tee /etc/apt/keyrings/docker.asc >/dev/null
sudo chmod a+r /etc/apt/keyrings/docker.asc
echo "deb [arch=$(dpkg --print-architecture) signed-by=/etc/apt/keyrings/docker.asc] https://download.docker.com/linux/ubuntu $(. /etc/os-release && echo ${UBUNTU_CODENAME:-$VERSION_CODENAME}) stable" | sudo tee /etc/apt/sources.list.d/docker.list >/dev/null
sudo apt update
sudo apt install -y docker-ce docker-ce-cli containerd.io docker-compose-plugin
sudo systemctl enable docker --now
sudo usermod -aG docker $USER   # lets you run docker without sudo (re-login after)
```

---

## Step 4 — Clone the Repo

```bash
git clone https://github.com/dchau360/frozen-bubble-sdl2.git
cd frozen-bubble-sdl2
```

---

## Step 5 — Get a Free SSL Certificate (Let's Encrypt)

Run certbot **before** starting Docker so port 80 is free:

```bash
sudo apt install certbot
sudo certbot certonly --standalone -d yourdomain.com
```

On first run certbot will:
1. Ask for an **email address** — used for expiry reminders and account recovery
2. Ask you to agree to the **Let's Encrypt Terms of Service**
3. Verify domain ownership over port 80
4. Write the certificate to `/etc/letsencrypt/live/yourdomain.com/`

---

## Step 6 — Copy the Certificates into Place

```bash
sudo cp /etc/letsencrypt/live/yourdomain.com/fullchain.pem docker/ssl/fullchain.pem
sudo cp /etc/letsencrypt/live/yourdomain.com/privkey.pem   docker/ssl/privkey.pem
```

---

## Step 7 — Start the Server

```bash
cd docker
./setup.sh -d          # -d runs in background
```

This builds fb-server from source and starts both the game server and the nginx
TLS proxy. To stop:

```bash
docker compose down
```

---

## How Players Connect

| Client | Host | Port |
|---|---|---|
| Desktop / Android (native) | `yourdomain.com` | `1511` |
| Browser (itch.io / WASM) | `yourdomain.com` | `443` |

In the game: **Net Game** → enter the host and port above → **Connect**.

---

## Renewing the Certificate

Let's Encrypt certificates expire after 90 days. To renew:

```bash
cd docker && docker compose down     # free port 80
sudo certbot renew
sudo cp /etc/letsencrypt/live/yourdomain.com/fullchain.pem docker/ssl/fullchain.pem
sudo cp /etc/letsencrypt/live/yourdomain.com/privkey.pem   docker/ssl/privkey.pem
./setup.sh -d
```

---

## Adding Your Server to the Public List

The game fetches a community server list from
[github.com/dchau360/frozen-bubble-servers](https://github.com/dchau360/frozen-bubble-servers)
at startup. Submit a pull request to add your server so players can find it automatically.

List both ports so native and browser players can discover your server:

```
# host:port  Display name
yourdomain.com:1511  Your Server Name (desktop/Android)
yourdomain.com:443   Your Server Name (browser)
```

To submit:

1. Fork [dchau360/frozen-bubble-servers](https://github.com/dchau360/frozen-bubble-servers)
2. Add your entries to `serverlist-1`
3. Open a pull request with your server's hostname and a brief description

---

## Local Testing (No Domain)

The `setup.sh` script generates a self-signed certificate automatically if no
valid certificate is found in `docker/ssl/`. This lets you verify the server is
running, but browser clients will reject the self-signed cert.

To test with native clients only (no Docker needed):

```bash
./build/server/fb-server -q -l -z    # port 1511
```
