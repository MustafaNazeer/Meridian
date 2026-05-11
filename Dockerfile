# syntax=docker/dockerfile:1.7
#
# Multi-stage build for the meridian-server binary. The runtime image
# contains only libstdc++, the binary, and a non-root user; the builder
# stage with the C++ toolchain is discarded. Tests, bench, and replay
# are switched off via the CMake options so the build context stays
# tight and the build runs in well under a minute.

ARG DEBIAN_TAG=trixie-slim

# --- builder stage --------------------------------------------------
FROM debian:${DEBIAN_TAG} AS builder

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update \
 && apt-get install -y --no-install-recommends \
        build-essential \
        cmake \
        ninja-build \
        g++ \
        ca-certificates \
 && rm -rf /var/lib/apt/lists/*

WORKDIR /work

# Only the engine-side sources are copied in. The frontend, design
# mockups, docs, and tests are not needed by the runtime binary.
COPY CMakeLists.txt ./
COPY include/ include/
COPY src/ src/
COPY apps/server/ apps/server/

RUN cmake -B build -S . -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DMERIDIAN_BUILD_TESTS=OFF \
        -DMERIDIAN_BUILD_BENCH=OFF \
        -DMERIDIAN_BUILD_REPLAY=OFF \
        -DMERIDIAN_BUILD_SERVER=ON \
 && cmake --build build --target meridian-server -j

# --- runtime stage --------------------------------------------------
FROM debian:${DEBIAN_TAG} AS runtime

RUN apt-get update \
 && apt-get install -y --no-install-recommends \
        ca-certificates \
        libstdc++6 \
 && rm -rf /var/lib/apt/lists/* \
 && groupadd --system meridian \
 && useradd --system --gid meridian --uid 10000 \
        --no-create-home --shell /usr/sbin/nologin meridian

COPY --from=builder /work/build/apps/server/meridian-server /usr/local/bin/meridian-server

USER meridian
EXPOSE 8080

# Defaults match the local dev posture: no origin allowlist. Fly.io
# overrides MERIDIAN_ORIGINS via [env] in fly.toml to lock the live
# deploy to the Cloudflare Pages origin plus localhost dev.
ENV MERIDIAN_PORT=8080 \
    MERIDIAN_RATE=50000 \
    MERIDIAN_SEED=42 \
    MERIDIAN_ORIGINS=""

# A small shim builds the argv from env vars so a deploy can change the
# port, rate, or origin allowlist without rebuilding the image.
ENTRYPOINT ["/bin/sh","-c","\
exec /usr/local/bin/meridian-server \
    --port \"$MERIDIAN_PORT\" \
    --rate \"$MERIDIAN_RATE\" \
    --seed \"$MERIDIAN_SEED\" \
    ${MERIDIAN_ORIGINS:+--origins \"$MERIDIAN_ORIGINS\"}\
"]
