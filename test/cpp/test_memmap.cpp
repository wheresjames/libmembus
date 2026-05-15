
#include <catch2/catch_test_macros.hpp>
#include "libmembus.h"


TEST_CASE("MemMap", "[memmap]")
{
    SECTION("Basic create and attach")
    {
        mmb::memmap creator, attacher;

        REQUIRE(creator.open("/test_memmap", 256, true, true));
        REQUIRE_FALSE(creator.existing());
        REQUIRE(creator.isOpen());
        REQUIRE(creator.size() == 256);
        REQUIRE(creator.name() == "/test_memmap");
        REQUIRE(creator.data() != nullptr);

        REQUIRE(attacher.open("/test_memmap", 256, false, false));
        REQUIRE(attacher.existing());
        REQUIRE(attacher.isOpen());
    }

    SECTION("Write and read back via two handles")
    {
        mmb::memmap writer, reader;
        REQUIRE(writer.open("/test_memmap_rw", 128, true, true));
        REQUIRE(reader.open("/test_memmap_rw", 128, false, false));

        std::string msg = "Hello shared memory!";
        REQUIRE(writer.write(msg) == (int64_t)msg.size());

        std::string back = reader.read(msg.size());
        REQUIRE(back == msg);
    }

    SECTION("Close clears state")
    {
        mmb::memmap m;
        REQUIRE(m.open("/test_memmap_close", 64, true, true));
        REQUIRE(m.isOpen());
        m.close();
        REQUIRE_FALSE(m.isOpen());
        REQUIRE(m.size() == 0);
        REQUIRE(m.name().empty());
    }

    SECTION("Attach while creator is alive; bNew forces recreation")
    {
        mmb::memmap creator;
        REQUIRE(creator.open("/test_memmap_new", 64, true, true));
        REQUIRE_FALSE(creator.existing());

        {
            mmb::memmap second;
            REQUIRE(second.open("/test_memmap_new", 64, false, false));
            REQUIRE(second.existing());
        }

        mmb::memmap fresh;
        REQUIRE(fresh.open("/test_memmap_new", 64, true, true));
        REQUIRE_FALSE(fresh.existing());
    }

    SECTION("Read with no size limit")
    {
        mmb::memmap w, r;
        REQUIRE(w.open("/test_memmap_full", 16, true, true));
        REQUIRE(r.open("/test_memmap_full", 16, false, false));
        std::string payload(16, 'X');
        REQUIRE(w.write(payload) == 16);
        std::string back = r.read();
        REQUIRE(back.size() == 16);
    }

    SECTION("read() on unopened map returns empty string")
    {
        mmb::memmap m;
        REQUIRE_FALSE(m.isOpen());
        REQUIRE(m.size() == 0);
        REQUIRE(m.read().empty());
        REQUIRE(m.read(10).empty());
    }

    SECTION("Opening an existing map with bCreate does not truncate it")
    {
        mmb::memmap creator, attacher;
        REQUIRE(creator.open("/test_memmap_no_truncate", 128, true, true));

        std::string payload = "payload that must survive attach";
        REQUIRE(creator.write(payload) == (int64_t)payload.size());

        REQUIRE(attacher.open("/test_memmap_no_truncate", 16, true, false));
        REQUIRE(attacher.existing());
        REQUIRE(attacher.size() == 128);
        REQUIRE(attacher.read(payload.size()) == payload);
    }

    SECTION("Attaching with nSize=0 reports the creator's actual size")
    {
        mmb::memmap creator, attacher;
        REQUIRE(creator.open("/test_memmap_actualsize", 256, true, true));
        REQUIRE(creator.size() == 256);

        REQUIRE(attacher.open("/test_memmap_actualsize", 0, false, false));
        REQUIRE(attacher.size() == 256);
    }

    SECTION("Read-only open attaches to an existing share")
    {
        mmb::memmap creator, reader;
        REQUIRE(creator.open("/test_memmap_ro", 64, true, true));
        std::string msg = "readonly test";
        REQUIRE(creator.write(msg) == (int64_t)msg.size());

        REQUIRE(reader.open("/test_memmap_ro", 0, false, false, /*bReadOnly=*/true));
        REQUIRE(reader.existing());
        REQUIRE(reader.isOpen());
        REQUIRE(reader.size() == 64);
        REQUIRE(reader.read(msg.size()) == msg);
    }

    SECTION("Read-only open cannot create a share that does not exist")
    {
        mmb::memmap::remove("/test_memmap_ro_nocreate");
        mmb::memmap m;
        REQUIRE_FALSE(m.open("/test_memmap_ro_nocreate", 64, /*bCreate=*/true, false, /*bReadOnly=*/true));
        REQUIRE_FALSE(m.isOpen());
    }
}
