#!/usr/bin/make -f
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Ostrich 55, the SC-55 plugin.  DPF is plugin/dpf, our fork Helge666/DPF; the core is
# Nuked-SC55's backend plus plugin/core, built by ../tools/build_core.sh into a static
# library.
#
#   make          Linux: VST3, CLAP, standalone (JACK)
#   make win      Windows, cross-compiled with mingw-w64: VST3, CLAP, standalone

NAME = Ostrich55

FILES_DSP = OstrichPlugin.cpp MidiInput.cpp
FILES_UI  = OstrichUI.cpp

include ../dpf/Makefile.plugins.mk

CORE_LIB ?= ../build-core/libsc55core.a

BUILD_CXX_FLAGS := -I. -I../core $(BUILD_CXX_FLAGS) -std=c++20
LINK_FLAGS      += $(abspath $(CORE_LIB))

ifeq ($(WINDOWS),true)
EXTRA_LIBS      += -lpthread -lwinmm
endif

TARGETS = vst3 clap jack

all:
	@../tools/build_core.sh
	@for f in bin/$(NAME).clap bin/$(NAME) bin/$(NAME).vst3/Contents/x86_64-linux/$(NAME).so; do \
		if [ -e "$$f" ] && [ "$(CORE_LIB)" -nt "$$f" ]; then rm -f "$$f"; fi; done
	@$(MAKE) --no-print-directory $(TARGETS)

CROSS_PREFIX ?= x86_64-w64-mingw32-
WIN_BUILD    = $(CURDIR)/build-win
WIN_BIN      = $(CURDIR)/bin-win
WIN_PRODUCTS = $(WIN_BIN)/$(NAME).clap \
               $(WIN_BIN)/$(NAME).vst3/Contents/x86_64-win/$(NAME).vst3 \
               $(WIN_BIN)/$(NAME).exe

win:
	@SC55_CORE_BUILD=$(WIN_BUILD)/core CXX=$(CROSS_PREFIX)g++ CC=$(CROSS_PREFIX)gcc AR=$(CROSS_PREFIX)ar \
		../tools/build_core.sh
	@for f in $(WIN_PRODUCTS); do \
		if [ -e "$$f" ] && [ "$(WIN_BUILD)/core/libsc55core.a" -nt "$$f" ]; then rm -f "$$f"; fi; done
	@$(MAKE) --no-print-directory clap vst3 jack \
		CC=$(CROSS_PREFIX)gcc CXX=$(CROSS_PREFIX)g++ AR=$(CROSS_PREFIX)ar \
		BUILD_DIR_SUFFIX=-win \
		DPF_BUILD_DIR=$(WIN_BUILD)/$(NAME) \
		DPF_TARGET_DIR=$(WIN_BIN) \
		CORE_LIB=$(WIN_BUILD)/core/libsc55core.a
	@file $(WIN_PRODUCTS)

.PHONY: all win
