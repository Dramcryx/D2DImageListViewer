#ifndef D2DILV_DEFINES_H
#define D2DILV_DEFINES_H

#ifndef UNICODE
#define UNICODE
#endif

#ifndef _UNICODE
#define _UNICODE
#endif

#ifndef WINE_UNICODE_NATIVE
#define WINE_UNICODE_NATIVE
#endif

#include <cassert>

#ifndef OK
#define OK(hr) assert(hr == S_OK)
#endif

#ifndef NOTNULL
#define NOTNULL(ptr) assert(ptr != nullptr)
#endif

#ifndef TRACE
#ifdef DEBUG
#include <iostream>
#define TRACE() std::cout << __PRETTY_FUNCTION__ << "\n";
#else
#define TRACE()
#endif
#endif

#endif
