#pragma once

#define LIBBLANK_NS lbk

namespace LIBBLANK_NS
{

#if defined(_WIN32)
#define   LIBBLANK_WINDOWS
#else
#define   LIBBLANK_POSIX
#endif

}; // end namespace

#include "libblank/sys.h"
#include "libblank/blank.h"
