#!/bin/bash
# Frozen Bubble multiplayer server setup
# Generates a self-signed certificate for LOCAL TESTING if no real cert is present,
# then starts the server stack with docker compose.
#
# For a PUBLIC server (required for browser/WASM clients):
#   1. Point a domain name at this machine
#   2. Run: sudo certbot certonly --standalone -d yourdomain.com
#   3. Link the certs (symlink, not copy -- certbot renew updates the target
#      of live/yourdomain.com/*.pem in place, so a symlink here never goes
#      stale and never needs re-running after a renewal; a copy does):
#        sudo ln -sf /etc/letsencrypt/live/yourdomain.com/fullchain.pem ssl/fullchain.pem
#        sudo ln -sf /etc/letsencrypt/live/yourdomain.com/privkey.pem   ssl/privkey.pem
#      (These real cert files are git-ignored; only *.pem.example templates are tracked.)
#   4. Run this script again.

set -e
cd "$(dirname "$0")"

CERT=ssl/fullchain.pem
KEY=ssl/privkey.pem

# Check whether valid PEM files already exist.
#
# `openssl pkey`, not `openssl rsa`: certbot issues ECDSA keys by default since
# version 2.0, and `openssl rsa -check` rejects those outright ("Not an RSA
# key"). The old check therefore classified a perfectly good Let's Encrypt key
# as invalid and fell through to the self-signed branch below, which truncated
# both of the operator's real files (audit finding REL-010).
#
# Parse-only, without -check: LibreSSL — which is what /usr/bin/openssl is on
# macOS — does not implement -check for `pkey` and fails it even on a valid RSA
# key. Parsing is enough to tell a real key from a missing or corrupt one.
cert_ok() { openssl x509  -in "$CERT" -noout 2>/dev/null; }
key_ok()  { openssl pkey  -in "$KEY"  -noout 2>/dev/null; }

if cert_ok && key_ok; then
    echo "SSL certificates found — skipping generation."
elif [ -s "$CERT" ] || [ -s "$KEY" ]; then
    # Something is there but did not validate. Never overwrite it: these are
    # the files the operator copied out of /etc/letsencrypt, and regenerating
    # silently replaced a working public certificate with a self-signed
    # CN=localhost one that every browser and WASM client rejects.
    echo ""
    echo "ERROR: docker/ssl/ already contains certificate material, but it did"
    echo "       not validate:"
    [ -s "$CERT" ] && { cert_ok || echo "         $CERT: not a readable X.509 certificate"; }
    [ -s "$KEY"  ] && { key_ok  || echo "         $KEY: not a readable private key"; }
    echo ""
    echo "Refusing to overwrite it — a real certificate would be destroyed."
    echo "Re-copy it, or move the existing files aside and re-run to generate"
    echo "a self-signed certificate for local testing:"
    echo ""
    echo "    mv $CERT $CERT.bak && mv $KEY $KEY.bak"
    echo ""
    exit 1
else
    echo ""
    echo "WARNING: No SSL certificate found in docker/ssl/"
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
