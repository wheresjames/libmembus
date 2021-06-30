#pragma once

#define LIBMEMBUS_NS mmb

namespace LIBMEMBUS_NS
{

#if defined(_WIN32)
#define   LIBMEMBUS_WINDOWS
#else
#define   LIBMEMBUS_POSIX
#endif

}; // end namespace

#include "libmembus/sys.h"
#include "libmembus/blank.h"
