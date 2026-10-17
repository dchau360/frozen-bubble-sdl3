#!/usr/bin/env bash
# Download DB-IP's free "IP to Country Lite" CSV (for fb-server) and "IP to
# City Lite" mmdb (for tools/visits.py) into docker/geoip/. Data: DB-IP.com,
# CC BY 4.0. They publish a new file each month; run this monthly (or from cron),
# then `sudo docker compose up -d --no-deps fb-server` to reload the CSV.
set -euo pipefail
cd "$(dirname "$0")/../docker"
mkdir -p geoip visits
fetch() {  # fetch <name-prefix> <ext> <dest>
    for back in 0 1; do
        ym=$(date -u -d "-$back month" +%Y-%m 2>/dev/null || date -u -v-"$back"m +%Y-%m)
        url="https://download.db-ip.com/free/$1-$ym.$2.gz"
        if curl -fsSL "$url" -o "geoip/$3.gz"; then
            gunzip -f "geoip/$3.gz"
            echo "got $url"
            return 0
        fi
    done
    echo "could not download $1" >&2
    return 1
}
fetch dbip-country-lite csv dbip-country-lite.csv
fetch dbip-city-lite mmdb dbip-city-lite.mmdb
ls -l geoip
