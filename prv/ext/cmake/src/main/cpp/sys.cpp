
#include <iostream>

#include "libblank.h"

#if defined(LIBBLANK_POSIX)
#   include <unistd.h>
#   include <signal.h>
#elif defined(LIBBLANK_WINDOWS)
#   include <windows.h>
#endif


namespace LIBBLANK_NS
{

static volatile int *g_fCount = 0;
static void ctrl_c_handler(int /*s*/)
{
    if (!g_fCount || 3 < (*g_fCount))
    {   std::cout << "~ ctrl-c ~\n";
        exit(1);
    }

    (*g_fCount)++;
    std::cout << "~ ctrl-c ~\n";
}


//-------------------------------------------------------------------
#if defined(LIBBLANK_POSIX)

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
#elif defined(LIBBLANK_WINDOWS)

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
