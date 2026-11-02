# Все команды выполняются единообразно из корня монорепозитория.
SERVICE := user_preferences
ROOT := $(abspath ../..)
.DEFAULT_GOAL := help
%:
	$(MAKE) -C $(ROOT) $@ SERVICE=$(SERVICE)
