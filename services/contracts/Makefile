# Все команды выполняются единообразно из корня монорепозитория.
SERVICE := contracts
ROOT := $(abspath ../..)
.DEFAULT_GOAL := help
%:
	$(MAKE) -C $(ROOT) $@ SERVICE=$(SERVICE)
