
#include "libmembus-internal.h"

#include <iostream>
#if defined(LIBMEMBUS_POSIX)
#   include <unistd.h>
#   include <signal.h>
#elif defined(LIBMEMBUS_WINDOWS)
#   include <windows.h>
#endif


namespace LIBMEMBUS_NS
{

static thread_local errc g_last_error = errc::ok;

void set_last_error(errc e)
{
    g_last_error = e;
}

errc last_error()
{
    return g_last_error;
}

const char *last_error_message(errc e)
{
    switch (e)
    {
    case errc::ok:                return "ok";
    case errc::invalid_argument:  return "invalid argument";
    case errc::open_failed:       return "open failed";
    case errc::create_failed:     return "create failed";
    case errc::map_failed:        return "map failed";
    case errc::size_mismatch:     return "size mismatch";
    case errc::invalid_layout:    return "invalid shared-memory layout";
    case errc::not_open:          return "not open";
    case errc::access_denied:     return "access denied";
    case errc::message_too_large: return "message too large";
    case errc::lock_timeout:      return "lock timeout";
    case errc::timeout:           return "timeout";
    case errc::overrun:           return "overrun";
    }
    return "unknown error";
}

const char *last_error_message()
{
    return last_error_message(last_error());
}

static volatile int *g_fCount = 0;
static void ctrl_c_handler(int /*s*/)
{
    static const char msg[] = "~ ctrl-c ~\n";
#if defined(LIBMEMBUS_POSIX)
    // Only async-signal-safe calls allowed here
    if (!g_fCount || 3 < (*g_fCount))
    {
        write(STDOUT_FILENO, msg, sizeof(msg) - 1);
        _exit(1);
    }
    __atomic_fetch_add((int*)g_fCount, 1, __ATOMIC_SEQ_CST);
    write(STDOUT_FILENO, msg, sizeof(msg) - 1);
#else
    // Windows CtrlHandler runs in a dedicated thread, not a true async signal
    if (!g_fCount || 3 < (*g_fCount))
    {   std::cout << msg;
        exit(1);
    }
    *g_fCount = *g_fCount + 1;
    std::cout << msg;
#endif
}


//-------------------------------------------------------------------
#if defined(LIBMEMBUS_POSIX)

void install_ctrl_c_handler(volatile int *fCount)
{
    // Save the flag location
    g_fCount = fCount;

    // Install the signal handler
    struct sigaction sigIntHandler;
    sigIntHandler.sa_handler = ctrl_c_handler;
    sigemptyset(&sigIntHandler.sa_mask);
    sigIntHandler.sa_flags = 0;
    sigaction(SIGINT, &sigIntHandler, 0);
}


//-------------------------------------------------------------------
#elif defined(LIBMEMBUS_WINDOWS)

BOOL WINAPI CtrlHandler(DWORD fdwCtrlType)
{
    if (CTRL_C_EVENT != fdwCtrlType)
        return FALSE;

    ctrl_c_handler(0);

    return TRUE;
}

void install_ctrl_c_handler(volatile int *fCount)
{
    // Save the flag location
    g_fCount = fCount;

    // Set ctrl handler
    SetConsoleCtrlHandler(CtrlHandler, TRUE);
}

#endif

} // end namespace
