#!/bin/bash
# Frozen Bubble multiplayer server setup
# Generates a self-signed certificate for LOCAL TESTING if no real cert is present,
# then starts the server stack with docker compose.
#
# For a PUBLIC server (required for browser/WASM clients):
#   1. Point a domain name at this machine
#   2. Run: sudo certbot certonly --standalone -d yourdomain.com
#   3. Copy the certs:
#        sudo cp /etc/letsencrypt/live/yourdomain.com/fullchain.pem ssl/fullchain.pem
#        sudo cp /etc/letsencrypt/live/yourdomain.com/privkey.pem   ssl/privkey.pem
#   4. Run this script again.

set -e
cd "$(dirname "$0")"

CERT=ssl/fullchain.pem
KEY=ssl/privkey.pem

# Check whether valid PEM files already exist
cert_ok() { openssl x509 -in "$CERT" -noout 2>/dev/null; }
key_ok()  { openssl rsa  -in "$KEY"  -check -noout 2>/dev/null; }

if cert_ok && key_ok; then
    echo "SSL certificates found — skipping generation."
else
    echo ""
    echo "WARNING: No valid SSL certificate found in docker/ssl/"
    echo "Generating a SELF-SIGNED certificate for local testing."
    echo ""
    echo "Self-signed certificates will NOT work for browser (WASM) clients"
    echo "connecting from a public HTTPS page. You need a real domain and a"
    echo "certificate from Let's Encrypt (see instructions above)."
    echo ""

    openssl req -x509 -newkey rsa:2048 -days 365 -nodes \
        -keyout "$KEY" \
        -out    "$CERT" \
        -subj "/CN=localhost"

    echo ""
    echo "Self-signed cert generated. Native clients can use port 1511 directly."
    echo "Browser clients require a real domain + Let's Encrypt cert."
    echo ""
fi

echo "Starting Frozen Bubble server stack..."
docker compose up --build "$@"
