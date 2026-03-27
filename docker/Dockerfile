# fb-server — WebSocket + TCP game server for Frozen Bubble multiplayer
# Build context: repo root (so COPY can reach server/)

FROM ubuntu:22.04 AS builder

RUN apt-get update && apt-get install -y --no-install-recommends \
    cmake ninja-build gcc libglib2.0-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY server/ ./
RUN cmake -B /build -G Ninja -DCMAKE_BUILD_TYPE=Release . \
    && cmake --build /build --parallel

# ── Runtime image ────────────────────────────────────────────────────────────
FROM ubuntu:22.04

RUN apt-get update && apt-get install -y --no-install-recommends \
    libglib2.0-0 \
    && rm -rf /var/lib/apt/lists/*

COPY --from=builder /build/fb-server /usr/local/bin/fb-server

EXPOSE 1511

# -q  don't register with frozen-bubble.org master (offline since ~2012)
# -l  enable UDP broadcast for LAN auto-discovery
# -z  no language preference
CMD ["/usr/local/bin/fb-server", "-q", "-l", "-z"]
