.DEFAULT_GOAL := help
REPO_ROOT := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
include $(REPO_ROOT)/tools/versions.env
SERVICE ?= broker
PYTHON3 ?= $(shell command -v python3.12 || command -v python3)
NPROCS ?= 4
DOCKER_IMAGE ?= $(shell $(PYTHON3) $(REPO_ROOT)/scripts/container-images.py ref --variant $(if $(filter %-sanitize,$(MAKECMDGOALS)),sanitize,dev))
DOCKER_PLATFORM ?= $(shell docker version --format '{{.Server.Os}}/{{.Server.Arch}}')
DOCKER_VOLUME_PREFIX = broker-$(subst /,-,$(DOCKER_PLATFORM))
# Linux-окружение и результаты сборки не смешиваются с нативными файлами хоста.
DOCKER_STATE_VOLUMES = $(foreach dir,.venv-gen build-debug build-release build-sanitize,-v $(DOCKER_VOLUME_PREFIX)-$(subst .,_,$(dir)):/workspaces/broker/$(dir))
PRESETS := debug release sanitize
.PHONY: $(addprefix cmake-,$(PRESETS)) $(addprefix build-,$(PRESETS)) $(addprefix test-,$(PRESETS)) $(addprefix test-only-,$(PRESETS)) $(addprefix test-all-,$(PRESETS)) $(addprefix run-,$(PRESETS)) $(addprefix start-,$(PRESETS))

.PHONY: format check-format help gen check-gen check-gen-all test-tools doctor docker-image docker-pull lock-images new-service testsuite-clean
help:
	@echo 'make doctor | gen | check-gen-all | test-tools | cmake-debug | test-debug | docker-pull | docker-image | docker-test-debug | lock-images'

gen:
	@PYTHON=$$(PYTHON3="$(PYTHON3)" scripts/openapi_gen/ensure_venv.sh); "$$PYTHON" scripts/openapi_gen/generate.py --service-dir "services/$(SERVICE)"
check-gen:
	@PYTHON=$$(PYTHON3="$(PYTHON3)" scripts/openapi_gen/ensure_venv.sh); "$$PYTHON" scripts/openapi_gen/generate.py --service-dir "services/$(SERVICE)" --check
check-gen-all:
	@set -e; for svc in services/*; do $(MAKE) check-gen SERVICE=$$(basename "$$svc"); done
	@PYTHON=$$(PYTHON3="$(PYTHON3)" scripts/openapi_gen/ensure_venv.sh); "$$PYTHON" scripts/check-config.py && "$$PYTHON" scripts/openapi_gen/generate.py --service-dir tests/fixtures/contract_service --check

test-tools:
	@PYTHON=$$(PYTHON3="$(PYTHON3)" scripts/openapi_gen/ensure_venv.sh); "$$PYTHON" -m unittest discover -s tests -v

format check-format:
	@PYTHON=$$(PYTHON3="$(PYTHON3)" scripts/openapi_gen/ensure_venv.sh); "$$PYTHON" scripts/check-format.py $(if $(filter format,$@),--fix,)

doctor:
	@$(PYTHON3) scripts/doctor.py

$(addprefix cmake-,$(PRESETS)): cmake-%:
	@PYTHON3="$(PYTHON3)" scripts/openapi_gen/ensure_venv.sh >/dev/null
	$(REPO_ROOT)/.venv-gen/bin/cmake --preset $* -DCMAKE_MAKE_PROGRAM=$(REPO_ROOT)/.venv-gen/bin/ninja -DPython3_EXECUTABLE=$$(PYTHON3="$(PYTHON3)" scripts/openapi_gen/ensure_venv.sh)
$(addprefix build-,$(PRESETS)): build-%: cmake-%
	$(REPO_ROOT)/.venv-gen/bin/cmake --build build-$* -j $(NPROCS) --target $(SERVICE)
$(addprefix test-,$(PRESETS)): test-%: cmake-%
	$(REPO_ROOT)/.venv-gen/bin/cmake --build build-$* -j $(NPROCS)
	$(REPO_ROOT)/.venv-gen/bin/ctest --test-dir build-$* --output-on-failure --no-tests=error -R '^($(SERVICE)_unittest|testsuite-$(SERVICE)|testsuite-contract_service|client_contract_unittest)$$'
$(addprefix test-only-,$(PRESETS)): test-only-%: test-%
$(addprefix test-all-,$(PRESETS)): test-all-%: cmake-%
	$(REPO_ROOT)/.venv-gen/bin/cmake --build build-$* -j $(NPROCS)
	$(REPO_ROOT)/.venv-gen/bin/ctest --test-dir build-$* --output-on-failure --no-tests=error
$(addprefix run-,$(PRESETS)): run-%: build-%
	./build-$*/services/$(SERVICE)/$(SERVICE) -c services/$(SERVICE)/configs/static_config.yaml --config_vars services/$(SERVICE)/configs/config_vars.yaml
$(addprefix start-,$(PRESETS)): start-%: cmake-%
	$(REPO_ROOT)/.venv-gen/bin/cmake --build build-$* --target start-$(SERVICE)

new-service:
	@PYTHON=$$(PYTHON3="$(PYTHON3)" scripts/openapi_gen/ensure_venv.sh); "$$PYTHON" scripts/create_service.py "$(NAME)" $(if $(filter 1,$(POSTGRES)),--postgresql,) $(if $(filter 1,$(GRPC)),--grpc,)

lock-images:
	$(PYTHON3) scripts/container-images.py lock

docker-pull:
	docker pull --platform $(DOCKER_PLATFORM) $(DOCKER_IMAGE)

docker-image:
	docker build --platform $(DOCKER_PLATFORM) --target dev -f tools/Dockerfile -t $(BROKER_IMAGE) .
docker-%:
	docker run --rm --init --platform $(DOCKER_PLATFORM) --network=host $(DOCKER_STATE_VOLUMES) -e ASAN_OPTIONS -e UBSAN_OPTIONS -v "$(REPO_ROOT):/workspaces/broker" -w /workspaces/broker -e SERVICE=$(SERVICE) $(DOCKER_IMAGE) make $* SERVICE=$(SERVICE) NPROCS=$(NPROCS)

.PHONY: db-up db-down testsuite-clean dist-clean
db-up:
	docker compose up -d postgres-$(SERVICE)
db-down:
	docker compose stop postgres-$(SERVICE)
# Testsuite самостоятельно управляет своими процессами; чужие PostgreSQL не останавливаем.
testsuite-clean:
	@echo 'Testsuite очищает свои процессы при завершении; локальные БД не затрагиваются.'
dist-clean:
	rm -rf build-debug build-release build-sanitize
