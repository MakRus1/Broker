.DEFAULT_GOAL := help
REPO_ROOT := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
include $(REPO_ROOT)/tools/versions.env
SERVICE ?= broker
PYTHON3 ?= $(shell command -v python3.12 || command -v python3)
NPROCS ?= 4
TEST_SHARED_CONTRACTS ?= 0
PRESETS := debug release sanitize
CMAKE_SERVICES ?= $(SERVICE)
CMAKE_SHARED_CONTRACTS ?= $(if $(filter 1,$(TEST_SHARED_CONTRACTS)),ON,OFF)
$(addprefix test-all-,$(PRESETS)): CMAKE_SERVICES = all
$(addprefix test-all-,$(PRESETS)): CMAKE_SHARED_CONTRACTS = ON
$(addprefix ci-build-,$(PRESETS)): CMAKE_SERVICES = $(CI_BUILD_SERVICES)
$(addprefix ci-build-,$(PRESETS)): CMAKE_SHARED_CONTRACTS = $(if $(filter false,$(CI_SHARED_CONTRACTS)),OFF,ON)
DOCKER_IMAGE ?= $(shell $(PYTHON3) $(REPO_ROOT)/scripts/container-images.py ref --variant $(if $(filter %-sanitize,$(MAKECMDGOALS)),sanitize,dev))
DOCKER_PLATFORM ?= $(shell docker version --format '{{.Server.Os}}/{{.Server.Arch}}')
DOCKER_VOLUME_PREFIX = broker-$(subst /,-,$(DOCKER_PLATFORM))
# Linux-окружение и результаты сборки не смешиваются с нативными файлами хоста.
DOCKER_STATE_VOLUMES = $(foreach dir,.venv-gen build-debug build-release build-sanitize,-v $(DOCKER_VOLUME_PREFIX)-$(subst .,_,$(dir)):/workspaces/broker/$(dir))
.PHONY: $(addprefix cmake-,$(PRESETS)) $(addprefix build-,$(PRESETS)) $(addprefix test-,$(PRESETS)) $(addprefix test-only-,$(PRESETS)) $(addprefix test-all-,$(PRESETS)) $(addprefix run-,$(PRESETS)) $(addprefix start-,$(PRESETS))

.PHONY: format check-format help gen check-gen check-gen-all test-tools doctor docker-image docker-pull lock-images new-service testsuite-clean
help:
	@echo 'make local-update SERVICE=shipments | doctor | gen | check-gen-all | test-tools | cmake-debug | test-debug | docker-pull | docker-image | docker-test-debug | lock-images'

gen:
	@PYTHON=$$(PYTHON3="$(PYTHON3)" scripts/openapi_gen/ensure_venv.sh); "$$PYTHON" scripts/openapi_gen/export_clients.py --service-dir "services/$(SERVICE)"
	@PYTHON=$$(PYTHON3="$(PYTHON3)" scripts/openapi_gen/ensure_venv.sh); "$$PYTHON" scripts/openapi_gen/generate.py --service-dir "services/$(SERVICE)"
check-gen:
	@PYTHON=$$(PYTHON3="$(PYTHON3)" scripts/openapi_gen/ensure_venv.sh); "$$PYTHON" scripts/openapi_gen/generate.py --service-dir "services/$(SERVICE)" --check
check-gen-all:
	@PYTHON=$$(PYTHON3="$(PYTHON3)" scripts/openapi_gen/ensure_venv.sh); "$$PYTHON" scripts/openapi_gen/export_clients.py --check
	@set -e; for svc in services/*; do $(MAKE) check-gen SERVICE=$$(basename "$$svc"); done
	@PYTHON=$$(PYTHON3="$(PYTHON3)" scripts/openapi_gen/ensure_venv.sh); "$$PYTHON" scripts/check-migrations.py && "$$PYTHON" scripts/check-config.py && "$$PYTHON" scripts/openapi_gen/generate.py --service-dir tests/fixtures/contract_service --check

test-tools:
	@PYTHON=$$(PYTHON3="$(PYTHON3)" scripts/openapi_gen/ensure_venv.sh); "$$PYTHON" scripts/ci-tools.py --full

format check-format:
	@PYTHON=$$(PYTHON3="$(PYTHON3)" scripts/openapi_gen/ensure_venv.sh); "$$PYTHON" scripts/check-format.py $(if $(filter format,$@),--fix,)

doctor:
	@$(PYTHON3) scripts/doctor.py

$(addprefix cmake-,$(PRESETS)): cmake-%:
	@PYTHON3="$(PYTHON3)" scripts/openapi_gen/ensure_venv.sh >/dev/null
	$(REPO_ROOT)/.venv-gen/bin/cmake --preset $* -DBROKER_SERVICES="$(CMAKE_SERVICES)" -DBROKER_SHARED_CONTRACTS=$(CMAKE_SHARED_CONTRACTS) -DCMAKE_MAKE_PROGRAM=$(REPO_ROOT)/.venv-gen/bin/ninja -DPython3_EXECUTABLE=$$(PYTHON3="$(PYTHON3)" scripts/openapi_gen/ensure_venv.sh)
$(addprefix build-,$(PRESETS)): build-%: cmake-%
	$(REPO_ROOT)/.venv-gen/bin/cmake --build build-$* -j $(NPROCS) --target $(SERVICE)
$(addprefix test-,$(PRESETS)): test-%: cmake-%
	$(REPO_ROOT)/.venv-gen/bin/python scripts/ci-tests.py build --build-dir build-$* --services "$(SERVICE)" --jobs $(NPROCS) $(if $(filter 1,$(TEST_SHARED_CONTRACTS)),--shared-contracts,--no-shared-contracts)
	$(REPO_ROOT)/.venv-gen/bin/ctest --test-dir build-$* --output-on-failure --no-tests=error -R '^($(SERVICE)_unittest|testsuite-$(SERVICE)$(if $(filter 1,$(TEST_SHARED_CONTRACTS)),|testsuite-contract_service|client_contract_unittest,))$$'
# CI собирает один раз на окружение; тесты выбранных сервисов запускаются отдельно.
.PHONY: $(addprefix ci-build-,$(PRESETS)) $(addprefix ci-test-,$(PRESETS))
$(addprefix ci-build-,$(PRESETS)): ci-build-%: cmake-%
	$(REPO_ROOT)/.venv-gen/bin/python scripts/ci-tests.py build --build-dir build-$* --services $(CI_BUILD_SERVICES) --jobs $(NPROCS) $(if $(filter false,$(CI_SHARED_CONTRACTS)),--no-shared-contracts,)
$(addprefix ci-test-,$(PRESETS)): ci-test-%:
	$(REPO_ROOT)/.venv-gen/bin/python scripts/ci-tests.py run --build-dir build-$* --services $(CI_SERVICES) --output-dir "$(CI_RESULTS_DIR)" $(if $(filter false,$(CI_SHARED_CONTRACTS)),--no-shared-contracts,)

SMOKE_BUILD_DIR ?= build-$*
.PHONY: $(addprefix ci-smoke-,$(PRESETS))
$(addprefix ci-smoke-,$(PRESETS)): ci-smoke-%:
	$(abspath $(SMOKE_BUILD_DIR))/venv-userver-default/bin/python tests/integration/logistics_smoke.py --build-dir "$(SMOKE_BUILD_DIR)"

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
	docker run --rm --init --platform $(DOCKER_PLATFORM) --network=host $(DOCKER_STATE_VOLUMES) -e CCACHE_DIR=/workspaces/broker/.ccache -e CCACHE_BASEDIR=/workspaces/broker -e CCACHE_MAXSIZE=512M -e CCACHE_COMPILERCHECK=content -e ASAN_OPTIONS -e UBSAN_OPTIONS -e CI_SERVICES -e CI_BUILD_SERVICES -e CI_RESULTS_DIR -e CI_SHARED_CONTRACTS -v "$(REPO_ROOT):/workspaces/broker" -w /workspaces/broker -e SERVICE=$(SERVICE) $(DOCKER_IMAGE) make $* SERVICE=$(SERVICE) NPROCS=$(NPROCS) TEST_SHARED_CONTRACTS=$(TEST_SHARED_CONTRACTS)

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

.PHONY: local-up local-down local-status local-logs local-check local-migrate
local-up local-down local-status local-logs local-check local-migrate:
	@PYTHON=$$(PYTHON3="$(PYTHON3)" scripts/openapi_gen/ensure_venv.sh); "$$PYTHON" scripts/local-env.py $(patsubst local-%,%,$@)

.PHONY: export-client-specs check-client-specs
export-client-specs check-client-specs:
	@PYTHON=$$(PYTHON3="$(PYTHON3)" scripts/openapi_gen/ensure_venv.sh); "$$PYTHON" scripts/openapi_gen/export_clients.py $(if $(filter command line,$(origin SERVICE)),--service-dir "services/$(SERVICE)",) $(if $(filter check-client-specs,$@),--check,)

.PHONY: local-update
local-update:
	@PYTHON=$$(PYTHON3="$(PYTHON3)" scripts/openapi_gen/ensure_venv.sh); "$$PYTHON" scripts/local-env.py update $(if $(filter file default undefined,$(origin SERVICE)),,--service "$(SERVICE)") --jobs $(NPROCS)
