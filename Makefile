# -----------------------------------------------------------------------------
# Validation
# -----------------------------------------------------------------------------

GOAL_COUNT := $(words $(MAKECMDGOALS))

ifneq ($(GOAL_COUNT),1)
    ifneq ($(GOAL_COUNT),0)
        $(error You cannot specify more than 1 target (got $(GOAL_COUNT): $(MAKECMDGOALS)))
    endif
endif

# -----------------------------------------------------------------------------
# Global Settings
# -----------------------------------------------------------------------------

PREFIX ?= /usr/local

OPT ?= 2

CC = gcc
MAKE = make

# -----------------------------------------------------------------------------
# Private Settings
# -----------------------------------------------------------------------------

LIB_NAME = sarena
HEADERS = include/sarena.h

INSTALL_INCLUDE = include/*

OPT_FLAG = -O$(OPT)

DEBUG ?= 1
ifeq ($(DEBUG),1)
    DEBUG_FLAG = -g
    OPT_FLAG = -O0
endif

# -----------------------------------------------------------------------------
# Build Flags
# -----------------------------------------------------------------------------

DEP_CFLAGS =
DEP_LFLAGS =

# ---------------------------------------------------------
# Test Flags
# ---------------------------------------------------------

TEST_CFLAGS_DEBUG = $(DEBUG_FLAG)
TEST_CFLAGS_OPTIMIZATION = -O0
TEST_CFLAGS_WARN = -Wall
TEST_CFLAGS_MAKE = -MMD -MP
TEST_CFLAGS_INCLUDE = -Iinclude $(DEP_CFLAGS)

TEST_CFLAGS = -c $(TEST_CFLAGS_INCLUDE) $(TEST_CFLAGS_MAKE) \
$(TEST_CFLAGS_WARN) $(TEST_CFLAGS_DEBUG) $(TEST_CFLAGS_OPTIMIZATION)

TEST_LFLAGS =

# -----------------------------------------------------------------------------
# Targets
# -----------------------------------------------------------------------------

.PHONY: all clean install uninstall

all:

# demo -----------------------------------------------------

demo: build/demo.o
	@mkdir -p build
	$(CC) build/demo.o -o $@ $(TEST_LFLAGS)

build/demo.o: demo.c $(HEADERS)
	@mkdir -p $(dir $@)
	$(CC) $(TEST_CFLAGS) demo.c -o $@

# install --------------------------------------------------

install: $(HEADERS)
	@mkdir -p $(PREFIX)/include/$(LIB_NAME) # headers
	cp -r $(INSTALL_INCLUDE) $(PREFIX)/include/$(LIB_NAME)

# uninstall ------------------------------------------------

uninstall:
	rm -rf $(PREFIX)/include/$(LIB_NAME)

# clean ----------------------------------------------------

clean:
	rm -rf build/
	rm -f demo
