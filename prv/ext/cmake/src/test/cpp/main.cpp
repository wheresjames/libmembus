
#include <atomic>
#include <iostream>
#include <iomanip>
#include <cstring>

#include "libmembus.h"

//-------------------------------------------------------------------

template<typename T>
    T filename(const T &path, const T &seps = "/\\")
{   typename T::size_type pos = path.find_last_of(seps);
    if (T::npos == pos)
        return path;
    return T(path, pos + 1);
}

#ifdef _WIN32
#   define PASSSTR "[PASS] "
#   define ERRSTR  "[ERROR] "
#   define DBGSTR  "[DBUG] "
#else
#   define PASSSTR "[\e[1;32mPASS\e[1;0m] "
#   define ERRSTR  "[\e[1;31mERROR\e[1;0m] "
#   define DBGSTR  "[\033[92mDBUG\033[0m] "
#endif

// Log debug message
#define debugLog(s) std::cout << DBGSTR << filename<std::string>(__FILE__) << "(" << __LINE__ << "): " << s << std::endl;

// Assert macros
#define assertTrue(s) { if (!(s)) { std::cout << (ERRSTR #s) << std::endl; return -1; } else std::cout << (PASSSTR #s) << std::endl; }
#define assertFalse(s) { if ((s)) { std::cout << (ERRSTR #s) << std::endl; return -1; } else std::cout << (PASSSTR #s) << std::endl; }
#define assertZero(s) { if ((s)) { std::cout << (ERRSTR #s) << std::endl; return -1; } else std::cout << (PASSSTR #s) << std::endl; }
#define assertNotZero(s) { if (!(s)) { std::cout << (ERRSTR #s) << std::endl; return -1; } else std::cout << (PASSSTR #s) << std::endl; }

// Quiet
#define assertTrueQ(s) { if (!(s)) { std::cout << (ERRSTR #s) << std::endl; return -1; } }
#define assertFalseQ(s) { if ((s)) { std::cout << (ERRSTR #s) << std::endl; return -1; } }


//-------------------------------------------------------------------
int Test_Any()
{

    return 0;
}


int main(int /*argc*/, char * /*argv*/ [])
{
    // Install ctrl-c handler
    static volatile int ctrl_c_count = 0;
    mmb::install_ctrl_c_handler(&ctrl_c_count);

    std::cout << " --- TEST FOR " APPNAME "\n";
    std::cout << " --- VERSION " APPVER " [" APPBUILD "]\n";
    std::cout << " --- Starting Tests ---\n";

    // Check macros
    assertTrue(true);
    assertFalse(false);
    assertTrueQ(true);
    assertFalseQ(false);
    assertZero(0);
    assertNotZero(1);
    debugLog("Debug message");

    assertZero(Test_Any());

    std::cout << " --- Success ---\n";

    return 0;
}

