<h1>FROZEN-BUBBLE</h1>
<p class="sub">World highscores · classic 100 levels</p>

<div class="controls">
<span class="pill" role="group" aria-label="Board">
<button type="button" data-board="0" aria-pressed="true">Furthest level</button>
<button type="button" data-board="1" aria-pressed="false">Most points</button>
</span>
<span class="pill" role="group" aria-label="Period">
<button type="button" data-scope="alltime" aria-pressed="true">All-time</button>
<button type="button" data-scope="week" aria-pressed="false">This week</button>
</span>
<span class="pill" role="group" aria-label="Controls">
<button type="button" data-track="0" aria-pressed="true">Keyboard / gamepad</button>
<button type="button" data-track="1" aria-pressed="false">Mouse / touch</button>
</span>
</div>

<div class="panel"><table><thead id="fb-head"></thead><tbody id="fb-body"><tr><td class="empty">Loading…</td></tr></tbody></table></div>
<p id="fb-status" class="status">Connecting to fb.servequake.com…</p>

<div class="about" markdown="1">

**Furthest level** ranks runs from level 1 of the standard 100 levels: the
furthest level cleared wins, and the faster time breaks a tie. **Most points**
ranks the most points scored in one life. The score starts over at every
death, and the level shown is the one that life got to. "This week" starts
over every Monday at 00:00 UTC.

Keyboard/gamepad and mouse/touch runs are ranked separately, since aiming
with one is not the same game as aiming with the other. Players are listed as
`nick#tag`: the tag comes from the player's anonymous account, so two players
with the same nickname stay apart. Scores are sent by each player's game and
are not verified.

The board lives on this port's own server at fb.servequake.com. Frozen Bubble:
SDL3 is a fan-made port, and the board is not run by the original Frozen
Bubble authors.

Want to be on it? Get the game on the [home page](../), play a classic game
from level 1, and your results are sent when you're back in the menu.
Players can turn this off with **World highscores** in the 1-player menu; see
the [privacy policy](../privacy/#world-highscores).

</div>

<script>
(function () {
  // The boards live on the port's own server (fb-server, protocol 1.7:
  // server/hiscores.h). Browsers reach it the way the web build of the game
  // does, over a secure WebSocket; the server answers in binary frames.
  // Boards 0/1 are furthest level (keyboard/mouse), 2/3 most points.
  var SERVER = "wss://fb.servequake.com/";
  var BOARDS = 4;
  var boards = [], weekStart = 0;
  var view = { kind: 0, scope: "alltime", track: 0 };
  // "#points" (and "#mouse") open straight onto that board.
  var hash = location.hash.replace("#", "").split("-");
  if (hash.indexOf("points") >= 0) view.kind = 1;
  if (hash.indexOf("mouse") >= 0) view.track = 1;
  var statusEl = document.getElementById("fb-status");
  function levelLabel(l) { return l > 100 ? "all 100" : String(l); }
  function timeLabel(ms) {
    var t = Math.floor(ms / 1000), h = Math.floor(t / 3600), m = Math.floor(t / 60) % 60, s = t % 60;
    var mm = (h ? String(m).padStart(2, "0") : m) + "'" + String(s).padStart(2, "0") + '"';
    return h ? h + ":" + mm : mm;
  }
  function pointsLabel(p) { return p.toLocaleString("en-US") + " pts"; }
  function parseList(field) {
    if (field === "-") return [];
    return field.split(",").map(function (item) {
      var eq = item.lastIndexOf("="), v = item.slice(eq + 1).split("/");
      return { name: item.slice(0, eq), level: +v[0], ms: +v[1], points: +(v[2] || 0) };
    });
  }
  function same(a, b) { return a.level === b.level && a.ms === b.ms && a.points === b.points; }
  function cell(tr, cls, text) { var td = tr.insertCell(); if (cls) td.className = cls; td.textContent = text; return td; }
  function render() {
    document.querySelectorAll("[data-board]").forEach(function (b) { b.setAttribute("aria-pressed", String(+b.dataset.board === view.kind)); });
    document.querySelectorAll("[data-scope]").forEach(function (b) { b.setAttribute("aria-pressed", String(b.dataset.scope === view.scope)); });
    document.querySelectorAll("[data-track]").forEach(function (b) { b.setAttribute("aria-pressed", String(+b.dataset.track === view.track)); });
    var points = view.kind === 1;
    var cols = points ? ["Rank", "Player", "Score", "Level", "Time"] : ["Rank", "Player", "Level", "Time"];
    var head = document.getElementById("fb-head");
    head.textContent = "";
    var hr = head.insertRow();
    cols.forEach(function (c) { var th = document.createElement("th"); th.textContent = c; if (c === "Time") th.className = "time"; hr.appendChild(th); });
    var board = boards[view.kind * 2 + view.track];
    if (!board) return;
    var list = board[view.scope], body = document.getElementById("fb-body");
    body.textContent = "";
    if (!list.length) {
      var td = cell(body.insertRow(), "empty", view.scope === "week" ? "Nobody yet this week. Be the first!" : "Nobody yet. Be the first!");
      td.colSpan = cols.length;
      return;
    }
    var ranks = [];
    list.forEach(function (e, i) {
      var rank = i && same(list[i - 1], e) ? ranks[i - 1] : i + 1;
      ranks.push(rank);
      var tr = body.insertRow();
      if (rank <= 3) tr.className = "r" + rank;
      cell(tr, "rank", "#" + rank);
      var hashAt = e.name.lastIndexOf("#");
      var name = cell(tr, "name", hashAt > 0 ? e.name.slice(0, hashAt) : e.name);
      if (hashAt > 0) {
        var tag = document.createElement("span");
        tag.className = "tag";
        tag.textContent = e.name.slice(hashAt);
        name.appendChild(tag);
      }
      if (points) {
        cell(tr, "big", pointsLabel(e.points));
        cell(tr, "", levelLabel(e.level));
      } else {
        cell(tr, "big", levelLabel(e.level));
      }
      cell(tr, "dim time", timeLabel(e.ms));
    });
  }
  function bind(attr, key, num) {
    document.querySelectorAll("[data-" + attr + "]").forEach(function (b) {
      b.addEventListener("click", function () {
        var v = b.getAttribute("data-" + attr);
        view[key] = num ? +v : v;
        render();
      });
    });
  }
  bind("board", "kind", true);
  bind("scope", "scope", false);
  bind("track", "track", true);
  render();
  function weekLabel() {
    if (!weekStart) return "";
    var d = new Date(weekStart * 1000);
    return " This week started " + d.toLocaleDateString(undefined, { month: "short", day: "numeric", timeZone: "UTC" }) + ".";
  }
  var ws, buf = "", got = 0;
  try { ws = new WebSocket(SERVER); } catch (e) { statusEl.textContent = "Couldn't reach the server."; return; }
  ws.binaryType = "arraybuffer";
  var decoder = new TextDecoder();
  ws.onmessage = function (ev) {
    buf += typeof ev.data === "string" ? ev.data : decoder.decode(ev.data, { stream: true });
    var nl;
    while ((nl = buf.indexOf("\n")) >= 0) {
      var line = buf.slice(0, nl).replace(/\r$/, ""); buf = buf.slice(nl + 1);
      if (/^FB\/1\.\d+ PUSH: SERVER_READY/.test(line)) {
        if (+line.split(" ")[0].split(".")[1] < 7) { statusEl.textContent = "The server doesn't have a world board yet."; ws.close(); return; }
        for (var b = 0; b < BOARDS; b++) ws.send("FB/1.3 HISCORES " + b + "\n");
      }
      var m = line.match(/^FB\/1\.\d+ HISCORES: (.*)$/);
      if (m) {
        var f = m[1].split(" ");
        if (f.length >= 4) {
          weekStart = +f[0];
          boards[got] = { alltime: parseList(f[1]), week: parseList(f[2]) };
        }
        if (++got === BOARDS) { statusEl.textContent = "Live from fb.servequake.com." + weekLabel(); ws.close(); }
        render();
      }
    }
  };
  ws.onerror = function () { if (got < BOARDS) statusEl.textContent = "Couldn't reach the server. Try again later."; };
})();
</script>
