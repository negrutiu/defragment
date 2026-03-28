#pragma once

// Target platform
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0500    //_WIN32_WINNT_WIN2K
#endif

#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include <tchar.h>
#include <stdlib.h>

#include <windows.h>

#include <strsafe.h>

#include <assert.h>
#if defined(_DEBUG) || defined(DBG)
#define verify(expr) assert(expr)
#else
#define verify(expr) ((void)(expr))
#endif
