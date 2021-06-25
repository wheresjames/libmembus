
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

#ifdef WIN32
#   define PASSSTR "[PASS] "
#   define ERRSTR  "[ERROR] "
#   define DBGSTR  "[DEBUG] "
#else
#   define PASSSTR "[\033[1;32mPASS\033[1;0m] "
#   define ERRSTR  "[\033[1;31mERROR\033[1;0m] "
#   define DBGSTR  "[\033[92mDEBUG\033[0m] "
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
int Test_MessageQueue()
{
    mmb::memmsg tx, rx;
    std::string msg = "Message";

    tx.open("/mymsg", 1024, true, true);
    rx.open("/mymsg", 1024, false, false);

    tx.write(msg);
    std::string rmsg = rx.read(0);
    assertTrue(rmsg == msg)


    //---------------------------------------------------------------
    // Test message queue
    for(int b = 0; b < 128; b++)
    {
        mmb::memmsg tx, rx;

        assertTrueQ(tx.open("/mymsg", 64 + b, true, true));
        assertFalseQ(tx.existing());
        assertTrueQ(rx.open("/mymsg", 64 + b, false, false));
        assertTrueQ(rx.existing());

        for(int i = 0; i < 1000; i++)
        {
            std::string msg; msg += "Message "; msg += std::to_string(i);
            // debugLog(msg);
            assertTrueQ(tx.write(msg));
            std::string rmsg = rx.read(0);
            assertTrueQ(rmsg == msg)
        }
    }

    return 0;
}


int main(int /*argc*/, char */*argv*/[])
{
    int result = 0;

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

    // Test message queue
    assertZero(Test_MessageQueue());

    std::cout << " --- Success ---\n";

    return 0;
}

