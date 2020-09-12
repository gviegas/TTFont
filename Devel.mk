#!/usr/bin/env -vS make -f

#
# Font
# Devel.mk - Development Makefile.
#
# Copyright (C) 2020 Gustavo C. Viegas.
#

SHELL := /bin/sh
.SUFFIXES: .cc .o .d

INCLUDE_DIR := include/
SRC_DIR := src/
BIN_DIR := bin/
BUILD_DIR := build/
TEST_DIR := test/

SRC := \
  $(wildcard $(SRC_DIR)*.cc) \
  $(wildcard $(TEST_DIR)*.cc)

OBJ := $(subst $(SRC_DIR),$(BUILD_DIR),$(SRC:.cc=.o))
OBJ := $(subst $(TEST_DIR),$(BUILD_DIR),$(OBJ))

DEP := $(OBJ:.o=.d)

CXX := /usr/bin/clang++
CXX_FLAGS := -std=gnu++17 -Wpedantic -Wall -Wextra -Og

LD_LIBS := -lyf
LD_FLAGS := \
  -iquote $(INCLUDE_DIR) \
  -iquote $(SRC_DIR)

PP := $(CXX) -E
PP_FLAGS := -D FONT_DEVEL

OUT := $(BIN_DIR)Devel

devel: $(OBJ)
	$(CXX) $(CXX_FLAGS) $(LD_FLAGS) $^ $(LD_LIBS) -o $(OUT)

-include $(DEP)

.PHONY: clean-out
clean-out:
	rm -f $(OUT)

.PHONY: clean-obj
clean-obj:
	rm -f $(OBJ)

.PHONY: clean-dep
clean-dep:
	rm -f $(DEP)

.PHONY: clean
clean: clean-out clean-obj clean-dep

$(BUILD_DIR)%.o: $(SRC_DIR)%.cc
	$(CXX) $(CXX_FLAGS) $(LD_FLAGS) $(PP_FLAGS) -c $< -o $@

$(BUILD_DIR)%.o: $(TEST_DIR)%.cc
	$(CXX) $(CXX_FLAGS) $(LD_FLAGS) $(PP_FLAGS) -c $< -o $@

$(BUILD_DIR)%.d: $(SRC_DIR)%.cc
	@$(PP) $(LD_FLAGS) $(PP_FLAGS) $< -MM -MT $(@:.d=.o) > $@

$(BUILD_DIR)%.d: $(TEST_DIR)%.cc
	@$(PP) $(LD_FLAGS) $(PP_FLAGS) $< -MM -MT $(@:.d=.o) > $@
