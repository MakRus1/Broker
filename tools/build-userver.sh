#!/usr/bin/env bash
# Собираем один вариант userver; каталог установки не смешивает ABI разных режимов.
set -euo pipefail
variant=${1:?Укажите dev или sanitize}
if [[ "$variant" == dev ]]; then
    modes=(Release Debug)
    prefix=/opt/userver-install
    sanitizers=''
elif [[ "$variant" == sanitize ]]; then
    modes=(Debug)
    prefix=/opt/userver-sanitize
    sanitizers='addr ub'
else
    echo "Неизвестный вариант: $variant" >&2
    exit 1
fi
for mode in "${modes[@]}"; do
    build_dir="/tmp/userver-build/$variant/$mode"
    cmake -S /opt/userver -B "$build_dir" -G Ninja \
        -DCMAKE_BUILD_TYPE="$mode" -DCMAKE_CXX_STANDARD=20 -DCMAKE_CXX_SCAN_FOR_MODULES=OFF \
        -DCMAKE_INSTALL_PREFIX="$prefix" \
        -DUSERVER_PIP_OPTIONS='--constraint;/opt/broker/python-constraints.txt' \
        -DUSERVER_SANITIZE="$sanitizers" \
        -DUSERVER_INSTALL=ON -DUSERVER_FEATURE_GRPC=ON -DUSERVER_FEATURE_POSTGRESQL=ON \
        -DUSERVER_FEATURE_OTLP=ON -DUSERVER_FEATURE_CHAOTIC=ON -DUSERVER_FEATURE_CHAOTIC_OPENAPI=OFF
    cmake --build "$build_dir" -j "${BUILD_JOBS:-4}"
    cmake --install "$build_dir"
done
printf '%s\n' "$sanitizers" > "$prefix/broker-sanitizers.txt"
