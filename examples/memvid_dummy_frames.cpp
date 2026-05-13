#include "libmembus.h"

#include <iostream>

int main()
{
    mmb::memvid_writer writer;
    if (!writer.open("/libmembus_example_vid", 64, 48, 30, 3))
        return 1;

    mmb::memvid_reader reader;
    if (!reader.open("/libmembus_example_vid"))
        return 2;

    int64_t slot = writer.getPtr(0);
    writer.fill(slot, 0x7f);
    writer.next();

    if (!reader.wait(100))
        return 3;

    bool overrun = false;
    auto frame = reader.readNext(&overrun);
    if (overrun || !frame.m_ptr)
        return 4;

    std::cout << frame.m_w << "x" << frame.m_h << " frame\n";
    return 0;
}
