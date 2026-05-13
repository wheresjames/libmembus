#include "libmembus.h"

#include <iostream>

int main()
{
    mmb::memcmd_receiver receiver;
    mmb::memcmd_sender sender;

    if (!receiver.open("/libmembus_example_cmd", 1024, true)
        || !sender.open("/libmembus_example_cmd", 1024))
    {
        std::cerr << mmb::last_error_message() << "\n";
        return 1;
    }

    if (!sender.write("pan_left"))
        return 2;

    bool overrun = false;
    std::string cmd = receiver.read(100, &overrun);
    if (overrun || cmd.empty())
        return 3;

    std::cout << cmd << "\n";
    return 0;
}
