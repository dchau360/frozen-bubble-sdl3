# fb-server — WebSocket + TCP game server for Frozen Bubble multiplayer
# Build context: repo root (so COPY can reach server/)

FROM ubuntu:22.04 AS builder

RUN apt-get update && apt-get install -y --no-install-recommends \
    cmake ninja-build gcc libglib2.0-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY server/ ./
# server/CMakeLists.txt reads its version from the root CMakeLists.txt at
# ../CMakeLists.txt relative to /src — i.e. /CMakeLists.txt. Copy it in so the
# binary gets the real version instead of the 0.0.0-unknown fallback.
COPY CMakeLists.txt /CMakeLists.txt
RUN cmake -B /build -G Ninja -DCMAKE_BUILD_TYPE=Release . \
    && cmake --build /build --parallel

# ── Runtime image ────────────────────────────────────────────────────────────
FROM ubuntu:22.04

RUN apt-get update && apt-get install -y --no-install-recommends \
    libglib2.0-0 \
    && rm -rf /var/lib/apt/lists/*

COPY --from=builder /build/fb-server /usr/local/bin/fb-server

EXPOSE 1511

# -d  stay in foreground (required in Docker — without this fb-server daemonizes
#     and the container exits immediately)
# -q  don't register with frozen-bubble.org master (offline since ~2012)
# -l  enable UDP broadcast for LAN auto-discovery
# -z  no language preference
CMD ["/usr/local/bin/fb-server", "-d", "-q", "-l", "-z", "-o", "CONNECT"]
