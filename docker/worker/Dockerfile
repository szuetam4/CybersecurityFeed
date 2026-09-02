FROM debian:12-slim AS builder

RUN apt-get update && apt-get install -y --no-install-recommends \
    g++ \
    cmake \
    make \
    libcurl4-openssl-dev \
    libsqlite3-dev \
    nlohmann-json3-dev \
    ca-certificates \
    libpugixml-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /build
COPY scraper/worker.cpp .

RUN g++ -std=c++17 -O2 -o worker worker.cpp \
    -lcurl -lsqlite3 -lpthread -lpugixml

FROM debian:12-slim

RUN apt-get update && apt-get install -y --no-install-recommends \
    libcurl4 \
    libsqlite3-0 \
    libpugixml1v5 \
    sqlite3 \
    curl \
    ca-certificates \
    cron \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY --from=builder /build/worker ./worker
COPY scraper/resources ./scraper/resources

COPY docker/worker/crontab.template /app/cron/crontab.template
COPY docker/worker/entrypoint.sh /entrypoint.sh
RUN chmod +x /entrypoint.sh

ENTRYPOINT ["/entrypoint.sh"]
