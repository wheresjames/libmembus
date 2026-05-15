
#include <catch2/catch_test_macros.hpp>
#include "libmembus.h"

#include <set>
#include <thread>
#include <vector>


TEST_CASE("Convenience APIs", "[helpers]")
{
    SECTION("Session IDs are exposed for restart detection")
    {
        mmb::memmsg tx;
        REQUIRE(tx.open("/helpers_session_msg", 256, true, true));
        REQUIRE(tx.getSessionId() != 0);

        mmb::memcmd cmd;
        REQUIRE(cmd.open("/helpers_session_cmd", 256, true, true));
        REQUIRE(cmd.getSessionId() != 0);

        mmb::memkv kv;
        REQUIRE(kv.create("/helpers_session_kv", 1, 8, 8, true));
        REQUIRE(kv.getSessionId() != 0);
    }

    SECTION("memvid waitForFrame observes published frames")
    {
        mmb::memvid vid;
        REQUIRE(vid.open("/helpers_vid_wait", true, 16, 8, mmb::video_format::rgb24, 30, 2));
        int64_t seq = vid.getSeq();
        REQUIRE_FALSE(vid.waitForFrame(0, seq));
        vid.next(1);
        REQUIRE(vid.waitForFrame(100, seq));
    }

    SECTION("Helper wrappers cover common message and command patterns")
    {
        mmb::memmsg_writer tx;
        mmb::memmsg_reader rx;
        REQUIRE(tx.open("/helpers_msg", 256));
        REQUIRE(rx.open("/helpers_msg", 256));
        REQUIRE(tx.write("hello"));
        REQUIRE(rx.read(100) == "hello");

        mmb::memcmd_receiver receiver;
        mmb::memcmd_sender sender;
        REQUIRE(receiver.open("/helpers_cmd", 256));
        REQUIRE(sender.open("/helpers_cmd", 256));
        REQUIRE(sender.write("go"));
        REQUIRE(receiver.read(100) == "go");
    }

    SECTION("memaud_writer open (no bNew parameter)")
    {
        // Verify the wrapper compiles and functions without the removed bNew param.
        // Open the reader before the first publish so resync() captures seq=0,
        // then wait() correctly detects the subsequent next() call.
        mmb::memaud_writer aw;
        REQUIRE(aw.open("/helpers_memaud_writer", 1, mmb::audio_format::u8, 8000, 100, 4));

        mmb::memaud_reader ar;
        REQUIRE(ar.open("/helpers_memaud_writer"));

        REQUIRE(aw.fill(0, 0xAA));
        aw.next(1);

        REQUIRE(ar.wait(100));

        mmb::memaud::audview v = ar.readNext();
        REQUIRE(v.m_ptr != nullptr);
        REQUIRE(v.m_size > 0);
    }

    SECTION("memvid_reader readNext detects overrun and resumes on next frame")
    {
        // Writer publishes enough frames to lap a stalled reader, then publishes
        // one more.  readNext() must signal overrun and then deliver the new frame.
        mmb::memvid_writer vw;
        REQUIRE(vw.open("/helpers_vid_overrun", 16, 8, mmb::video_format::gray8, 30, 3));

        mmb::memvid_reader vr;
        REQUIRE(vr.open("/helpers_vid_overrun"));

        // Lap the reader: publish bufs+1 frames without the reader consuming any
        for (int i = 0; i < 4; i++)
        {
            vw.fill(vw.getPtr(), i);
            vw.next(1);
        }

        bool ov = false;
        mmb::memvid::vidview frame = vr.readNext(&ov);
        REQUIRE(ov);                    // overrun detected
        (void)frame;                    // returned view must not be used on overrun

        // Publish one more frame; reader should receive it cleanly after resync
        vw.fill(vw.getPtr(), 0xFF);
        vw.next(1);

        REQUIRE(vr.wait(100));
        ov = false;
        frame = vr.readNext(&ov);
        REQUIRE_FALSE(ov);              // clean delivery after resync
        REQUIRE(frame.m_ptr != nullptr);
    }

    SECTION("memaud_reader readNext detects overrun and resumes on next buffer")
    {
        mmb::memaud_writer aw;
        REQUIRE(aw.open("/helpers_aud_overrun", 1, mmb::audio_format::u8, 8000, 100, 3));

        mmb::memaud_reader ar;
        REQUIRE(ar.open("/helpers_aud_overrun"));

        for (int i = 0; i < 4; i++)
        {
            aw.fill(aw.getPtr(), i);
            aw.next(1);
        }

        bool ov = false;
        mmb::memaud::audview buf = ar.readNext(&ov);
        REQUIRE(ov);
        (void)buf;

        aw.fill(aw.getPtr(), 0xFF);
        aw.next(1);

        REQUIRE(ar.wait(100));
        ov = false;
        buf = ar.readNext(&ov);
        REQUIRE_FALSE(ov);
        REQUIRE(buf.m_ptr != nullptr);
    }
}


TEST_CASE("Stress", "[stress]")
{
    SECTION("memcmd handles multiple writer handles")
    {
        constexpr int writers = 4;
        constexpr int messages = 50;

        mmb::memcmd receiver;
        REQUIRE(receiver.open("/stress_memcmd", 8192, true, true));

        std::vector<std::thread> threads;
        for (int w = 0; w < writers; w++)
        {
            threads.emplace_back([w]() {
                mmb::memcmd sender;
                if (!sender.open("/stress_memcmd", 8192))
                    return;
                for (int i = 0; i < messages; i++)
                    sender.write("w" + std::to_string(w) + "_" + std::to_string(i));
            });
        }

        for (auto &t : threads)
            t.join();

        std::set<std::string> seen;
        for (int i = 0; i < writers * messages; i++)
        {
            std::string msg = receiver.read(100);
            REQUIRE_FALSE(msg.empty());
            seen.insert(msg);
        }

        REQUIRE(seen.size() == writers * messages);
    }

    SECTION("multiple memmsg readers each receive the stream")
    {
        mmb::memmsg tx, r1, r2;
        REQUIRE(tx.open("/stress_memmsg_readers", 4096, true, true));
        REQUIRE(r1.open("/stress_memmsg_readers", 4096, false, false));
        REQUIRE(r2.open("/stress_memmsg_readers", 4096, false, false));

        for (int i = 0; i < 100; i++)
            REQUIRE(tx.write("m" + std::to_string(i)));

        for (int i = 0; i < 100; i++)
        {
            std::string expected = "m" + std::to_string(i);
            REQUIRE(r1.read(100) == expected);
            REQUIRE(r2.read(100) == expected);
        }
    }
}
