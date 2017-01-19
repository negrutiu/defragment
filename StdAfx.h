#pragma once

// Target platform
#ifndef _WIN32_WINNT
	#define _WIN32_WINNT 0x0500	///_WIN32_WINNT_WIN2K
#endif
#include <SDKDDKVer.h>


#include <stdio.h>
#include <tchar.h>
#include <stdlib.h>

#include <Windows.h>

#ifdef __MINGW32__
#undef __CRT__NO_INLINE
#endif
#include <strsafe.h>
#ifdef __MINGW32__
#define __CRT__NO_INLINE
#endif

#include <assert.h>
#if defined (_DEBUG) || defined (DBG)
	#define verify(expr) assert(expr)
#else
	#define verify(expr) ((void)(expr))
#endif
