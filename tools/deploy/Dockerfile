# Версии передаются из существующих lock-файлов; неявного latest нет.
ARG DEV_IMAGE
ARG NODE_IMAGE
FROM ${NODE_IMAGE} AS frontend
WORKDIR /frontend
COPY frontend/admin/package.json frontend/admin/pnpm-lock.yaml ./
RUN npm install --global pnpm@11.25.0 && pnpm install --frozen-lockfile
COPY frontend/admin/ ./
RUN pnpm build

WORKDIR /web
COPY frontend/web/package.json frontend/web/pnpm-lock.yaml ./
RUN pnpm install --frozen-lockfile
COPY frontend/web/ ./
RUN pnpm build

FROM ${DEV_IMAGE} AS build
USER root
WORKDIR /workspaces/broker
COPY CMakeLists.txt requirements.txt .clang-format ./
COPY cmake/ cmake/
COPY libs/ libs/
COPY services/ services/
COPY schemas/ schemas/
COPY scripts/openapi_gen/ scripts/openapi_gen/
COPY scripts/templates/ scripts/templates/
COPY scripts/service_catalog.py scripts/service_catalog.py
COPY tools/versions.env tools/python-constraints.txt tools/
RUN cmake -S . -B /tmp/build -G Ninja -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_CXX_SCAN_FOR_MODULES=OFF -DBUILD_TESTING=OFF \
    -DPython3_EXECUTABLE=/opt/broker/venv/bin/python && \
    cmake --build /tmp/build -j 4 --target $(python3 scripts/service_catalog.py --plain)
COPY tools/deploy/package.py /tmp/package.py
RUN python3 /tmp/package.py /tmp/build /runtime $(python3 scripts/service_catalog.py --plain)

FROM ubuntu:24.04@sha256:534baea6a22c03a63003dbc8dbe78fe34bc0d7e595d9a9dc9834884ff530eb55 AS runtime
ARG SERVICE
ARG REVISION
LABEL org.opencontainers.image.source="https://github.com/MakRus1/Broker" \
      org.opencontainers.image.revision="${REVISION}"
COPY --from=build /runtime/libraries/ /
COPY --from=build /runtime/bin/${SERVICE} /app/service
COPY --from=build /etc/ssl/certs/ca-certificates.crt /etc/ssl/certs/ca-certificates.crt
COPY --from=build /usr/share/zoneinfo /usr/share/zoneinfo
COPY --from=frontend /frontend/dist /app/assets
COPY --from=frontend /web/dist /app/web-assets
USER 10001:10001
ENV USERVER_ENABLE_STACK_USAGE_MONITOR=0 LANG=C.UTF-8
WORKDIR /app
ENTRYPOINT ["/app/service", "-c", "/etc/broker/static_config.yaml", "--config_vars", "/etc/broker-secrets/config_vars.yaml"]
