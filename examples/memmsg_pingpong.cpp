#include "libmembus.h"

#include <iostream>

int main()
{
    mmb::memmsg_writer tx;
    mmb::memmsg_reader rx;

    if (!tx.open("/libmembus_example_msg", 1024)
        || !rx.open("/libmembus_example_msg", 1024))
    {
        std::cerr << mmb::last_error_message() << "\n";
        return 1;
    }

    if (!tx.write("ping"))
        return 2;

    bool overrun = false;
    std::string msg = rx.read(100, &overrun);
    if (overrun || msg != "ping")
        return 3;

    std::cout << msg << "\n";
    return 0;
}
