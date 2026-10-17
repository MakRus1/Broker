# Один и тот же образ используется в CI и Dev Container, для amd64 и arm64.
FROM curlimages/curl:8.12.1@sha256:94e9e444bcba979c2ea12e27ae39bee4cd10bc7041a472c4727a558e213744e6 AS certificates
FROM ubuntu:24.04@sha256:534baea6a22c03a63003dbc8dbe78fe34bc0d7e595d9a9dc9834884ff530eb55
SHELL ["/bin/bash", "-euo", "pipefail", "-c"]
ENV DEBIAN_FRONTEND=noninteractive
# Минимальный Ubuntu еще не содержит CA для HTTPS-снимка apt.
COPY --from=certificates /etc/ssl/certs/ca-certificates.crt /etc/ssl/certs/ca-certificates.crt
# Снимок репозиториев фиксирует в том числе транзитивные системные зависимости.
RUN sed -i -E '/^URIs:/c\URIs: https://snapshot.ubuntu.com/ubuntu/20261001T000000Z/' /etc/apt/sources.list.d/ubuntu.sources && sed -i '/^URIs:/a Check-Valid-Until: no' /etc/apt/sources.list.d/ubuntu.sources
COPY tools/apt-packages.txt /tmp/apt-packages.txt
RUN apt-get update && xargs -r apt-get install -y --no-install-recommends < /tmp/apt-packages.txt && rm -rf /var/lib/apt/lists/*
COPY tools/versions.env /opt/broker/versions.env
COPY requirements.txt /opt/broker/requirements.txt
COPY tools/python-constraints.txt /opt/broker/python-constraints.txt
RUN source /opt/broker/versions.env && python3 -m venv /opt/broker/venv && \
    /opt/broker/venv/bin/pip install --no-cache-dir "cmake==$CMAKE_VERSION" "ninja==$NINJA_VERSION" -r /opt/broker/requirements.txt && \
    curl -fL "https://codeload.github.com/userver-framework/userver/tar.gz/$USERVER_COMMIT" -o /tmp/userver.tar.gz && \
    echo "$USERVER_ARCHIVE_SHA256  /tmp/userver.tar.gz" | sha256sum -c - && \
    mkdir /opt/userver && tar -xzf /tmp/userver.tar.gz -C /opt/userver --strip-components=1 && rm /tmp/userver.tar.gz
ENV PATH=/opt/broker/venv/bin:$PATH CC=clang-18 CXX=clang++-18
ARG TARGETARCH
RUN --mount=type=cache,id=broker-userver-${TARGETARCH},target=/tmp/userver-build cmake -S /opt/userver -B /tmp/userver-build -G Ninja \
    -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_STANDARD=20 -DCMAKE_CXX_SCAN_FOR_MODULES=OFF -DCMAKE_INSTALL_PREFIX=/opt/userver-install \
    -DUSERVER_PIP_OPTIONS="--constraint;/opt/broker/python-constraints.txt" \
    -DUSERVER_INSTALL=ON -DUSERVER_FEATURE_GRPC=ON -DUSERVER_FEATURE_POSTGRESQL=ON -DUSERVER_FEATURE_OTLP=ON \
    -DUSERVER_FEATURE_CHAOTIC=ON -DUSERVER_FEATURE_CHAOTIC_OPENAPI=OFF && \
    cmake --build /tmp/userver-build -j 4 && cmake --install /tmp/userver-build || { cat /tmp/userver-build/CMakeFiles/CMakeConfigureLog.yaml; exit 1; }
# userver устанавливает отдельные библиотеки и CMake-экспорты для Debug и Release.
RUN --mount=type=cache,id=broker-userver-debug-${TARGETARCH},target=/tmp/userver-debug-build cmake -S /opt/userver -B /tmp/userver-debug-build -G Ninja \
    -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_STANDARD=20 -DCMAKE_CXX_SCAN_FOR_MODULES=OFF -DCMAKE_INSTALL_PREFIX=/opt/userver-install \
    -DUSERVER_PIP_OPTIONS="--constraint;/opt/broker/python-constraints.txt" \
    -DUSERVER_INSTALL=ON -DUSERVER_FEATURE_GRPC=ON -DUSERVER_FEATURE_POSTGRESQL=ON -DUSERVER_FEATURE_OTLP=ON \
    -DUSERVER_FEATURE_CHAOTIC=ON -DUSERVER_FEATURE_CHAOTIC_OPENAPI=OFF && \
    cmake --build /tmp/userver-debug-build -j 4 && cmake --install /tmp/userver-debug-build || { cat /tmp/userver-debug-build/CMakeFiles/CMakeConfigureLog.yaml; exit 1; }
ENV CMAKE_PREFIX_PATH=/opt/userver-install
# Testsuite требует en_US.UTF-8, а ASan/UBSan — заголовки и runtime Clang.
RUN apt-get update && apt-get install -y --no-install-recommends locales libclang-rt-18-dev && \
    echo 'en_US.UTF-8 UTF-8' > /etc/locale.gen && locale-gen && rm -rf /var/lib/apt/lists/*
ENV LANG=en_US.UTF-8 LC_ALL=en_US.UTF-8
# userfaultfd недоступен в обычном Docker; отключаем только необязательный учет стека корутин.
ENV USERVER_ENABLE_STACK_USAGE_MONITOR=0
# PostgreSQL testsuite отказывается работать от root.
RUN useradd --create-home --uid 1001 builder && mkdir -p /workspaces/broker/{.venv-gen,build-debug,build-release,build-sanitize} && chown -R builder:builder /workspaces/broker
USER builder
WORKDIR /workspaces/broker
