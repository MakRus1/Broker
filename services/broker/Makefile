SERVICE := broker
ROOT := $(abspath $(dir $(lastword $(MAKEFILE_LIST)))/../..)

.DEFAULT:
	$(MAKE) -C $(ROOT) SERVICE=$(SERVICE) $(MAKECMDGOALS)
