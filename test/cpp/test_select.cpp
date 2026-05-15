
#include <catch2/catch_test_macros.hpp>
#include "libmembus.h"

#include <thread>


TEST_CASE("memmsg poll", "[memmsg][poll]")
{
    SECTION("poll() returns false on empty queue")
    {
        mmb::memmsg tx, rx;
        REQUIRE(tx.open("/poll_msg_empty", 256, true, true));
        REQUIRE(rx.open("/poll_msg_empty", 256, false, false));
        REQUIRE_FALSE(rx.poll());
    }

    SECTION("poll() returns true after a write")
    {
        mmb::memmsg tx, rx;
        REQUIRE(tx.open("/poll_msg_data", 256, true, true));
        REQUIRE(rx.open("/poll_msg_data", 256, false, false));
        REQUIRE(tx.write("hello"));
        REQUIRE(rx.poll());
    }

    SECTION("poll() returns false after the message is consumed")
    {
        mmb::memmsg tx, rx;
        REQUIRE(tx.open("/poll_msg_consume", 256, true, true));
        REQUIRE(rx.open("/poll_msg_consume", 256, false, false));
        REQUIRE(tx.write("hi"));
        REQUIRE(rx.poll());
        rx.read(0);
        REQUIRE_FALSE(rx.poll());
    }
}


TEST_CASE("memcmd poll", "[memcmd][poll]")
{
    SECTION("poll() returns false on empty channel")
    {
        mmb::memcmd a;
        REQUIRE(a.open("/poll_cmd_empty", 256, true, true));
        REQUIRE_FALSE(a.poll());
    }

    SECTION("poll() returns true after a write")
    {
        mmb::memcmd sender, receiver;
        REQUIRE(sender.open("/poll_cmd_data", 256, false, true));
        REQUIRE(receiver.open("/poll_cmd_data", 256, true, false));
        REQUIRE(sender.write("go"));
        REQUIRE(receiver.poll());
    }

    SECTION("poll() returns false after the command is consumed")
    {
        mmb::memcmd sender, receiver;
        REQUIRE(sender.open("/poll_cmd_consume", 256, false, true));
        REQUIRE(receiver.open("/poll_cmd_consume", 256, true, false));
        REQUIRE(sender.write("stop"));
        REQUIRE(receiver.poll());
        receiver.read(0);
        REQUIRE_FALSE(receiver.poll());
    }
}


TEST_CASE("select", "[select]")
{
    SECTION("returns -1 when no condition is ever true (timeout)")
    {
        bool always_false = false;
        int idx = mmb::select(50, {[&]{ return always_false; }});
        REQUIRE(idx == -1);
    }

    SECTION("returns 0 immediately when first condition is already true")
    {
        int idx = mmb::select(0, {[&]{ return true; }, [&]{ return false; }});
        REQUIRE(idx == 0);
    }

    SECTION("returns correct index for second-ready condition")
    {
        int idx = mmb::select(0, {[&]{ return false; }, [&]{ return true; }});
        REQUIRE(idx == 1);
    }

    SECTION("returns -1 for empty condition list")
    {
        int idx = mmb::select(50, {});
        REQUIRE(idx == -1);
    }

    SECTION("detects memmsg data via poll() condition")
    {
        mmb::memmsg tx, rx;
        REQUIRE(tx.open("/select_msg", 256, true, true));
        REQUIRE(rx.open("/select_msg", 256, false, false));

        // Nothing yet — non-blocking should time out
        REQUIRE(mmb::select(0, {[&]{ return rx.poll(); }}) == -1);

        REQUIRE(tx.write("test"));
        REQUIRE(mmb::select(100, {[&]{ return rx.poll(); }}) == 0);
    }

    SECTION("detects memvid frame via getSeq condition")
    {
        mmb::memvid vid;
        REQUIRE(vid.open("/select_vid", true, 8, 4, mmb::video_format::gray8, 30, 3));
        int64_t seq = vid.getSeq();

        REQUIRE(mmb::select(0, {[&]{ return vid.getSeq() > seq; }}) == -1);

        vid.next(1);
        REQUIRE(mmb::select(100, {[&]{ return vid.getSeq() > seq; }}) == 0);
    }

    SECTION("selects the first ready source among multiple mixed sources")
    {
        // Two independent sources; only the video one is ready.
        mmb::memvid vid;
        REQUIRE(vid.open("/select_multi_vid", true, 8, 4, mmb::video_format::gray8, 30, 3));
        mmb::memmsg tx, rx;
        REQUIRE(tx.open("/select_multi_msg", 256, true, true));
        REQUIRE(rx.open("/select_multi_msg", 256, false, false));

        int64_t seq = vid.getSeq();
        vid.next(1);  // publish a frame; leave the queue empty

        int idx = mmb::select(100, {
            [&]{ return rx.poll(); },          // index 0 — not ready
            [&]{ return vid.getSeq() > seq; }  // index 1 — ready
        });
        REQUIRE(idx == 1);
    }

    SECTION("vector overload works identically to initializer-list overload")
    {
        std::vector<std::function<bool()>> conds = {
            []{ return false; },
            []{ return true;  }
        };
        REQUIRE(mmb::select(0, conds) == 1);
    }

    SECTION("select wakes when condition becomes true during wait")
    {
        mmb::memmsg tx, rx;
        REQUIRE(tx.open("/select_wakeup", 256, true, true));
        REQUIRE(rx.open("/select_wakeup", 256, false, false));

        // Write from a background thread after a short delay
        std::thread writer([&]{
            std::this_thread::sleep_for(std::chrono::milliseconds(30));
            tx.write("wakeup");
        });

        int idx = mmb::select(500, {[&]{ return rx.poll(); }});
        writer.join();

        REQUIRE(idx == 0);
        REQUIRE(rx.read(0) == "wakeup");
    }
}
