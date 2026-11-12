/* SPDX-License-Identifier: GPL-3.0-or-later
 * Ostrich 32, mingw-w64 cross builds only: gearmulator includes <Windows.h>, mingw-w64
 * ships it as <windows.h>, and Linux file names are case-sensitive. */
#include_next <windows.h>   /* the next folder on the include path: never this file again */
