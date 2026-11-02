#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Compile Nuked-SC55's backend (nuked-sc55/src/backend, unmodified) and our core around it
# (plugin/core) into libsc55core.a, and build the sc55_render tool.  Nuked-SC55's own
# CMake build wants SDL2 and RtMidi for its frontends; the backend alone needs neither,
# so it is compiled here directly, the file list taken from its CMakeLists.txt.
#
#   CXX, CC, AR        compilers (mingw-w64 for Windows)
#   SC55_CORE_BUILD    output folder, default plugin/build-core
set -u
HERE="$(cd "$(dirname "$0")/../.." && pwd)"
NUKED="$HERE/nuked-sc55"
BACK="$NUKED/src/backend"

CXX="${CXX:-g++}"
CC="${CC:-gcc}"
AR="${AR:-ar}"
OUT="${SC55_CORE_BUILD:-$HERE/plugin/build-core}"
mkdir -p "$OUT/obj"

MACHINE="$("$CXX" -dumpmachine)"
case "$MACHINE" in *mingw*|*windows*) CROSS_WINDOWS=1 ;; *) CROSS_WINDOWS=0 ;; esac

# backend/config.h, as CMake's configure_file() makes it.  ASIO belongs to the frontend;
# decoder2 is Nuked-SC55's default.
VERSION="$(sed -n 's/^project *(nuked-sc55 VERSION \([0-9.]*\).*/\1/p' "$NUKED/CMakeLists.txt")"
IFS=. read -r VMAJ VMIN VPAT <<< "$VERSION"
SOURCE_ID="ostrich55 $(git -C "$NUKED" describe --tags --always 2>/dev/null || echo unknown)"
mkdir -p "$OUT/generated/backend"
CFG="$OUT/generated/backend/config.h"
sed -e 's/#cmakedefine01 NUKED_ENABLE_ASIO/#define NUKED_ENABLE_ASIO 0/' \
    -e 's/#cmakedefine01 NUKED_ENABLE_DECODER2/#define NUKED_ENABLE_DECODER2 1/' \
    -e "s/@CMAKE_PROJECT_VERSION@/$VERSION/" \
    -e "s/@NUKED_SOURCE@/$SOURCE_ID/" \
    -e "s/@CMAKE_PROJECT_VERSION_MAJOR@/$VMAJ/" \
    -e "s/@CMAKE_PROJECT_VERSION_MINOR@/$VMIN/" \
    -e "s/@CMAKE_PROJECT_VERSION_PATCH@/$VPAT/" \
    "$BACK/config.h.in" > "$CFG.new"
if cmp -s "$CFG.new" "$CFG"; then rm "$CFG.new"; else mv "$CFG.new" "$CFG"; fi

INCS=(-I "$BACK" -I "$OUT/generated/backend" -I "$NUKED/src" -I "$HERE/plugin/core")

SOURCES=(config diagnostics emu file_hashing file_io lcd mcu_interrupt mcu_opcodes mcu_timer
         mcu pcm rom_io rom standard_romsets submcu
         decoder2/address_modes decoder2/cache decoder2/disassemble
         decoder2/disassemble_handlers decoder2/disassemblers_address
         decoder2/disassemblers_imm16 decoder2/disassemblers_imm8 decoder2/disassemblers_rn
         decoder2/disassemblers_top decoder2/dispatch decoder2/dispatchers_aa
         decoder2/dispatchers_arn decoder2/dispatchers_d8d16_rn decoder2/dispatchers_imm16
         decoder2/dispatchers_postinc_rn decoder2/dispatchers_predec_rn
         decoder2/dispatchers_rn decoder2/dispatchers_top decoder2/instruction_handlers)

CXXFLAGS=(-std=c++23 -O2 -g -Wall -fno-strict-aliasing)
CFLAGS=(-O2 -g)
if [ "$CROSS_WINDOWS" = 1 ]; then
  CXXFLAGS+=(-mstackrealign -D__USE_MINGW_ANSI_STDIO=1)
else
  CXXFLAGS+=(-fPIC)
  CFLAGS+=(-fPIC)
fi

# An object is rebuilt when its source, any backend header or the core header is newer.
NEWEST_HEADER="$(ls -t "$BACK"/*.h "$BACK"/decoder2/*.h "$CFG" "$HERE/plugin/core/sc55_core.h" | head -1)"

OBJS=()
fail=0
compile() {   # compile <source> <object> <compiler and flags...>
  local src="$1" obj="$2"; shift 2
  printf '  %-34s ' "${src#$HERE/}"
  if [ -f "$obj" ] && [ "$obj" -nt "$src" ] && [ "$obj" -nt "$NEWEST_HEADER" ]; then
    echo "up to date"; OBJS+=("$obj"); return
  fi
  if "$@" -c "$src" -o "$obj" 2> "$obj.log"; then
    echo "ok"; OBJS+=("$obj")
  else
    echo "FAILED  ($(grep -c 'error' "$obj.log") errors, see $obj.log)"; fail=1
  fi
}

for s in "${SOURCES[@]}"; do
  compile "$BACK/$s.cpp" "$OUT/obj/${s//\//_}.o" "$CXX" "${CXXFLAGS[@]}" "${INCS[@]}"
done
compile "$BACK/sha/sha224-256.c" "$OUT/obj/sha224-256.o" "$CC" "${CFLAGS[@]}" -I "$BACK"
compile "$HERE/plugin/core/sc55_core.cpp" "$OUT/obj/sc55_core.o" "$CXX" "${CXXFLAGS[@]}" "${INCS[@]}"
[ $fail -ne 0 ] && exit 1

rm -f "$OUT/libsc55core.a"
"$AR" rcs "$OUT/libsc55core.a" "${OBJS[@]}" && echo "  archived                           $OUT/libsc55core.a"

EXE=""
LDFLAGS=()
if [ "$CROSS_WINDOWS" = 1 ]; then EXE=".exe"; LDFLAGS=(-static); fi
printf '  %-34s ' "sc55_render"
if "$CXX" -std=c++20 -O2 -g -Wall -I "$HERE/plugin/core" "$HERE/plugin/tools/sc55_render.cpp" \
        "$OUT/libsc55core.a" "${LDFLAGS[@]}" -o "$OUT/sc55_render$EXE" 2> "$OUT/sc55_render.log"; then
  echo "built"
else
  echo "FAILED  (see $OUT/sc55_render.log)"
  fail=1
fi
exit $fail
