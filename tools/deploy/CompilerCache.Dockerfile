# Отдельный обмен содержимым cache mount: экспорт слоёв BuildKit его не сохраняет.
ARG DEV_IMAGE=scratch
FROM ${DEV_IMAGE} AS restore
USER root
ARG TARGETARCH
ARG CACHE_TRANSFER
RUN --mount=type=bind,from=compiler-cache,target=/seed \
    --mount=type=cache,id=broker-release-${TARGETARCH},target=/cache/ccache \
    test -n "$CACHE_TRANSFER" && cp -a /seed/. /cache/ccache/

FROM ${DEV_IMAGE} AS snapshot
USER root
ARG TARGETARCH
ARG CACHE_TRANSFER
RUN --mount=type=cache,id=broker-release-${TARGETARCH},target=/cache/ccache \
    test -n "$CACHE_TRANSFER" && mkdir -p /snapshot && cp -a /cache/ccache/. /snapshot/

FROM scratch AS export
COPY --from=snapshot /snapshot/ /
