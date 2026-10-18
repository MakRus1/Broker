# Все команды выполняются единообразно из корня монорепозитория.
SERVICE := admin_bff
ROOT := $(abspath ../..)
.DEFAULT_GOAL := help
%:
	$(MAKE) -C $(ROOT) $@ SERVICE=$(SERVICE)
