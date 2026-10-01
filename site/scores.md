<h1>World highscores</h1>

<div class="tabs big" role="group" aria-label="Controls (turn on both to compare)">
<button type="button" data-track="0" aria-pressed="true">Keyboard</button>
<button type="button" data-track="1" aria-pressed="false">Mouse/Touch</button>
</div>
<div class="tabs" role="group" aria-label="Board">
<button type="button" data-board="0" aria-pressed="true">World level</button>
<button type="button" data-board="1" aria-pressed="false">World points</button>
</div>

<div class="boards">
<section class="board"><h2>ALL-TIME</h2><ol id="fb-alltime"><li class="empty">Loading…</li></ol></section>
<section class="board"><h2 id="fb-week-title">THIS WEEK</h2><ol id="fb-week"><li class="empty">Loading…</li></ol></section>
</div>
<p id="fb-status" class="status">Connecting to fb.servequake.com…</p>
<p class="note">Scores are sent by each player's game and aren't verified.</p>

<div class="about" markdown="1">

**World level** ranks runs from level 1 of the standard 100 levels: the
furthest level cleared wins, and the faster time breaks a tie. **World
points** ranks the most points scored in one life. The score starts over at
every death, and the level shown is the one that life got to. "This week"
starts over every Monday at 00:00 UTC.

Keyboard/gamepad and mouse/touch runs are ranked separately, since aiming
with one is not the same game as aiming with the other. Turn both on to see
them in one list, each run tagged <span class="badge t0">KB</span> or
<span class="badge t1">M/T</span>. Players are listed as
`nick#tag`: the tag comes from the player's anonymous account, so two players
with the same nickname stay apart.

The board lives on this port's own server at fb.servequake.com. Frozen Bubble:
SDL3 is a fan-made port, and the board is not run by the original Frozen
Bubble authors.

Want to be on it? Get the game on the [home page](../), play a classic game
from level 1, and your results are sent when you're back in the menu. The
same boards are in the game under **High Scores**. Players can turn sending
off with **World highscores** in the 1-player menu; see the
[privacy policy](../privacy/#world-highscores).

</div>

<script>
(function () {
  // The boards live on the port's own server (fb-server, protocol 1.7:
  // server/hiscores.h). Browsers reach it the way the web build of the game
  // does, over a secure WebSocket; the server answers in binary frames.
  // Boards 0/1 are furthest level (keyboard/mouse), 2/3 most points. Rows
  // carry the same fields, in the same words, as the game's own WORLD LEVEL /
  // WORLD POINTS tabs (HighscoreManager::RenderWorldBoard, worldboard.cpp).
  var SERVER = "wss://fb.servequake.com/";
  var BOARDS = 4;
  var boards = [], weekStart = 0;
  // tracks: which inputs are on, [keyboard/gamepad, mouse/touch]; both on
  // merges the two boards and tags each row (the game's High Scores screen
  // does the same, with the same labels and colours).
  var view = { kind: 0, tracks: [true, false] };
  // "#points" (and "#mouse" or "#both") open straight onto that board; the
  // game's "Open in browser" button sends whichever it was showing.
  var hash = location.hash.replace("#", "").split("-");
  if (hash.indexOf("points") >= 0) view.kind = 1;
  if (hash.indexOf("mouse") >= 0) view.tracks = [false, true];
  if (hash.indexOf("both") >= 0) view.tracks = [true, true];
  var statusEl = document.getElementById("fb-status");
  function levelLabel(l) { return l > 100 ? "won!" : "level " + l; }
  function shortLevel(l) { return l > 100 ? "won!" : "lv " + l; }
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
  // Server order for the board kind (server/hiscores.c): furthest level then
  // lower time; or most points, then higher level, then lower time.
  function better(a, b) {
    if (view.kind === 1 && a.points !== b.points) return b.points - a.points;
    if (a.level !== b.level) return b.level - a.level;
    return a.ms - b.ms;
  }
  function tagged(list, track) { return list.map(function (e) { return Object.assign({ track: track }, e); }); }
  function span(li, cls, text) { var s = document.createElement("span"); s.className = cls; s.textContent = text; li.appendChild(s); }
  var both = false;
  function fill(id, list, emptyText) {
    var ol = document.getElementById(id);
    ol.textContent = "";
    if (!list.length) {
      var li = document.createElement("li");
      li.className = "empty";
      li.textContent = emptyText;
      ol.appendChild(li);
      return;
    }
    var ranks = [];
    list.forEach(function (e, i) {
      var rank = i && same(list[i - 1], e) ? ranks[i - 1] : i + 1;
      ranks.push(rank);
      var li = document.createElement("li");
      span(li, "rank" + (rank <= 3 ? " m" + rank : ""), rank + ".");
      if (both) {
        var badge = document.createElement("span");
        badge.className = "badge t" + e.track;
        badge.textContent = e.track ? "M/T" : "KB";
        badge.title = e.track ? "Mouse/touch" : "Keyboard/gamepad";
        li.appendChild(badge);
      }
      span(li, "name", e.name);
      span(li, "figs", view.kind === 1
        ? pointsLabel(e.points) + "  " + shortLevel(e.level) + "  " + timeLabel(e.ms)
        : levelLabel(e.level) + "  " + timeLabel(e.ms));
      ol.appendChild(li);
    });
  }
  function render() {
    document.querySelectorAll("[data-board]").forEach(function (b) { b.setAttribute("aria-pressed", String(+b.dataset.board === view.kind)); });
    document.querySelectorAll("[data-track]").forEach(function (b) { b.setAttribute("aria-pressed", String(view.tracks[+b.dataset.track])); });
    both = view.tracks[0] && view.tracks[1];
    var shown = [0, 1].filter(function (t) { return view.tracks[t] && boards[view.kind * 2 + t]; });
    if (!shown.length) return;
    function pick(key) {
      var list = [];
      shown.forEach(function (t) { list = list.concat(tagged(boards[view.kind * 2 + t][key], t)); });
      return both ? list.sort(better) : list;  // Array.sort is stable: a tie keeps keyboard first
    }
    fill("fb-alltime", pick("alltime"), "Nobody yet -- be the first!");
    fill("fb-week", pick("week"), "Nobody yet this week -- be the first!");
  }
  function bind(attr, key) {
    document.querySelectorAll("[data-" + attr + "]").forEach(function (b) {
      b.addEventListener("click", function () { view[key] = +b.getAttribute("data-" + attr); render(); });
    });
  }
  bind("board", "kind");
  // Each input button is a toggle; the last one on stays on.
  document.querySelectorAll("[data-track]").forEach(function (b) {
    b.addEventListener("click", function () {
      var t = +b.dataset.track;
      if (view.tracks[t] && !view.tracks[1 - t]) return;
      view.tracks[t] = !view.tracks[t];
      render();
    });
  });
  render();
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
        if (++got === BOARDS) {
          statusEl.textContent = "Live from fb.servequake.com.";
          if (weekStart) {
            var d = new Date(weekStart * 1000);
            document.getElementById("fb-week-title").textContent = "THIS WEEK (SINCE " +
              d.toLocaleDateString("en-US", { month: "short", day: "numeric", timeZone: "UTC" }).toUpperCase() + ")";
          }
          ws.close();
        }
        render();
      }
    }
  };
  ws.onerror = function () { if (got < BOARDS) statusEl.textContent = "Couldn't reach the server. Try again later."; };
})();
</script>
