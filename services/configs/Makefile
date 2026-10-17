# Все команды выполняются единообразно из корня монорепозитория.
SERVICE := configs
ROOT := $(abspath ../..)
.DEFAULT_GOAL := help
%:
	$(MAKE) -C $(ROOT) $@ SERVICE=$(SERVICE)
