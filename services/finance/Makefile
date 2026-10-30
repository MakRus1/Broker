# Все команды выполняются единообразно из корня монорепозитория.
SERVICE := finance
ROOT := $(abspath ../..)
.DEFAULT_GOAL := help
%:
	$(MAKE) -C $(ROOT) $@ SERVICE=$(SERVICE)
