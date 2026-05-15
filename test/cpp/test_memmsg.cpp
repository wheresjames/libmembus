
#include <catch2/catch_test_macros.hpp>
#include "libmembus.h"


TEST_CASE("MessageQueue", "[memmsg]")
{
    SECTION("Basic send and receive")
    {
        mmb::memmsg tx, rx;
        std::string msg = "Message";

        tx.open("/mymsg", 1024, true, true);
        rx.open("/mymsg", 1024, false, false);

        tx.write(msg);
        std::string rmsg = rx.read(0);
        REQUIRE(rmsg == msg);
    }

    SECTION("Round-trip across buffer sizes")
    {
        for (int b = 0; b < 128; b++)
        {
            mmb::memmsg tx, rx;

            REQUIRE(tx.open("/mymsg", 64 + b, true, true));
            REQUIRE_FALSE(tx.existing());
            REQUIRE(rx.open("/mymsg", 64 + b, false, false));
            REQUIRE(rx.existing());

            for (int i = 0; i < 1000; i++)
            {
                std::string msg = "Message " + std::to_string(i);
                REQUIRE(tx.write(msg));
                std::string rmsg = rx.read(0);
                REQUIRE(rmsg == msg);
            }
        }
    }

    SECTION("Mixed-length messages round-trip through alignment boundaries")
    {
        // Each frame advances by (fv_last + len) rounded up to 8 bytes.
        // Interleave write/read so every length from 1..30 forces a different
        // alignment for the *next* frame header, exercising all eight offsets mod 8.
        mmb::memmsg tx, rx;
        REQUIRE(tx.open("/mymsg_align", 256, true, true));
        REQUIRE(rx.open("/mymsg_align", 256, false, false));

        for (int cycle = 0; cycle < 10; cycle++)
        {
            for (int len = 1; len <= 30; len++)
            {
                std::string msg(len, 'a' + (len % 26));
                REQUIRE(tx.write(msg));
                REQUIRE(rx.read(100) == msg);
            }
        }
    }
}


TEST_CASE("MessageQueue edge cases", "[memmsg]")
{
    SECTION("Read with timeout on empty queue returns empty")
    {
        mmb::memmsg tx, rx;
        REQUIRE(tx.open("/mymsg_ec", 256, true, true));
        REQUIRE(rx.open("/mymsg_ec", 256, false, false));

        std::string empty = rx.read(0);
        REQUIRE(empty.empty());
    }

    SECTION("Size mismatch on attach must fail")
    {
        mmb::memmsg tx, rx;
        REQUIRE(tx.open("/mymsg_sizematch", 128, true, true));
        REQUIRE_FALSE(rx.open("/mymsg_sizematch", 64, false, false));
    }

    SECTION("Write on read-only handle must fail")
    {
        mmb::memmsg tx, rx;
        REQUIRE(tx.open("/mymsg_readonly", 256, true, true));
        REQUIRE(rx.open("/mymsg_readonly", 256, false, false));
        REQUIRE_FALSE(rx.write("should fail"));
    }

    SECTION("Empty string write must fail")
    {
        mmb::memmsg tx, rx;
        REQUIRE(tx.open("/mymsg_empty_write", 256, true, true));
        REQUIRE(rx.open("/mymsg_empty_write", 256, false, false));
        REQUIRE_FALSE(tx.write(""));
        REQUIRE(rx.read(0).empty());
    }

    SECTION("existing() flag")
    {
        mmb::memmsg tx, rx;
        REQUIRE(tx.open("/mymsg_existing", 256, true, true));
        REQUIRE_FALSE(tx.existing());
        REQUIRE(rx.open("/mymsg_existing", 256, false, false));
        REQUIRE(rx.existing());
    }

    SECTION("Attach rejects shares too small for the requested buffer")
    {
        mmb::memmap raw;
        REQUIRE(raw.open("/mymsg_small_backing", 16, true, true));
        ((int64_t*)raw.data())[0] = 256;

        mmb::memmsg rx;
        REQUIRE_FALSE(rx.open("/mymsg_small_backing", 256, false, false));
    }

    SECTION("Multiple messages round-trip")
    {
        mmb::memmsg tx, rx;
        REQUIRE(tx.open("/mymsg_multi", 512, true, true));
        REQUIRE(rx.open("/mymsg_multi", 512, false, false));

        const char *msgs[] = {"alpha", "beta", "gamma", "delta", "epsilon"};
        for (auto &m : msgs)
        {
            REQUIRE(tx.write(m));
            std::string back = rx.read(0);
            REQUIRE(back == m);
        }
    }

    SECTION("Overrun detection and resync")
    {
        // Small buffer so we can force the writer to lap the reader quickly.
        // Each frame is fv_last (16) + payload bytes; "x" frames are 17 bytes each.
        // A 128-byte buffer holds ~7 frames before wrapping.
        mmb::memmsg tx, rx;
        REQUIRE(tx.open("/mymsg_overrun", 128, true, true));
        REQUIRE(rx.open("/mymsg_overrun", 128, false, false));

        // Read one message first to initialise m_nLastSeq
        REQUIRE(tx.write("seed"));
        bool ov = false;
        std::string s = rx.read(0, &ov);
        REQUIRE(s == "seed");
        REQUIRE_FALSE(ov);

        // Flood the buffer: write enough messages to guarantee the reader's
        // position has been overwritten at least once
        for (int i = 0; i < 64; i++)
            tx.write("x");

        // Reader should now detect overrun and signal it
        s = rx.read(0, &ov);
        REQUIRE(s.empty());
        REQUIRE(ov);

        // After resync the reader should receive messages normally again
        REQUIRE(tx.write("after-resync"));
        s = rx.read(100, &ov);
        REQUIRE(s == "after-resync");
        REQUIRE_FALSE(ov);
    }
}
