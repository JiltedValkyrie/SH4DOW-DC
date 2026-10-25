# SH4DOW-DC
#
#   make test                       host check of the plain C path: test/shade_test, test/foot_test, the examples
#   make test-shz SH4ZAM=<checkout> host check of the SH4ZAM path (SH4ZAM's software back-end, 0.10 or later): test/shade_test, test/foot_test
#   make dc SH4ZAM=<checkout>       Dreamcast library build/libsh4dow_dc.a (KallistiOS environment sourced)
#   make clean

CC      ?= cc
CFLAGS  ?= -O2
WARN     = -std=c99 -Wall -Wextra
BUILD    = build
SRC      = src/sh4dow_dc.c src/sh4dow_foot.c

KOS_CC  ?= kos-cc
KOS_AR  ?= kos-ar
SH4ZAM  ?=
DC_FLAGS = -O2 -Wall -Wextra -Iinclude -I$(SH4ZAM)/include -ffast-math -ffp-contract=fast -mfsrra -mfsca -fno-math-errno

.PHONY: test test-shz dc clean

test: $(BUILD)/shade_test $(BUILD)/foot_test $(BUILD)/example $(BUILD)/quickstart $(BUILD)/feet
	$(BUILD)/shade_test
	$(BUILD)/foot_test
	$(BUILD)/example
	$(BUILD)/quickstart
	$(BUILD)/feet

$(BUILD)/%: $(SRC) include/sh4dow_dc.h
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) $(WARN) -DSH4DOW_DC_NO_SH4ZAM -Iinclude $(filter %.c,$^) -lm -o $@

$(BUILD)/shade_test: test/shade_test.c
$(BUILD)/foot_test: test/foot_test.c
$(BUILD)/example: example/example.c
$(BUILD)/quickstart: example/quickstart.c
$(BUILD)/feet: example/feet.c

test-shz: $(BUILD)/shz/shade_test $(BUILD)/shz/foot_test
	$(BUILD)/shz/shade_test
	$(BUILD)/shz/foot_test

$(BUILD)/shz/%: test/%.c $(SRC) include/sh4dow_dc.h
	@test -n "$(SH4ZAM)" || { echo "set SH4ZAM to your SH4ZAM checkout"; exit 1; }
	@mkdir -p $(BUILD)/shz
	$(CC) $(CFLAGS) -std=gnu11 -Wall -Wextra -DSH4DOW_TEST_TOL=0.0005f -Iinclude -I$(SH4ZAM)/include $(filter %.c,$^) $(SH4ZAM)/source/sw/shz_xmtrx_sw.c -lm -o $@

dc: $(BUILD)/libsh4dow_dc.a

$(BUILD)/libsh4dow_dc.a: $(SRC) include/sh4dow_dc.h
	@test -n "$(SH4ZAM)" || { echo "set SH4ZAM to your SH4ZAM checkout"; exit 1; }
	@mkdir -p $(BUILD)
	$(KOS_CC) $(DC_FLAGS) -c src/sh4dow_dc.c -o $(BUILD)/sh4dow_dc.o
	$(KOS_CC) $(DC_FLAGS) -c src/sh4dow_foot.c -o $(BUILD)/sh4dow_foot.o
	$(KOS_AR) rcs $@ $(BUILD)/sh4dow_dc.o $(BUILD)/sh4dow_foot.o

clean:
	rm -rf $(BUILD)
