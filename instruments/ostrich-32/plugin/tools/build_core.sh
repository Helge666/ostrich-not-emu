#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Build Munt's libmt32emu (munt/mt32emu, unmodified) with its own CMake as a static
# library, compile our core around it (plugin/core) into libmt32core.a, and build the
# mt32_render tool.
#
#   CXX, CC, AR        compilers (mingw-w64 for Windows: cross-compiles)
#   MT32_CORE_BUILD    output folder, default plugin/build-core
#
# The plugin links $OUT/libmt32core.a and $OUT/munt/libmt32emu.a.
set -u
HERE="$(cd "$(dirname "$0")/../.." && pwd)"
MUNT="$HERE/munt/mt32emu"
CORE="$HERE/plugin/core"

CXX="${CXX:-g++}"
CC="${CC:-gcc}"
AR="${AR:-ar}"
OUT="${MT32_CORE_BUILD:-$HERE/plugin/build-core}"
mkdir -p "$OUT/obj"

MACHINE="$("$CXX" -dumpmachine)"
case "$MACHINE" in *mingw*|*windows*) CROSS_WINDOWS=1 ;; *) CROSS_WINDOWS=0 ;; esac

# --- libmt32emu, as Munt builds it -------------------------------------------------------
# Static, C++ interface (a static build has it anyway), no C interface, built-in resampler
# (no libsoxr or libsamplerate to ship), no tests.
CMAKE_ARGS=(
	-S "$MUNT" -B "$OUT/munt"
	-DCMAKE_BUILD_TYPE=Release
	-DCMAKE_POSITION_INDEPENDENT_CODE=ON
	-DCMAKE_C_COMPILER="$(command -v "$CC")"
	-DCMAKE_CXX_COMPILER="$(command -v "$CXX")"
	-DCMAKE_AR="$(command -v "$AR")"
	-Dlibmt32emu_SHARED=OFF
	-Dlibmt32emu_C_INTERFACE=OFF
	-Dlibmt32emu_CPP_INTERFACE=ON
	-Dlibmt32emu_WITH_INTERNAL_RESAMPLER=ON
	-DBUILD_TESTING=OFF
)
if [ "$CROSS_WINDOWS" = 1 ]; then
	CMAKE_ARGS+=(-DCMAKE_SYSTEM_NAME=Windows -DCMAKE_CXX_FLAGS="-mstackrealign")
fi
printf '  %-34s ' "libmt32emu (munt/mt32emu)"
if [ ! -f "$OUT/munt/CMakeCache.txt" ]; then
	cmake "${CMAKE_ARGS[@]}" > "$OUT/munt-configure.log" 2>&1 \
		|| { echo "FAILED  (see $OUT/munt-configure.log)"; exit 1; }
fi
if cmake --build "$OUT/munt" -j"${MT32_JOBS:-5}" > "$OUT/munt-build.log" 2>&1; then
	echo "ok"
else
	echo "FAILED  (see $OUT/munt-build.log)"; exit 1
fi
MUNT_LIB="$OUT/munt/libmt32emu.a"
MUNT_INC="$OUT/munt/include"

# --- our core ----------------------------------------------------------------------------
CXXFLAGS=(-std=c++17 -O2 -g -Wall -I "$CORE" -I "$MUNT_INC")
if [ "$CROSS_WINDOWS" = 1 ]; then CXXFLAGS+=(-mstackrealign); else CXXFLAGS+=(-fPIC); fi

fail=0
printf '  %-34s ' "plugin/core/mt32_core.cpp"
OBJ="$OUT/obj/mt32_core.o"
if [ -f "$OBJ" ] && [ "$OBJ" -nt "$CORE/mt32_core.cpp" ] && [ "$OBJ" -nt "$CORE/mt32_core.h" ] \
		&& [ "$OBJ" -nt "$MUNT_LIB" ]; then
	echo "up to date"
elif "$CXX" "${CXXFLAGS[@]}" -c "$CORE/mt32_core.cpp" -o "$OBJ" 2> "$OBJ.log"; then
	echo "ok"
else
	echo "FAILED  (see $OBJ.log)"; exit 1
fi
rm -f "$OUT/libmt32core.a"
"$AR" rcs "$OUT/libmt32core.a" "$OBJ" && echo "  archived                           $OUT/libmt32core.a"

# --- mt32_render -------------------------------------------------------------------------
EXE=""
LDFLAGS=()
if [ "$CROSS_WINDOWS" = 1 ]; then EXE=".exe"; LDFLAGS=(-static); fi
printf '  %-34s ' "mt32_render"
if "$CXX" -std=c++17 -O2 -g -Wall -I "$CORE" "$HERE/plugin/tools/mt32_render.cpp" \
		"$OUT/libmt32core.a" "$MUNT_LIB" "${LDFLAGS[@]}" -o "$OUT/mt32_render$EXE" \
		2> "$OUT/mt32_render.log"; then
	echo "built"
else
	echo "FAILED  (see $OUT/mt32_render.log)"
	fail=1
fi
exit $fail
