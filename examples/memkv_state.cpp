#include "libmembus.h"

#include <iostream>

int main()
{
    mmb::memkv owner;
    if (!owner.create("/libmembus_example_kv", 2, 16, 32, true))
        return 1;

    owner.setName(0, "state");
    owner.setName(1, "fps");
    owner.setAll({{"state", "running"}, {"fps", "30"}});

    mmb::memkv reader;
    if (!reader.open("/libmembus_example_kv"))
        return 2;

    std::cout << reader.getValue("state") << " @ " << reader.getValue("fps") << "fps\n";
    return 0;
}
