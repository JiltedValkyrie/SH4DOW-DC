# SH4DOW-DC
#
#   make test                  host check of the plain C path: test/shade_test, example/example, example/quickstart
#   make dc SH4ZAM=<checkout>  Dreamcast library build/libsh4dow_dc.a (KallistiOS environment sourced)
#   make clean

CC      ?= cc
CFLAGS  ?= -O2
WARN     = -std=c99 -Wall -Wextra
BUILD    = build

KOS_CC  ?= kos-cc
KOS_AR  ?= kos-ar
SH4ZAM  ?=
DC_FLAGS = -O2 -Wall -Wextra -Iinclude -I$(SH4ZAM)/include -ffast-math -ffp-contract=fast -mfsrra -mfsca -fno-math-errno

.PHONY: test dc clean

test: $(BUILD)/shade_test $(BUILD)/example $(BUILD)/quickstart
	$(BUILD)/shade_test
	$(BUILD)/example
	$(BUILD)/quickstart

$(BUILD)/%: src/sh4dow_dc.c include/sh4dow_dc.h
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) $(WARN) -DSH4DOW_DC_NO_SH4ZAM -Iinclude $(filter %.c,$^) -lm -o $@

$(BUILD)/shade_test: test/shade_test.c
$(BUILD)/example: example/example.c
$(BUILD)/quickstart: example/quickstart.c

dc: $(BUILD)/libsh4dow_dc.a

$(BUILD)/libsh4dow_dc.a: src/sh4dow_dc.c include/sh4dow_dc.h
	@test -n "$(SH4ZAM)" || { echo "set SH4ZAM to your SH4ZAM checkout"; exit 1; }
	@mkdir -p $(BUILD)
	$(KOS_CC) $(DC_FLAGS) -c src/sh4dow_dc.c -o $(BUILD)/sh4dow_dc.o
	$(KOS_AR) rcs $@ $(BUILD)/sh4dow_dc.o

clean:
	rm -rf $(BUILD)
