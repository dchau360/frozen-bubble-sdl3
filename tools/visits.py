#!/usr/bin/env python3
"""Private report of who visited the site, from nginx's visit log.

Run on the server, never checked-in data: docker/visits/ holds one file per
UTC day (see docker/nginx.conf) and docker/geoip/ holds DB-IP's Lite files
(tools/update-geoip.sh). City lookup needs `pip install maxminddb`; without it
the country CSV is used.

  tools/visits.py                 last 7 days, one line per address
  tools/visits.py --days 30
  tools/visits.py --raw           every request
  tools/visits.py --prune 90      delete log files older than 90 days
"""
import argparse, bisect, csv, ipaddress, sys, time
from collections import defaultdict
from datetime import datetime, timedelta, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent / "docker"


def make_lookup():
    mmdb = ROOT / "geoip" / "dbip-city-lite.mmdb"
    try:
        import maxminddb
        if mmdb.exists():
            reader = maxminddb.open_database(str(mmdb))

            def look(ip):
                try:
                    r = reader.get(ip) or {}
                except Exception:
                    return "?"
                name = lambda k: (r.get(k) or {}).get("names", {}).get("en", "")
                parts = [name("city"), (r.get("subdivisions") or [{}])[0].get("names", {}).get("en", ""),
                         (r.get("country") or {}).get("iso_code", "")]
                return ", ".join(p for p in parts if p) or "?"
            return look
    except ImportError:
        print("(pip install maxminddb for city names; showing countries)", file=sys.stderr)
    csvp = ROOT / "geoip" / "dbip-country-lite.csv"
    if not csvp.exists():
        return lambda ip: "?"
    rows = []
    with open(csvp, newline="") as f:
        for a, b, cc in csv.reader(f):
            try:
                rows.append((int(ipaddress.ip_address(a)), int(ipaddress.ip_address(b)), cc, ipaddress.ip_address(a).version))
            except ValueError:
                pass
    rows.sort()
    starts = [r[0] for r in rows]

    def look(ip):
        try:
            a = ipaddress.ip_address(ip)
        except ValueError:
            return "?"
        n = int(a)
        i = bisect.bisect_right(starts, n) - 1
        while i >= 0 and rows[i][3] != a.version:
            i -= 1
        return rows[i][2] if i >= 0 and rows[i][0] <= n <= rows[i][1] else "?"
    return look


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--days", type=int, default=7)
    ap.add_argument("--raw", action="store_true")
    ap.add_argument("--prune", type=int, metavar="DAYS")
    a = ap.parse_args()
    logs = ROOT / "visits"
    today = datetime.now(timezone.utc).date()
    if a.prune is not None:
        for f in logs.glob("*.log"):
            try:
                d = datetime.strptime(f.stem, "%Y-%m-%d").date()
            except ValueError:
                continue
            if (today - d).days > a.prune:
                f.unlink()
                print("deleted", f.name)
        return
    look = make_lookup()
    seen = defaultdict(lambda: [0, "", "", ""])  # count, first, last, ua
    for i in range(a.days):
        f = logs / f"{today - timedelta(days=i)}.log"
        if not f.exists():
            continue
        for line in f.read_text(errors="replace").splitlines():
            p = line.split("\t")
            if len(p) < 8:
                continue
            ts, ip, _m, uri, status, upg, _ref, ua = p[:8]
            if a.raw:
                print(ts, ip, look(ip), status, "WS" if upg else "", uri, ua[:60])
                continue
            s = seen[ip]
            s[0] += 1
            s[1] = s[1] or ts
            s[2] = ts
            s[3] = ua
    if a.raw:
        return
    print(f"{'requests':>8}  {'first seen':19}  {'last seen':19}  {'address':39}  location")
    for ip, (n, first, last, ua) in sorted(seen.items(), key=lambda kv: kv[1][2], reverse=True):
        print(f"{n:>8}  {first[:19]:19}  {last[:19]:19}  {ip:39}  {look(ip)}")
    print(f"\n{len(seen)} distinct addresses in the last {a.days} days")


if __name__ == "__main__":
    main()
