<h1>Weekly rankings</h1>

<div class="boards three">
<section class="board"><h2>ROUND WINS</h2><ol id="fb-wins"><li class="empty">Loading…</li></ol></section>
<section class="board"><h2>ROUND LOSSES</h2><ol id="fb-losses"><li class="empty">Loading…</li></ol></section>
<section class="board"><h2>BUBBLES POPPED</h2><ol id="fb-popped"><li class="empty">Loading…</li></ol></section>
</div>
<p id="fb-status" class="status">Connecting to fb.servequake.com…</p>
<p id="fb-week" class="note">Resets every Monday at 00:00 UTC.</p>

<div class="about" markdown="1">

These are this week's online multiplayer standings on this port's server at
fb.servequake.com, the same lists the game shows under **Weekly rankings** in
the online lobby. Every round played there counts: the winner (or the whole
winning team) gets a round win, every other player a round loss, and everyone
keeps the bubbles they popped. The week starts over every Monday at 00:00 UTC.

Only players signed in to an account are counted, which the game does on its
own. Players are listed as `nick#tag`: the tag comes from the player's
anonymous account, so two players with the same nickname stay apart. Bots,
unnamed players and tournament rounds are not counted.

Frozen Bubble: SDL3 is a fan-made port, and these rankings are not run by the
original Frozen Bubble authors. Want to be on them? Get the game on the
[home page](../), pick **Net game**, and play a few rounds. Single-player runs
have their own board: [World highscores](../scores/).

</div>

<script>
(function () {
  // The rankings live on the port's own server (fb-server's WEEKLY command,
  // protocol 1.5: server/weeklystats.c). WEEKLY needs no name or account, so
  // the page just asks and closes. Reply:
  //   WEEKLY: <week_start> <wins> <losses> <popped> <me> <lobby>
  // with each list "nick#tag=count,..." (top 10) or "-".
  var SERVER = "wss://fb.servequake.com/";
  var statusEl = document.getElementById("fb-status");
  function parseList(field) {
    if (!field || field === "-") return [];
    return field.split(",").map(function (item) {
      var eq = item.lastIndexOf("=");
      return { name: item.slice(0, eq), count: +item.slice(eq + 1) };
    });
  }
  function span(li, cls, text) { var s = document.createElement("span"); s.className = cls; s.textContent = text; li.appendChild(s); }
  function fill(id, list, unit) {
    var ol = document.getElementById(id);
    ol.textContent = "";
    if (!list.length) {
      var li = document.createElement("li");
      li.className = "empty";
      li.textContent = "Nobody yet this week.";
      ol.appendChild(li);
      return;
    }
    var rank = 0;
    list.forEach(function (e, i) {
      if (!i || list[i - 1].count !== e.count) rank = i + 1;
      var li = document.createElement("li");
      span(li, "rank" + (rank <= 3 ? " m" + rank : ""), rank + ".");
      span(li, "name", e.name);
      span(li, "figs", e.count.toLocaleString("en-US") + (unit ? " " + unit : ""));
      ol.appendChild(li);
    });
  }
  function failed() {
    ["fb-wins", "fb-losses", "fb-popped"].forEach(function (id) {
      var ol = document.getElementById(id);
      if (ol.querySelector(".empty") && ol.textContent === "Loading…") ol.firstChild.textContent = "—";
    });
    statusEl.textContent = "Couldn't reach the server. Try again later.";
  }
  var ws, buf = "", done = false;
  try { ws = new WebSocket(SERVER); } catch (e) { failed(); return; }
  ws.binaryType = "arraybuffer";
  var decoder = new TextDecoder();
  ws.onmessage = function (ev) {
    buf += typeof ev.data === "string" ? ev.data : decoder.decode(ev.data, { stream: true });
    var nl;
    while ((nl = buf.indexOf("\n")) >= 0) {
      var line = buf.slice(0, nl).replace(/\r$/, ""); buf = buf.slice(nl + 1);
      if (/^FB\/1\.\d+ PUSH: SERVER_READY/.test(line)) {
        if (+line.split(" ")[0].split(".")[1] < 5) { statusEl.textContent = "The server doesn't keep weekly rankings yet."; done = true; ws.close(); return; }
        ws.send("FB/1.3 WEEKLY\n");
      }
      var m = line.match(/^FB\/1\.\d+ WEEKLY: (.*)$/);
      if (m) {
        var f = m[1].split(" ");
        done = true;
        fill("fb-wins", parseList(f[1]), "");
        fill("fb-losses", parseList(f[2]), "");
        fill("fb-popped", parseList(f[3]), "");
        if (+f[0]) {
          var d = new Date(+f[0] * 1000);
          document.getElementById("fb-week").textContent = "Week of " +
            d.toLocaleDateString("en-US", { month: "long", day: "numeric", year: "numeric", timeZone: "UTC" }) +
            ". Resets every Monday at 00:00 UTC.";
        }
        statusEl.textContent = "Live from fb.servequake.com.";
        ws.close();
      }
    }
  };
  ws.onerror = function () { if (!done) failed(); };
})();
</script>
