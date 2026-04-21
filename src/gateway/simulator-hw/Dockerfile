FROM oven/bun:1 AS build

WORKDIR /app

RUN apt-get update \
	&& apt-get install -y --no-install-recommends docker-cli \
	&& rm -rf /var/lib/apt/lists/*

COPY package.json bun.lock ./
RUN bun install --frozen-lockfile

COPY . .
RUN bun run build

EXPOSE 4173

ENV HOST=0.0.0.0
ENV PORT=4173
ENV MQTT_HOST=mosquitto
ENV MQTT_PORT=1883
ENV SIM_DATA_DIR=/app/sim-data

CMD ["bun", "run", "start"]
