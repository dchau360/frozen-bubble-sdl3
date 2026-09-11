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
git clone https://github.com/dchau360/frozen-bubble-sdl3.git
cd frozen-bubble-sdl3
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

## Step 6 — Link the Certificates into Place

```bash
sudo tools/link-fb-certs.sh yourdomain.com
```

`tools/link-fb-certs.sh` (checked into the repo) symlinks `docker/ssl/*.pem`
to `/etc/letsencrypt/live/yourdomain.com/*.pem` rather than copying them:
that `live/` path is itself a symlink into `archive/`, and `certbot renew`
repoints it to the freshly renewed file without ever changing the `live/`
path itself. Symlinking `docker/ssl/*.pem` to it means renewal never needs a
copy step again — the script also recreates the nginx container so the
newly-linked cert actually gets served (Docker resolves a bind-mounted
symlink once, at container creation, so a running nginx keeps serving
whatever cert was live when *it* started even after the symlink is
repointed). It's idempotent, so running it again later (e.g. after a
renewal) is always safe — see the script's own header for details. Domain is
a required argument; there's no default, since this script is shared across
every hoster's own server.

---

## Step 7 — Start the Server

```bash
cd docker
./setup.sh -d          # -d runs in background
```

This builds fb-server from source and starts the game server, the nginx TLS
proxy, and the Discord join-alert relay. To stop:

```bash
docker compose down
```

---

## Optional — Discord Join & Result Alerts

Every time a player arrives on your server, it can post a message to a
Discord channel of your choosing — their nick and your server's name, and
nothing else. The alert fires when they connect and appear in the lobby,
not when they join a game room: someone waiting alone in a room they just
opened is the person an alert should bring company to, and once a second
player has joined them the notification has nothing left to offer. Neither the player's IP nor their
self-reported location is included, deliberately: a Discord channel can
have members well beyond whoever runs the server, and the game itself now
invites players into a community Discord from its own UI, so an alert
channel is a good deal more public than it used to be.

The same relay also posts a message at the end of every round: the game
mode, who won (or that it was a draw), and the full player roster. It uses
the same webhook, the same `DISCORD_SERVER_NAME` override, and the same
stub/live modes below — there is nothing extra to configure. See
[Round-result alerts](#round-result-alerts) further down for the details
and what it does and doesn't include.

This works out of the box in a **stub mode** that logs what it would have
posted but delivers nothing. Making alerts actually arrive needs a Discord
webhook URL, which only you as the operator can obtain.

> **Name your server first.** The alert says *"alice joined **&lt;your server&gt;**"*,
> and that name comes from `fb-server`'s `-n` flag. Without it the name falls
> back to the hostname, which inside a container is the container ID — your
> alerts read `alice joined ce98fdda68c6`, and the ID changes every rebuild.
> Set it in `docker/.env`:
>
> ```bash
> FB_SERVER_NAME=myserver
> ```
>
> **`-n` caps out at 12 characters, `[a-zA-Z0-9.-]` only, and `fb-server`
> refuses to start at all if it's violated** — not a warning, a hard exit,
> which under `restart: unless-stopped` means a crash loop. A domain like
> `fb.example.org` (14 chars) is already too long; something short and
> recognizable is safer than the domain itself. Confirm it took with
> `docker compose logs fb-server | grep Servername`.
>
> The same name is what your server advertises to the public server list, so
> this is worth setting whether or not you use Discord alerts at all.
>
> **Want the real name in Discord anyway?** `-n`'s limit only constrains
> what's advertised in-game — the Discord side of the pipe has no such cap.
> Set `DISCORD_SERVER_NAME` in `docker/.env` and every alert shows that
> instead, with `-n` untouched:
>
> ```bash
> DISCORD_SERVER_NAME=fb.example.org
> ```
>
> See [server/discord-relay/README.md](server/discord-relay/README.md#live-delivery).

**Nothing breaks if you skip this.** Joins still happen normally; the alert
just never fires. The relay is also entirely optional — remove
`FB_SERVER_DISCORD_RELAY` from the `fb-server` service to turn the feature
off.

### Checking the stub

```bash
docker compose logs discord-relay
```

A join logs a line like:

```
[stub] would post: 🔔 **alice** joined **myserver**
```

A round ending logs one too:

```
[stub] would post: 🏆 **alice** won (Race) on **myserver** — alice, bob
```

### Going live

In Discord: Server Settings → Integrations → Webhooks → New Webhook, pick the
channel it should post to, and copy its URL. Set it in a `.env` file next to
`docker-compose.yml`:

```bash
DISCORD_WEBHOOK_URL=https://discord.com/api/webhooks/...
```

Then `docker compose up -d --build discord-relay`. That one URL is both the
credential and the channel selector — create a different webhook to alert a
different channel.

The webhook does not have to be one you made. If a community channel has
issued you a URL so your server's joins show up alongside everyone else's,
set that as `DISCORD_WEBHOOK_URL` here and nothing else changes. Running
your own channel and posting to someone else's are the same one-variable
setup — see
[server/discord-relay/README.md](server/discord-relay/README.md) if you are
on the issuing end.

A burst of joins can hit Discord's per-webhook rate limit; a request that
gets rate-limited is logged and dropped rather than queued or retried.

The message carries neither an IP address nor a location, but it does carry
a nick, and a running stream of them says who plays on your server and when
— keep the webhook URL somewhere sensible all the same. Anyone who has it
can post anything to that channel, not just what this relay sends.

> **The in-game "Join our Discord" row is not this.** The NET GAME list and
> the online lobby each offer players a link to the game's *community*
> Discord, which is a fixed URL compiled into the client — it has nothing to
> do with your webhook and does not point at your channel. If you want your
> own players in your own Discord, advertise it yourself; there is no way
> for a server to change where that in-game link goes, deliberately (see
> `kDiscordInviteUrl` in `src/platform.cpp`).

### Round-result alerts

Posted once per round — when someone claims the win (or the round ends in a
draw), not once per full match. There's no reliable way to tell server-side
when a "match" (best-of-N by whatever win count a room's players agreed on)
is truly over, since that count only ever lives client-side and can differ
room to room, so this posts at the same granularity already shown to
players in the post-round stats table: one alert per round, saying who won,
which game mode it was played in (Classic/Clear/Race/Timed — unlabelled if
a room never set one), and every player who was in the room.

**The winner name is not verified.** It's exactly what the reporting
client's own message said, the same way the round-over notice every other
player's screen shows is. A modified client could in principle claim a win
it didn't earn; the roster next to it, by contrast, is always accurate,
since that comes from the server's own player list, not anything a client
sent. This alert does not, and cannot, replace the winner claim any client
already displays — it's the same information, on Discord.

**A rage-quit or dropped connection is never posted as a loss.** The server
already infers a win/loss from a player leaving mid-round for its own
internal stats, but that inference is unreliable — there's no way to tell a
rage-quit from a connection drop — and misattributing an outcome to a named
player in public would be worse than just not posting one.

**This is independent of the in-game lobby announcement.** Every server, not
just one with a relay configured, also posts the same round's result as
ordinary lobby chat — visible to anyone sitting in the lobby, not players off
in another room — with the round's win-count and top-5 scorers, and (on a
team win) the team and everyone on it. Nothing to configure: it fires from
the same round-end regardless of whether `FB_SERVER_DISCORD_RELAY` is even
set. See `CLAUDE.md`'s "Round-result lobby broadcast" for the details.

> **One thread per room, not one message per round.** By default a busy
> server's round-results are flat top-level messages, same as a join alert —
> fine at first, but a long-lived room can clutter the channel with one
> message per round. Set `DISCORD_BOT_TOKEN` and `DISCORD_CHANNEL_ID` and
> every room's first result opens a Discord thread named after that room;
> every later round for the same room posts into it instead. Join alerts are
> unaffected either way — they always stay flat, since a join isn't part of
> any one room's result history.
>
> This needs a Discord **bot**, not just the webhook above: Discord's
> webhook API can only create a *new* thread when the webhook's channel is a
> forum/media channel, and this relay is designed to share an ordinary text
> channel with join alerts. To set it up:
>
> 1. [Create a Discord Application](https://discord.com/developers/applications) →
>    **Bot** tab → **Reset Token** (or copy the existing one) → this is
>    `DISCORD_BOT_TOKEN`.
> 2. **OAuth2** tab → URL Generator → scope `bot` → permissions **View
>    Channel**, **Send Messages**, **Create Public Threads**, **Send
>    Messages in Threads** → open the generated URL and add the bot to your
>    server.
> 3. In Discord, right-click the channel you want round-results threaded in
>    → **Copy Channel ID** (enable Developer Mode under Settings → Advanced
>    if that option isn't there) → this is `DISCORD_CHANNEL_ID`.
> 4. Add both to `docker/.env`:
>
> ```bash
> DISCORD_BOT_TOKEN=your-bot-token-here
> DISCORD_CHANNEL_ID=123456789012345678
> ```
>
> Then `docker compose up -d --build discord-relay`. Setting only one of the
> two logs a warning and falls back to flat result alerts via the webhook,
> the same as setting neither.
>
> A relay restart forgets which rooms already have a thread open — the next
> result for a room already in progress just opens a new one, no worse than
> every room got before this existed.

See [server/discord-relay/README.md](server/discord-relay/README.md) for the
protocol and internals.

---

## Optional — Limit Concurrent Bots

Clients can add bots — either in a local game or, on the server, in a network
room — and a bot that joins your server identifies itself with a `BOT`
command right after connecting. Unlike a person, a bot costs you CPU on every
shot it takes (level generation, malus, chain reactions all run per board),
so if that is a resource concern, cap how many can be registered at once,
server-wide:

```yaml
    command: ["-d", "-q", "-l", "-z", "-o", "CONNECT", "-b", "20"]
```

Add that under the `fb-server` service in `docker-compose.yml` — it overrides
the image's default command (`docker/Dockerfile`'s `CMD`), so include every
flag you want, not just `-b`. `-b 0` refuses every bot outright; the default
without this override is 20. Once the cap is reached, a further bot's `BOT`
command gets `BOT_LIMIT_REACHED` and the client shows the player a message —
the connection itself is unaffected, so a capped-out bot is simply not a bot:
it can still join and play as an ordinary person would. `docker compose up -d
--build fb-server` to apply.

This is a global cap across every room on the server, not a per-room one —
clients already limit a single room to 4 bots on their own, but nothing stops
a modified client from opening more connections and registering each as a
bot, which is exactly the case this flag is for.

---

## Online Tournaments

The bundled `fb-server` also coordinates online tournaments. There is nothing to enable and no extra port or flag — it's built into the same binary and the same ports (1511 / 443) already covered above. Players create and join brackets entirely from the client's own online lobby.

### Resolving a disputed match

Each tournament match is best of three, and both players report the result themselves. If they report different winners, the match goes **disputed** and stays that way — the server never guesses, and there is no way for either player to retry on their own. Settling it takes a `RESOLVE` command sent over a connection the server has already marked **admin-authorized**.

Admin authorization here isn't a login or a password: `server/net.c` grants it automatically, once, at accept time, to any connection whose source address is exactly `127.0.0.1` (the only other command gated on it today is `ADMIN_REREAD`, an internal config-reload command — a room host's own `/kick` is a separate, unrelated permission and does not require this).

Because of Docker's networking, connecting to your published port from the host machine's own `localhost` does **not** qualify — Docker's NAT rewrites the source address before it reaches the container, so it never arrives as literally `127.0.0.1` from `fb-server`'s point of view. The connection has to originate *inside* the `fb-server` container itself:

```bash
docker compose exec fb-server bash
```

The runtime image has no `nc` or `python3` installed, but bash's own `/dev/tcp` pseudo-device is enough to speak the server's raw line protocol directly:

```bash
exec 3<>/dev/tcp/127.0.0.1/1511
head -1 <&3                                            # wait for SERVER_READY
printf 'FB/1.3 TOUR RESOLVE <tid> <mid> <winnerPid>\n' >&3
head -1 <&3                                            # read the reply
exec 3<&- 3>&-
```

(Any TCP client works the same way — the only requirement is that it run inside the container. Adjust the exact commands for whatever shell/tools you have available.)

`<tid>` and `<mid>` identify the tournament and the disputed match. A player will see both on their own bracket screen — `TOURNAMENT #<tid>` in the header, `#<mid>` on the disputed match's card, and "Awaiting operator" in their own match status — so ask whoever reported the dispute to pass them along, or connect the same way and send `TOUR STATE <tid>` to read the bracket yourself. `<winnerPid>` is either `0` — replay the disputed round without awarding anyone a win — or the numeric entrant ID of whichever of the two assigned players actually won, which commits that round's win to them. A malformed command, a winner ID that isn't one of the two assigned players, or the same command sent from a connection that isn't admin-authorized is simply denied; it also has no effect on a match that isn't currently disputed.

---

## How Players Connect

| Client | Host | Port |
|---|---|---|
| Desktop / Android (native) | `yourdomain.com` | `1511` |
| Browser (itch.io / WASM) | `yourdomain.com` | `443` |

In the game: **Net Game** → enter the host and port above → **Connect**.

---

## Abuse Reports

Players can report each other with `/report <nick> <reason>` in chat. Reports
are appended to a file for you to read; **nothing is acted on automatically**,
by design — nicks are chosen fresh on every connect and are not tied to any
account, so auto-kicking on report would hand every player a way to remove
anyone they liked.

```bash
docker compose exec fb-server cat /var/lib/fb-server/reports.log
```

Each line records the time, the reporter's nick and IP, who they reported, and
the reason. Cross-reference the IP against `joiners.log` in the same directory
if you need to identify someone across nick changes.

Override the location with `FB_SERVER_REPORT_FILE` if you want it elsewhere.
Acting on a report is entirely up to you as the operator — the game has no
built-in ban mechanism, so blocking at the firewall (or simply not running an
open server) is what enforcement looks like today.

Players also have `/block <nick>`, which hides someone's chat immediately and
entirely client-side — it needs nothing from you and works even if you never
read the report log.

---

## Updating the Server

A one-off update — pull the latest code, rebuild, and restart — is just:

```bash
cd ~/frozen-bubble-sdl3 && git pull && cd docker && docker compose up --build -d
```

But that alone doesn't touch your SSL certificate, which still needs
renewing every ~60 days (see below) and re-linking after every renewal so
nginx actually serves the new one. `tools/update-server.sh` (checked into
the repo) does both jobs — code update *and* cert renewal/relink — in one
script, safe to run by hand or on a schedule. It's a **template**: copy it
outside your checkout before using it (the script explains why in its own
header — in short, a script shouldn't `git pull` the repo it's currently
running out of):

```bash
cp ~/frozen-bubble-sdl3/tools/update-server.sh ~/update-server.sh
chmod +x ~/update-server.sh
sudo ~/update-server.sh yourdomain.com
```

That single run pulls `main`, rebuilds and restarts `fb-server` and
`discord-relay`, runs `certbot renew` (a no-op unless the cert is within 30
days of expiry), and calls `tools/link-fb-certs.sh` to relink and serve
whatever cert is currently live. Read the script's own header comment for
the full flag list (`--no-pull`, `--no-renew`, `--if-due`) and for how to
schedule it (e.g. daily via cron with `--if-due`, so it renews the cert
automatically without ever double-renewing or drifting off a fixed
interval) — it's all documented there rather than duplicated here.

---

## Renewing the Certificate

Let's Encrypt certificates expire after 90 days; renew any time within the
last 30 days of that window. If you're using `tools/update-server.sh` (see
above) on a schedule, this already happens automatically and you can skip
this section. To do it manually:

```bash
sudo certbot renew
sudo tools/link-fb-certs.sh yourdomain.com   # relinks + recreates nginx to serve it
```

`certbot renew` alone is not enough — see Step 6 above for why nginx keeps
serving the old cert until the container is recreated, which is what
`link-fb-certs.sh` handles.

Verify the renewal actually took effect before trusting it:

```bash
openssl x509 -in docker/ssl/fullchain.pem -noout -enddate
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
