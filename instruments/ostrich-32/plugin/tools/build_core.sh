#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Build 88emu (gearmulator/source/ronaldo/88emu, from our fork, unmodified beyond its
# `ostrich` branch) with gearmulator's own CMake as one static library, compile our core
# around it (plugin/core) into libmt32core.a, and build the mt32_render tool.
#
#   CXX, CC, AR, RC    compilers (mingw-w64 for Windows: cross-compiles)
#   MT32_CORE_BUILD    output folder, default plugin/build-core
#   MT32_JOBS          parallel jobs, default 5
#
# The plugin links $OUT/libmt32core.a and $OUT/88emu/.../lib88emu.a ($EMU88_LIB below), and
# on Linux -lpthread -ldl.
set -u
HERE="$(cd "$(dirname "$0")/../.." && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"   # the monorepo; submodules are in $ROOT/third_party
GM="$ROOT/third_party/gearmulator"
CORE="$HERE/plugin/core"

CXX="${CXX:-g++}"
CC="${CC:-gcc}"
AR="${AR:-ar}"
OUT="${MT32_CORE_BUILD:-$HERE/plugin/build-core}"
JOBS="${MT32_JOBS:-5}"
mkdir -p "$OUT/obj"

MACHINE="$("$CXX" -dumpmachine)"
case "$MACHINE" in *mingw*|*windows*) CROSS_WINDOWS=1 ;; *) CROSS_WINDOWS=0 ;; esac

# --- the parts of gearmulator 88emu needs ------------------------------------------------
# gearmulator's CMake configures its other products' libraries too, so these must be
# present even though 88emu links none of them; JUCE and the plugin SDKs are not needed.
# Shallow clones: about 660 MB instead of more than a gigabyte.
SUBS=(source/cpu/dsp56300 source/cpu/mc68k source/3rdparty/cpp-terminal source/3rdparty/RmlUi
      source/3rdparty/freetype source/3rdparty/portmidi-latest source/3rdparty/lunasvg)
if [ ! -f "$GM/CMakeLists.txt" ]; then
	echo "  third_party/gearmulator/ is empty: git -C \"$ROOT\" submodule update --init --depth 1 third_party/gearmulator"; exit 1
fi
for s in "${SUBS[@]}"; do
	if [ ! -e "$GM/$s/.git" ]; then
		printf '  %-34s ' "fetching gearmulator/$s"
		git -C "$GM" submodule update --init --depth 1 "$s" > "$OUT/submodules.log" 2>&1 \
			|| { echo "FAILED  (see $OUT/submodules.log)"; exit 1; }
		echo "ok"
	fi
done
for s in source/cpu/dsp56300 source/3rdparty/lunasvg; do
	git -C "$GM/$s" submodule update --init --depth 1 >> "$OUT/submodules.log" 2>&1 \
		|| { echo "  FAILED fetching the submodules of gearmulator/$s (see $OUT/submodules.log)"; exit 1; }
done

# --- 88emu, as gearmulator builds it ------------------------------------------------------
CMAKE_ARGS=(
	-S "$GM" -B "$OUT/88emu"
	-DCMAKE_BUILD_TYPE=Release
	-DCMAKE_POSITION_INDEPENDENT_CODE=ON
	-DCMAKE_C_COMPILER="$(command -v "$CC")"
	-DCMAKE_CXX_COMPILER="$(command -v "$CXX")"
	-DCMAKE_AR="$(command -v "$AR")"
	-Dgearmulator_BUILD_JUCEPLUGIN=OFF
	-Dgearmulator_BUILD_JUCEPLUGIN_CLAP=OFF
	-Dgearmulator_SYNTH_OSIRUS=OFF
	-Dgearmulator_SYNTH_OSTIRUS=OFF
	-Dgearmulator_SYNTH_VAVRA=OFF
	-Dgearmulator_SYNTH_XENIA=OFF
	-Dgearmulator_SYNTH_NODALRED2X=OFF
	-Dgearmulator_SYNTH_JE8086=OFF
	-Dgearmulator_COMPONENT_DSPBRIDGE=OFF
	-Dgearmulator_BUILD_INSTALLER=OFF
)
# mingw-w64 on Linux: header names as the Windows SDK spells them (plugin/tools/mingw) and
# Windows 10 as the target, for the memory-mapping calls dsp56300 uses.
WINFLAGS="-I$HERE/plugin/tools/mingw -D_WIN32_WINNT=0x0A00 -DNTDDI_VERSION=0x0A00000C"
if [ "$CROSS_WINDOWS" = 1 ]; then
	CMAKE_ARGS+=(-DCMAKE_SYSTEM_NAME=Windows
		-DCMAKE_RC_COMPILER="$(command -v "${RC:-${MACHINE}-windres}")"
		-DCMAKE_C_FLAGS="$WINFLAGS" -DCMAKE_CXX_FLAGS="$WINFLAGS")
fi
printf '  %-34s ' "88emu (gearmulator)"
if [ ! -f "$OUT/88emu/CMakeCache.txt" ]; then
	nice -n 19 cmake "${CMAKE_ARGS[@]}" > "$OUT/88emu-configure.log" 2>&1 \
		|| { echo "FAILED  (see $OUT/88emu-configure.log)"; exit 1; }
fi
if nice -n 19 cmake --build "$OUT/88emu" --target 88emu_bundle -j"$JOBS" > "$OUT/88emu-build.log" 2>&1; then
	echo "ok"
else
	echo "FAILED  (see $OUT/88emu-build.log)"; exit 1
fi
EMU88_LIB="$OUT/88emu/source/ronaldo/88emu/88lib/lib88emu.a"
# 88emu_bundle is a custom target: it writes lib88emu.a on every build, changed or not.
# This stamp is rewritten only when the library's content changes, and stands for it in
# the checks below, so that an unchanged 88emu does not relink the plugins.
EMU88_STAMP="$OUT/88emu.stamp"
EMU88_SUM="$(sha256sum "$EMU88_LIB" | cut -d" " -f1)"
if [ ! -f "$EMU88_STAMP" ] || [ "$(cat "$EMU88_STAMP")" != "$EMU88_SUM" ]; then
	echo "$EMU88_SUM" > "$EMU88_STAMP"
fi

# --- our core ----------------------------------------------------------------------------
CXXFLAGS=(-std=c++17 -O2 -g -Wall -I "$CORE" -I "$GM/source/ronaldo/88emu" -I "$GM/source/framework")
if [ "$CROSS_WINDOWS" = 1 ]; then CXXFLAGS+=($WINFLAGS); else CXXFLAGS+=(-fPIC); fi

fail=0
printf '  %-34s ' "plugin/core/mt32_core.cpp"
OBJ="$OUT/obj/mt32_core.o"
if [ -f "$OBJ" ] && [ "$OBJ" -nt "$CORE/mt32_core.cpp" ] && [ "$OBJ" -nt "$CORE/mt32_core.h" ] \
		&& [ "$OBJ" -nt "$EMU88_STAMP" ]; then
	echo "up to date"
elif nice -n 19 "$CXX" "${CXXFLAGS[@]}" -c "$CORE/mt32_core.cpp" -o "$OBJ" 2> "$OBJ.log"; then
	echo "ok"
else
	echo "FAILED  (see $OBJ.log)"; exit 1
fi
# Archive only when the object or 88emu changed: a newer library makes the Makefile relink
# the plugins, and it compares only this one (88emu is linked beside it).
LIB="$OUT/libmt32core.a"
if [ ! -f "$LIB" ] || [ "$OBJ" -nt "$LIB" ] || [ "$EMU88_STAMP" -nt "$LIB" ]; then
	rm -f "$LIB"
	"$AR" rcs "$LIB" "$OBJ" && echo "  archived                           $LIB"
else
	echo "  up to date                         $LIB"
fi

# --- mt32_render -------------------------------------------------------------------------
EXE=""
LIBS=(-lpthread -ldl)
if [ "$CROSS_WINDOWS" = 1 ]; then EXE=".exe"; LIBS=(-static -lpthread -lole32 -lshell32 -luuid -lws2_32 -lwinmm); fi
printf '  %-34s ' "mt32_render"
if nice -n 19 "$CXX" -std=c++17 -O2 -g -Wall -I "$CORE" "$HERE/plugin/tools/mt32_render.cpp" \
		"$OUT/libmt32core.a" "$EMU88_LIB" "${LIBS[@]}" -o "$OUT/mt32_render$EXE" \
		2> "$OUT/mt32_render.log"; then
	echo "built"
else
	echo "FAILED  (see $OUT/mt32_render.log)"
	fail=1
fi
exit $fail
