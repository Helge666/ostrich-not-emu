/* SPDX-License-Identifier: GPL-3.0-or-later
 * Ostrich 32, mingw-w64 cross builds only: gearmulator includes the Windows SDK's
 * <shlobj_core.h>; mingw-w64 has its declarations in <shlobj.h>. */
#include_next <shlobj.h>
