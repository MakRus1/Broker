# Все команды выполняются единообразно из корня монорепозитория.
SERVICE := web_bff
ROOT := $(abspath ../..)
.DEFAULT_GOAL := help
%:
	$(MAKE) -C $(ROOT) $@ SERVICE=$(SERVICE)
