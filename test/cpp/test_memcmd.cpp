
#include <catch2/catch_test_macros.hpp>
#include "libmembus.h"


TEST_CASE("MemCmd", "[memcmd]")
{
    SECTION("Basic create, write, read")
    {
        mmb::memcmd receiver, sender;

        // Capture process creates the channel and registers as a reader
        REQUIRE(receiver.open("/memcmd_basic", 1024, /*bReader=*/true, /*bCreate=*/true));
        REQUIRE_FALSE(receiver.existing());
        REQUIRE(receiver.readerCount() == 1);

        // Consumer process attaches and sends a command
        REQUIRE(sender.open("/memcmd_basic", 1024));  // defaults: bReader=false, bCreate=false
        REQUIRE(sender.existing());
        REQUIRE(sender.readerCount() == 1);  // count unchanged by non-reader attach

        REQUIRE(sender.write("pan_left"));
        REQUIRE(receiver.read(0) == "pan_left");
    }

    SECTION("Multiple writers: ordering is preserved")
    {
        // Two independent sender handles writing to the same channel;
        // the mutex ensures their writes do not interleave or corrupt each other.
        mmb::memcmd receiver, w1, w2;
        REQUIRE(receiver.open("/memcmd_mw", 1024, true, true));
        REQUIRE(w1.open("/memcmd_mw", 1024));
        REQUIRE(w2.open("/memcmd_mw", 1024));

        REQUIRE(w1.write("pan_left"));
        REQUIRE(w2.write("pan_right"));
        REQUIRE(w1.write("pan_stop"));

        REQUIRE(receiver.read(0) == "pan_left");
        REQUIRE(receiver.read(0) == "pan_right");
        REQUIRE(receiver.read(0) == "pan_stop");
    }

    SECTION("Multiple independent readers each receive all messages")
    {
        // Broadcast semantics: r1 and r2 maintain separate read positions
        // and both receive the full message stream.
        mmb::memcmd sender, r1, r2;
        REQUIRE(sender.open("/memcmd_mr", 1024, false, true));
        REQUIRE(r1.open("/memcmd_mr", 1024, true,  false));
        REQUIRE(r2.open("/memcmd_mr", 1024, true,  false));
        REQUIRE(sender.readerCount() == 2);

        REQUIRE(sender.write("tilt_up"));
        REQUIRE(sender.write("zoom_in"));

        REQUIRE(r1.read(0) == "tilt_up");
        REQUIRE(r1.read(0) == "zoom_in");

        REQUIRE(r2.read(0) == "tilt_up");
        REQUIRE(r2.read(0) == "zoom_in");
    }

    SECTION("Any open handle may write, including reader handles")
    {
        // Unlike memmsg, there is no writer-only restriction.
        // Here the receiver (bReader=true) also writes a response,
        // demonstrating that write() is available on any handle.
        mmb::memcmd a, b;
        REQUIRE(a.open("/memcmd_any", 512, true,  true));   // reader + creator
        REQUIRE(b.open("/memcmd_any", 512, false, false));  // non-reader attacher

        REQUIRE(a.write("from_reader_handle"));
        REQUIRE(b.write("from_sender_handle"));

        REQUIRE(a.read(0) == "from_reader_handle");
        REQUIRE(a.read(0) == "from_sender_handle");
        REQUIRE(b.read(0) == "from_reader_handle");
        REQUIRE(b.read(0) == "from_sender_handle");
    }

    SECTION("Reader count tracks registered handles")
    {
        mmb::memcmd creator;
        REQUIRE(creator.open("/memcmd_rc", 512, true, true));
        REQUIRE(creator.readerCount() == 1);

        {
            mmb::memcmd r2;
            REQUIRE(r2.open("/memcmd_rc", 512, true, false));
            REQUIRE(creator.readerCount() == 2);
        }
        // r2 went out of scope — count decremented in destructor
        REQUIRE(creator.readerCount() == 1);

        // A non-reader attach does not affect the count
        mmb::memcmd sender;
        REQUIRE(sender.open("/memcmd_rc", 512, false, false));
        REQUIRE(creator.readerCount() == 1);
    }

    SECTION("Overrun detection and resync")
    {
        // Small buffer so the writer can lap the reader quickly.
        mmb::memcmd sender, receiver;
        REQUIRE(sender.open("/memcmd_overrun", 128, false, true));
        REQUIRE(receiver.open("/memcmd_overrun", 128, true,  false));

        // Seed one message so receiver establishes m_nLastSeq
        REQUIRE(sender.write("seed"));
        bool ov = false;
        REQUIRE(receiver.read(0, &ov) == "seed");
        REQUIRE_FALSE(ov);

        // Flood the buffer to lap the receiver
        for (int i = 0; i < 64; i++)
            sender.write("x");

        // Receiver detects overrun, returns empty, resyncs
        std::string msg = receiver.read(0, &ov);
        REQUIRE(msg.empty());
        REQUIRE(ov);

        // After resync the next message is received normally
        REQUIRE(sender.write("after_resync"));
        msg = receiver.read(100, &ov);
        REQUIRE(msg == "after_resync");
        REQUIRE_FALSE(ov);
    }

    SECTION("Non-blocking read on empty channel returns empty")
    {
        mmb::memcmd a;
        REQUIRE(a.open("/memcmd_empty_read", 256, true, true));
        bool ov = true;
        std::string msg = a.read(0, &ov);
        REQUIRE(msg.empty());
        REQUIRE_FALSE(ov);  // empty queue is not an overrun
    }

    SECTION("Empty write must fail")
    {
        mmb::memcmd a;
        REQUIRE(a.open("/memcmd_empty_write", 256, false, true));
        REQUIRE_FALSE(a.write(""));
    }

    SECTION("Size mismatch on attach must fail")
    {
        mmb::memcmd creator, bad;
        REQUIRE(creator.open("/memcmd_sizematch", 256, false, true));
        REQUIRE_FALSE(bad.open("/memcmd_sizematch", 128));
    }

    SECTION("existing() flag")
    {
        mmb::memcmd creator, attacher;
        REQUIRE(creator.open("/memcmd_existing", 256, true, true));
        REQUIRE_FALSE(creator.existing());
        REQUIRE(attacher.open("/memcmd_existing", 256));
        REQUIRE(attacher.existing());
    }

    SECTION("Round-trip across buffer sizes")
    {
        // Verify correct behaviour as the ring buffer wraps at various sizes
        for (int b = 0; b < 64; b++)
        {
            mmb::memcmd sender, receiver;
            REQUIRE(sender.open("/memcmd_rt", 64 + b, false, true));
            REQUIRE(receiver.open("/memcmd_rt", 64 + b, true, false));

            for (int i = 0; i < 200; i++)
            {
                std::string msg = "cmd_" + std::to_string(i);
                REQUIRE(sender.write(msg));
                REQUIRE(receiver.read(0) == msg);
            }
        }
    }

    SECTION("Mixed-length commands round-trip through alignment boundaries")
    {
        // Same alignment exercise as memmsg: frame stride is rounded to 8 bytes,
        // so lengths 1..30 hit every misalignment pattern before and after wrap.
        mmb::memcmd sender, receiver;
        REQUIRE(sender.open("/memcmd_align", 256, false, true));
        REQUIRE(receiver.open("/memcmd_align", 256, true,  false));

        for (int cycle = 0; cycle < 10; cycle++)
        {
            for (int len = 1; len <= 30; len++)
            {
                std::string msg(len, 'A' + (len % 26));
                REQUIRE(sender.write(msg));
                REQUIRE(receiver.read(100) == msg);
            }
        }
    }

    SECTION("Attach rejects shares too small for the requested buffer")
    {
        mmb::memmap raw;
        REQUIRE(raw.open("/memcmd_small_backing", 16, true, true));
        ((int64_t*)raw.data())[0] = 256;

        mmb::memcmd cmd;
        REQUIRE_FALSE(cmd.open("/memcmd_small_backing", 256));
    }
}
