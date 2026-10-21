<h1>Highscores</h1>

<div class="tabs big" role="group" aria-label="Controls (turn on both to compare)">
<button type="button" data-track="0" aria-pressed="true">Keyboard</button>
<button type="button" data-track="1" aria-pressed="true">Mouse/Touch</button>
</div>
<div class="tabs" role="group" aria-label="Board">
<button type="button" data-board="1" aria-pressed="true">Points</button>
<button type="button" data-board="0" aria-pressed="false">Level</button>
</div>

<div class="boards">
<section class="board"><h2>ALL-TIME</h2><ol id="fb-alltime"><li class="empty">Loading…</li></ol><div class="details" id="fb-alltime-details" aria-live="polite" hidden></div></section>
<section class="board"><h2 id="fb-week-title">THIS WEEK</h2><ol id="fb-week"><li class="empty">Loading…</li></ol><div class="details" id="fb-week-details" aria-live="polite" hidden></div></section>
</div>
<p id="fb-status" class="status">Connecting to fb.servequake.com…</p>
<p class="note">Scores are sent by each player's game and aren't verified. Click or tap a row for its details.</p>

<div class="about" markdown="1">

**Points** ranks the most points scored in one life, in a game from
level 1 of the standard 100 levels. The score starts over at every death,
and the level shown is the one that life got to. **Level** ranks whole
runs from level 1: the furthest level cleared wins, and the faster time
breaks a tie. The shots column is how many bubbles the run fired to get
there; it doesn't change the ranking, and runs from before October 2026 have
none. "This week"
starts over every Monday at 00:00 UTC. The date after a run is the day it was
set, in UTC; runs from before October 2026 have none.

Keyboard/gamepad and mouse/touch runs are ranked separately, since aiming
with one is not the same game as aiming with the other. A level played with
both counts as mouse/touch, so the keyboard board holds only levels played
entirely with keyboard or gamepad. Every run is tagged
<span class="badge t0">KB</span> or <span class="badge t1">M/T</span>, and
both are shown in one list until you switch one off. Players are listed as
`nick#tag`: the tag comes from the player's anonymous account, so two players
with the same nickname stay apart. The flag is the country the player's game
last reported when it went online; players who only play solo have none.

The board lives on this port's own server at fb.servequake.com. Frozen Bubble:
SDL3 is a fan-made port, and the board is not run by the original Frozen
Bubble authors.

Want to be on it? Get the game on the [home page](../), play a classic game
from level 1, and your results are sent when you're back in the menu. The
same boards are in the game under **High Scores**. Players can turn sending
off with **Online highscores** in the 1-player menu; see the
[privacy policy](../privacy/#world-highscores).

</div>

<script>
(function () {
  // The boards live on the port's own server (fb-server, protocol 1.7:
  // server/hiscores.h). Browsers reach it the way the web build of the game
  // does, over a secure WebSocket; the server answers in binary frames.
  // Boards 0/1 are furthest level (keyboard/mouse), 2/3 most points. Rows
  // carry the same fields, in the same words, as the game's own POINTS /
  // LEVEL tabs (HighscoreManager::RenderWorldBoard, worldboard.cpp).
  var SERVER = "wss://fb.servequake.com/";
  var BOARDS = 4;
  var boards = [], weekStart = 0;
  // tracks: which inputs are on, [keyboard/gamepad, mouse/touch]; both on
  // merges the two boards and tags each row (the game's High Scores screen
  // does the same, with the same labels and colours).
  // kind: 1 = most points (shown first, and the default), 0 = furthest level.
  var view = { kind: 1, tracks: [true, true] };
  // "#points" or "#level" (and "#kb" or "#mouse") open straight onto that
  // board, both inputs otherwise; the game's "Open in browser" button sends
  // whichever it was showing. "#both" is what older games sent for both;
  // they also sent no board for level, which now opens on points.
  var hash = location.hash.replace("#", "").split("-");
  if (hash.indexOf("points") >= 0) view.kind = 1;
  if (hash.indexOf("level") >= 0) view.kind = 0;
  if (hash.indexOf("kb") >= 0) view.tracks = [true, false];
  if (hash.indexOf("mouse") >= 0) view.tracks = [false, true];
  if (hash.indexOf("both") >= 0) view.tracks = [true, true];
  var statusEl = document.getElementById("fb-status");
  function shortLevel(l) { return l > 100 ? "won!" : "lv " + l; }
  function timeLabel(ms) {
    var t = Math.floor(ms / 1000), h = Math.floor(t / 3600), m = Math.floor(t / 60) % 60, s = t % 60;
    var mm = (h ? String(m).padStart(2, "0") : m) + "'" + String(s).padStart(2, "0") + '"';
    return h ? h + ":" + mm : mm;
  }
  function pointsLabel(p) { return p.toLocaleString("en-US") + " pts"; }
  // days: the reply's matching day list (the UTC day index each run was set,
  // 0 = from before the server kept them), or undefined from an older server;
  // shots likewise the shots each run took (0 = not counted).
  function parseList(field, days, shots) {
    if (field === "-") return [];
    var d = days && days !== "-" ? days.split(",") : [];
    var n = shots && shots !== "-" ? shots.split(",") : [];
    return field.split(",").map(function (item, i) {
      var eq = item.lastIndexOf("="), v = item.slice(eq + 1).split("/");
      // v[3]: the player's country, when the server has one.
      var cc = /^[A-Z]{2}$/.test(v[3] || "") ? v[3] : "";
      return { name: item.slice(0, eq), level: +v[0], ms: +v[1], points: +(v[2] || 0), country: cc,
               day: +(d[i] || 0), shots: +(n[i] || 0) };
    });
  }
  // "Oct 1", or "Aug 27 '25" outside this year; UTC, like the week.
  function dateLabel(day) {
    var d = new Date(day * 86400000);
    var s = d.toLocaleDateString("en-US", { month: "short", day: "numeric", timeZone: "UTC" });
    var y = d.getUTCFullYear();
    return y === new Date().getUTCFullYear() ? s : s + " '" + String(y).slice(-2);
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
  // The row picked for details, per list: which entry (by name and input, so
  // it survives a re-render), or null. Like the game's High Scores screen,
  // the whole record shows in a strip under the list.
  var picked = { "fb-alltime": null, "fb-week": null };
  var regionNames = null;
  try { regionNames = new Intl.DisplayNames(["en"], { type: "region" }); } catch (e) {}
  function countryName(cc) {
    try { return (regionNames && regionNames.of(cc)) || cc; } catch (e) { return cc; }
  }
  function levelPhrase(l) { return l > 100 ? "Cleared all 100 levels" : "Level " + l; }
  function showDetails(id, list, ranks) {
    var box = document.getElementById(id + "-details");
    var p = picked[id], i = -1;
    if (p) list.forEach(function (e, k) { if (i < 0 && e.name === p.name && e.track === p.track) i = k; });
    document.querySelectorAll("#" + id + " li.pick").forEach(function (li) {
      li.setAttribute("aria-pressed", String(+li.dataset.i === i));
    });
    if (i < 0) { picked[id] = null; box.hidden = true; box.textContent = ""; return; }
    var e = list[i];
    box.textContent = "";
    var head = document.createElement("div");
    head.className = "dhead";
    span(head, "drank", "#" + ranks[i] + (id === "fb-week" ? " this week" : " all-time"));
    span(head, "dname", e.name);
    box.appendChild(head);
    var dl = document.createElement("dl");
    function row(label, value) {
      if (!value) return;
      var dt = document.createElement("dt"); dt.textContent = label; dl.appendChild(dt);
      var dd = document.createElement("dd"); dd.textContent = value; dl.appendChild(dd);
    }
    if (view.kind === 1) {
      row("Points", pointsLabel(e.points) + " in one life");
      row("Reached", levelPhrase(e.level).toLowerCase());
    } else {
      row("Furthest", levelPhrase(e.level).toLowerCase());
      row("Shots", e.shots ? String(e.shots) : "");
    }
    row("Time", timeLabel(e.ms));
    row("Input", e.track ? "Mouse/touch" : "Keyboard/gamepad");
    row("Country", e.country ? countryName(e.country) : "");
    row("Set", e.day ? new Date(e.day * 86400000).toLocaleDateString("en-US",
      { year: "numeric", month: "long", day: "numeric", timeZone: "UTC" }) + " (UTC)" : "");
    box.appendChild(dl);
    box.hidden = false;
  }
  function pick(id, e) {
    var p = picked[id];
    picked[id] = p && p.name === e.name && p.track === e.track ? null : { name: e.name, track: e.track };
    render();
  }
  function fill(id, list, emptyText) {
    var ol = document.getElementById(id);
    // Re-rendering rebuilds the rows: note which one had the keyboard focus.
    var a = document.activeElement;
    var focusI = a && a.parentNode === ol && a.dataset.i !== undefined ? +a.dataset.i : -1;
    ol.textContent = "";
    if (!list.length) {
      var li = document.createElement("li");
      li.className = "empty";
      li.textContent = emptyText;
      ol.appendChild(li);
      showDetails(id, [], []);
      return;
    }
    var ranks = [];
    list.forEach(function (e, i) {
      var rank = i && same(list[i - 1], e) ? ranks[i - 1] : i + 1;
      ranks.push(rank);
      var li = document.createElement("li");
      li.className = "pick";
      li.dataset.i = i;
      li.tabIndex = 0;
      li.setAttribute("role", "button");
      li.addEventListener("click", function () { pick(id, e); });
      li.addEventListener("keydown", function (ev) {
        if (ev.key === "Enter" || ev.key === " ") { ev.preventDefault(); pick(id, e); }
        else if (ev.key === "Escape" && picked[id]) { picked[id] = null; render(); }
        else if (ev.key === "ArrowDown" || ev.key === "ArrowUp") {
          var next = ev.key === "ArrowDown" ? li.nextElementSibling : li.previousElementSibling;
          if (next) { ev.preventDefault(); next.focus(); }
        }
      });
      span(li, "rank" + (rank <= 3 ? " m" + rank : ""), rank + ".");
      {
        var badge = document.createElement("span");
        badge.className = "badge t" + e.track;
        badge.textContent = e.track ? "M/T" : "KB";
        badge.title = e.track ? "Mouse/touch" : "Keyboard/gamepad";
        li.appendChild(badge);
      }
      if (e.country) {
        var flag = document.createElement("span");
        flag.className = "flag";
        // Regional-indicator pair: the flag emoji, where the system has one.
        flag.textContent = String.fromCodePoint(0x1F1E6 + e.country.charCodeAt(0) - 65,
                                                0x1F1E6 + e.country.charCodeAt(1) - 65);
        flag.title = e.country;
        flag.setAttribute("aria-label", e.country);
        li.appendChild(flag);
      }
      span(li, "name", e.name);
      span(li, "figs", view.kind === 1
        ? pointsLabel(e.points) + "  " + shortLevel(e.level) + "  " + timeLabel(e.ms)
        : shortLevel(e.level) + "  " + timeLabel(e.ms));
      // A level board's runs carry their shots, as in the game; a points
      // record is one life, which the run's shots don't describe. Always a
      // cell there, like the date, so the columns line up.
      // On a phone the word goes (template-scores.html) and the number stays.
      if (view.kind === 0) {
        span(li, "shots", e.shots ? String(e.shots) : "");
        if (e.shots) {
          span(li.lastChild, "unit", e.shots === 1 ? " shot" : " shots");
          li.lastChild.title = e.shots + (e.shots === 1 ? " shot" : " shots");
        }
      }
      // Always a cell, empty for a run with no date, so the figures line up.
      span(li, "date", e.day ? dateLabel(e.day) : "");
      if (e.day)
        li.lastChild.title = "Set " + new Date(e.day * 86400000).toLocaleDateString("en-US",
          { year: "numeric", month: "long", day: "numeric", timeZone: "UTC" }) + " (UTC)";
      ol.appendChild(li);
    });
    showDetails(id, list, ranks);
    if (focusI >= 0 && ol.children[focusI]) ol.children[focusI].focus();
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
      b.addEventListener("click", function () {
        view[key] = +b.getAttribute("data-" + attr);
        picked["fb-alltime"] = picked["fb-week"] = null;  // another board: its own rows
        render();
      });
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
        if (+line.split(" ")[0].split(".")[1] < 7) { statusEl.textContent = "The server doesn't have a highscore board yet."; ws.close(); return; }
        for (var b = 0; b < BOARDS; b++) ws.send("FB/1.3 HISCORES " + b + "\n");
      }
      var m = line.match(/^FB\/1\.\d+ HISCORES: (.*)$/);
      if (m) {
        var f = m[1].split(" ");
        if (f.length >= 4) {
          weekStart = +f[0];
          boards[got] = { alltime: parseList(f[1], f[4], f[6]), week: parseList(f[2], f[5], f[7]) };
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
