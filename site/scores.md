# World highscores

The best classic single-player runs from Frozen Bubble: SDL3 players, live
from this port's own server at fb.servequake.com. (This is a fan-made port;
the board is not run by the original Frozen Bubble authors.) A run counts from level 1 of the
standard 100 levels: the furthest level cleared wins, and the faster time
breaks a tie. "This week" starts over every Monday at 00:00 UTC.

Keyboard/gamepad and mouse/touch runs are ranked separately, since aiming
with one is not the same game as aiming with the other. Players are listed as
`nick#tag`: the tag comes from the player's anonymous account, so two players
with the same nickname stay apart. Scores are sent by each player's game and
are not verified.

<div id="fb-scores">
<p id="fb-tabs">
<button type="button" data-track="0" aria-pressed="true">Keyboard / gamepad</button>
<button type="button" data-track="1" aria-pressed="false">Mouse / touch</button>
</p>
<p id="fb-status" class="effective-date">Loading…</p>
<h2>All-time</h2>
<table><thead><tr><th>#</th><th>Player</th><th>Reached</th><th>Time</th></tr></thead><tbody id="fb-alltime"></tbody></table>
<h2 id="fb-week-title">This week</h2>
<table><thead><tr><th>#</th><th>Player</th><th>Reached</th><th>Time</th></tr></thead><tbody id="fb-week"></tbody></table>
</div>

<style>
#fb-tabs button { font: inherit; padding: .35rem .8rem; margin-right: .4rem; border: 1px solid var(--border); border-radius: 6px; background: var(--code-bg); color: var(--fg); cursor: pointer; }
#fb-tabs button[aria-pressed="true"] { border-color: var(--link); color: var(--link); font-weight: 600; }
#fb-scores td:first-child, #fb-scores th:first-child { width: 2.5rem; }
</style>

<script>
(function () {
  // The board lives on the port's own server (fb-server, protocol 1.7:
  // server/hiscores.h). Browsers reach it the way the web build of the game
  // does, over a secure WebSocket; the server answers in binary frames.
  var SERVER = "wss://fb.servequake.com/";
  var boards = [null, null], track = 0, weekStart = 0;
  var statusEl = document.getElementById("fb-status");
  function levelLabel(l) { return l > 100 ? "all 100 levels" : "level " + l; }
  function timeLabel(ms) {
    var t = Math.floor(ms / 1000), h = Math.floor(t / 3600), m = Math.floor(t / 60) % 60, s = t % 60;
    var mm = (h ? String(m).padStart(2, "0") : m) + "'" + String(s).padStart(2, "0") + '"';
    return h ? h + ":" + mm : mm;
  }
  function parseList(field) {
    if (field === "-") return [];
    return field.split(",").map(function (item) {
      var eq = item.lastIndexOf("="), slash = item.lastIndexOf("/");
      return { name: item.slice(0, eq), level: +item.slice(eq + 1, slash), ms: +item.slice(slash + 1) };
    });
  }
  function fill(id, list) {
    var body = document.getElementById(id);
    body.textContent = "";
    if (!list.length) {
      var tr = body.insertRow(), td = tr.insertCell();
      td.colSpan = 4; td.textContent = "Nobody yet.";
      return;
    }
    list.forEach(function (e, i) {
      var rank = i + 1;
      while (rank > 1 && list[rank - 2].level === e.level && list[rank - 2].ms === e.ms) rank--;
      var tr = body.insertRow();
      [rank, e.name, levelLabel(e.level), timeLabel(e.ms)].forEach(function (v) {
        tr.insertCell().textContent = v;
      });
    });
  }
  function render() {
    document.querySelectorAll("#fb-tabs button").forEach(function (b) {
      b.setAttribute("aria-pressed", String(+b.dataset.track === track));
    });
    var b = boards[track];
    if (!b) return;
    fill("fb-alltime", b.alltime);
    fill("fb-week", b.week);
    if (weekStart) {
      var d = new Date(weekStart * 1000);
      document.getElementById("fb-week-title").textContent =
        "This week (since " + d.toLocaleDateString(undefined, { month: "short", day: "numeric", timeZone: "UTC" }) + ")";
    }
  }
  document.querySelectorAll("#fb-tabs button").forEach(function (b) {
    b.addEventListener("click", function () { track = +b.dataset.track; render(); });
  });
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
        ws.send("FB/1.3 HISCORES 0\n");
        ws.send("FB/1.3 HISCORES 1\n");
      }
      var m = line.match(/^FB\/1\.\d+ HISCORES: (.*)$/);
      if (m) {
        var f = m[1].split(" ");
        if (f.length >= 4) {
          weekStart = +f[0];
          boards[got] = { alltime: parseList(f[1]), week: parseList(f[2]) };
        }
        if (++got === 2) { statusEl.textContent = "Live from fb.servequake.com."; ws.close(); }
        render();
      }
    }
  };
  ws.onerror = function () { if (got < 2) statusEl.textContent = "Couldn't reach the server. Try again later."; };
})();
</script>

Want to be on it? Get the game on the [home page](../), play a classic game
from level 1, and your furthest level is sent when you're back in the menu.
Players can turn this off with **World highscores** in the 1-player menu; see
the [privacy policy](../privacy/#world-highscores).
