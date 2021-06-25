#pragma once

// #define _GLIBCXX_USE_CXX11_ABI 0

#define LIBMEMBUS_NS mmb

namespace LIBMEMBUS_NS
{

#if defined(_WIN32)
#define   LIBMEMBUS_WINDOWS
#else
#define   LIBMEMBUS_POSIX
#endif

}; // end namespace


#include <memory>
#include <exception>
#include <cstring>
#include <cstdlib>
#include <string>
#include <iostream>
// #include <mutex>
// #include <condition_variable>

#include "libmembus/sys.h"
#include "libmembus/memmap.h"
#include "libmembus/memmsg.h"
#include "libmembus/memvid.h"
#include "libmembus/memaud.h"


