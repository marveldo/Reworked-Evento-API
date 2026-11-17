# Evento API: Drogon 1.9 + Postgres + Redis
#
#   docker build -t evento-api .
#   docker run --env-file .env -p 10000:10000 evento-api
#
# Two stages: "build" has compilers and -dev packages; the final image only has the
# binary, its runtime libraries, the migrations and the email templates.

# ---------------------------------------------------------------------------
# Build
# ---------------------------------------------------------------------------
FROM ubuntu:24.04 AS build

ARG DEBIAN_FRONTEND=noninteractive
ARG DROGON_VERSION=v1.9.14
ARG MIGRATE_VERSION=v4.18.3
# How many files compile at once. Each C++ compile here can use 1-2 GB of RAM, and Ninja
# otherwise runs one per CPU core, which can blow past the memory of a small build machine.
ARG BUILD_JOBS=2

RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential cmake ninja-build git pkg-config ca-certificates curl \
        # Drogon
        libjsoncpp-dev uuid-dev zlib1g-dev libssl-dev libpq-dev libhiredis-dev libbrotli-dev libc-ares-dev \
        # password hashing
        libsodium-dev \
    && rm -rf /var/lib/apt/lists/*

# Ubuntu's Drogon package is 1.8; the app needs 1.9 (coroutine middleware, WebSocketCoroController).
# This layer is cached, so it's only rebuilt when DROGON_VERSION changes.
RUN git clone --depth 1 --branch ${DROGON_VERSION} --recurse-submodules --shallow-submodules \
        https://github.com/drogonframework/drogon.git /tmp/drogon \
    && cmake -S /tmp/drogon -B /tmp/drogon/build -G Ninja \
        -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_STANDARD=20 \
        -DBUILD_EXAMPLES=OFF -DBUILD_CTL=OFF -DBUILD_TESTING=OFF \
        -DBUILD_POSTGRESQL=ON -DBUILD_REDIS=ON -DBUILD_MYSQL=OFF -DBUILD_SQLITE=OFF \
    && cmake --build /tmp/drogon/build --parallel ${BUILD_JOBS} \
    && cmake --install /tmp/drogon/build \
    && rm -rf /tmp/drogon

# golang-migrate, to run migrations when the container starts.
RUN curl -fsSL https://github.com/golang-migrate/migrate/releases/download/${MIGRATE_VERSION}/migrate.linux-amd64.tar.gz \
        | tar -xz -C /usr/local/bin migrate

WORKDIR /src
COPY . .
# CMAKE_CXX_STANDARD=20: the CMakeLists only detects C++20 if the compiler defaults to it, and GCC 13 doesn't.
RUN cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_STANDARD=20 \
    && cmake --build build --target evento_api --parallel ${BUILD_JOBS}

# ---------------------------------------------------------------------------
# Run
# ---------------------------------------------------------------------------
FROM ubuntu:24.04

ARG DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
        # HTTPS to Resend, Cloudinary, Google and ipinfo
        ca-certificates \
        # Drogon
        libjsoncpp25 libuuid1 zlib1g libssl3t64 libpq5 libhiredis1.1.0 libbrotli1 libcares2 \
        libsodium23 \
    && rm -rf /var/lib/apt/lists/* \
    && useradd --system --create-home --home-dir /app app

WORKDIR /app
COPY --from=build /src/build/evento_api ./evento_api
COPY --from=build /usr/local/bin/migrate /usr/local/bin/migrate
COPY migrations ./migrations
COPY templates ./templates
COPY openapi.json ./
COPY docker-entrypoint.sh ./docker-entrypoint.sh

# Strip Windows line endings (CRLF) from the script, or /bin/sh fails with "not found".
RUN sed -i 's/\r$//' docker-entrypoint.sh \
    && chmod +x docker-entrypoint.sh \
    && chown -R app:app /app

USER app

# Render sets PORT itself; APP_HOST must be 0.0.0.0 to accept traffic from outside the container.
ENV APP_ENV=production \
    APP_HOST=0.0.0.0 \
    PORT=10000 \
    EMAIL_TEMPLATES_DIR=templates/emails

EXPOSE 10000
ENTRYPOINT ["./docker-entrypoint.sh"]
