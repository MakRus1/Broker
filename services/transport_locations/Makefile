# Все команды выполняются единообразно из корня монорепозитория.
SERVICE := transport_locations
ROOT := $(abspath ../..)
.DEFAULT_GOAL := help
%:
	$(MAKE) -C $(ROOT) $@ SERVICE=$(SERVICE)
