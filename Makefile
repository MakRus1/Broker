SERVICE ?= broker
NPROCS ?= $(shell nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)
CLANG_FORMAT ?= clang-format
DOCKER_IMAGE ?= ghcr.io/userver-framework/ubuntu-22.04-userver-pg-dev:v2.15
DOCKER_ARGS = $(shell if [ -t 0 ]; then echo -it; fi)
DOCKER_UID ?= $(shell /usr/bin/id -u 2>/dev/null || id -u)
DOCKER_GID ?= $(shell /usr/bin/id -g 2>/dev/null || id -g)
PRESETS ?= debug release
DOCKER_COMPOSE := $(shell if docker compose version >/dev/null 2>&1; then echo "docker compose"; elif command -v docker-compose >/dev/null 2>&1; then echo "docker-compose"; fi)
# userver needs userfaultfd (StackUsageMonitor); same flags as .devcontainer/devcontainer.json
DOCKER_RUN_OPTS = --cap-add=SYS_PTRACE --security-opt seccomp=unconfined --network=host

.PHONY: check-docker-platform
check-docker-platform:
	@if [ "$$(uname -m)" = "arm64" ] && command -v colima >/dev/null 2>&1; then \
		ctx=$$(docker context show 2>/dev/null); \
		case "$$ctx" in \
			colima) profile=default ;; \
			colima-*) profile=$${ctx#colima-} ;; \
			*) profile= ;; \
		esac; \
		if [ -n "$$profile" ]; then \
			arch=$$(colima list 2>/dev/null | awk -v p="$$profile" '$$1 == p {print $$3}'); \
			if [ "$$arch" = "aarch64" ]; then \
				echo "Ошибка: Docker context $$ctx (Colima $$profile, $$arch) не может стабильно запускать amd64-образ userver." >&2; \
				echo "  colima start userver --arch x86_64 --memory 8" >&2; \
				echo "  docker context use colima-userver" >&2; \
				echo "  make dist-clean && make docker-test-debug" >&2; \
				exit 1; \
			fi; \
		fi; \
	fi

.PHONY: all
all: test-debug test-release

.PHONY: $(addprefix cmake-, $(PRESETS))
$(addprefix cmake-, $(PRESETS)): cmake-%:
	cmake --preset $*

$(addsuffix /CMakeCache.txt, $(addprefix build-, $(PRESETS))): build-%/CMakeCache.txt:
	$(MAKE) cmake-$*

.PHONY: $(addprefix build-, $(PRESETS))
$(addprefix build-, $(PRESETS)): build-%: build-%/CMakeCache.txt
	cmake --build build-$* -j $(NPROCS) --target $(SERVICE)

.PHONY: testsuite-clean
testsuite-clean:
	-$(MAKE) db-down-all
	-pkill -f '/tmp/.yasuite-user.*postgres' 2>/dev/null || true
	rm -rf /tmp/.yasuite-user
	@for svc in build-*/services/*/; do \
		rm -rf "$$svc/Testing/Temporary"; \
		mkdir -p "$$svc/Testing/Temporary"; \
	done

.PHONY: test-all-debug test-all-release
test-all-debug:
	@for svc in $$(find services -mindepth 1 -maxdepth 1 -type d -exec basename {} \; | sort); do \
		$(MAKE) test-debug SERVICE=$$svc || exit 1; \
	done

test-all-release:
	@for svc in $$(find services -mindepth 1 -maxdepth 1 -type d -exec basename {} \; | sort); do \
		$(MAKE) test-release SERVICE=$$svc || exit 1; \
	done

.PHONY: $(addprefix test-, $(PRESETS)) $(addprefix test-only-, $(PRESETS))
$(addprefix test-, $(PRESETS)): test-%: build-%/CMakeCache.txt
	$(MAKE) testsuite-clean
	$(MAKE) test-only-$*

$(addprefix test-only-, $(PRESETS)): test-only-%: build-%/CMakeCache.txt
	cmake --build build-$* -j $(NPROCS) --target $(SERVICE) $(SERVICE)_unittest $(SERVICE)_benchmark
	cd build-$* && ((test -t 1 && GTEST_COLOR=1 PYTEST_ADDOPTS="--color=yes" ctest -R '^($(SERVICE)_unittest|$(SERVICE)_benchmark|testsuite-$(SERVICE))$$' -V) || ctest -R '^($(SERVICE)_unittest|$(SERVICE)_benchmark|testsuite-$(SERVICE))$$' -V)
	pycodestyle services/$(SERVICE)/tests

.PHONY: $(addprefix start-, $(PRESETS))
$(addprefix start-, $(PRESETS)): start-%:
	cmake --build build-$* -v --target start-$(SERVICE)

.PHONY: $(addprefix clean-, $(PRESETS))
$(addprefix clean-, $(PRESETS)): clean-%:
	cmake --build build-$* --target clean

.PHONY: dist-clean
dist-clean:
	rm -rf build*
	rm -rf services/*/tests/__pycache__/
	rm -rf services/*/tests/.pytest_cache/
	rm -rf .ccache
	rm -rf .vscode/.cache
	rm -rf .vscode/compile_commands.json

.PHONY: $(addprefix install-, $(PRESETS))
$(addprefix install-, $(PRESETS)): install-%: build-%
	cmake --install build-$* -v --component $(SERVICE)

.PHONY: install
install: install-release

.PHONY: format
format:
	find libs services -name '*pp' -type f | xargs $(CLANG_FORMAT) -i
	find services -name '*.py' -type f | xargs autopep8 -i

.PHONY: deps-macos
deps-macos:
	./scripts/install-deps-macos.sh

.PHONY: db-up db-down db-up-all db-down-all
db-up:
	@test -n "$(DOCKER_COMPOSE)" || (echo "docker compose / docker-compose not found" >&2 && exit 1)
	$(DOCKER_COMPOSE) up -d postgres-$(SERVICE)

db-down:
	@if [ -n "$(DOCKER_COMPOSE)" ]; then $(DOCKER_COMPOSE) stop postgres-$(SERVICE); fi

db-up-all:
	@test -n "$(DOCKER_COMPOSE)" || (echo "docker compose / docker-compose not found" >&2 && exit 1)
	$(DOCKER_COMPOSE) up -d

db-down-all:
	@if [ -n "$(DOCKER_COMPOSE)" ]; then $(DOCKER_COMPOSE) down; fi

.PHONY: new-service
new-service:
	@test -n "$(NAME)" || (echo "Usage: make new-service NAME=my_service [POSTGRES=1 GRPC=1]" && exit 1)
	./scripts/create-service.sh $(NAME) $(if $(POSTGRES),--postgresql,) $(if $(GRPC),--grpc,)

DOCKER_MAKE_TARGETS := testsuite-clean \
	$(addprefix cmake-, $(PRESETS)) \
	$(addprefix build-, $(PRESETS)) \
	$(addprefix test-only-, $(PRESETS)) \
	$(addprefix test-, $(PRESETS)) \
	$(addprefix clean-, $(PRESETS)) \
	$(addprefix start-, $(PRESETS))

.PHONY: $(addprefix docker-, $(DOCKER_MAKE_TARGETS))
$(addprefix docker-, $(DOCKER_MAKE_TARGETS)): docker-%: check-docker-platform
	docker run $(DOCKER_ARGS) \
		$(DOCKER_RUN_OPTS) \
		-u $(DOCKER_UID):$(DOCKER_GID) \
		-v "$$PWD:$$PWD" \
		-w "$$PWD" \
		-e HOME="$$PWD/.docker-home" \
		-e CCACHE_DIR="$$PWD/.ccache" \
		-e SERVICE=$(SERVICE) \
		$(DOCKER_IMAGE) \
		sh -c 'mkdir -p "$$HOME" && make $*'
