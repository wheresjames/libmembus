
#include <catch2/catch_test_macros.hpp>

#include "libmembus.h"


//-------------------------------------------------------------------
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
        REQUIRE(m.read().empty());
        REQUIRE(m.read(10).empty());
    }

    SECTION("Attaching with nSize=0 reports the creator's actual size")
    {
        mmb::memmap creator, attacher;
        REQUIRE(creator.open("/test_memmap_actualsize", 256, true, true));
        REQUIRE(creator.size() == 256);

        REQUIRE(attacher.open("/test_memmap_actualsize", 0, false, false));
        REQUIRE(attacher.size() == 256);
    }
}


//-------------------------------------------------------------------
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
}


//-------------------------------------------------------------------
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
}


//-------------------------------------------------------------------
TEST_CASE("MemVid", "[memvid]")
{
    SECTION("Invalid parameter rejection")
    {
        mmb::memvid v;
        REQUIRE_FALSE(v.open("/memvid_bad", true,  0,  0, 24, 30, 3));  // zero w/h
        REQUIRE_FALSE(v.open("/memvid_bad", true, 64, 48,  8, 30, 3));  // bpp != 24
        REQUIRE_FALSE(v.open("/memvid_bad", true, 64, 48, 24,  0, 3));  // zero fps
        REQUIRE_FALSE(v.open("/memvid_bad", true, 64, 48, 24, 30, 0));  // zero bufs
    }

    SECTION("Create and verify metadata")
    {
        mmb::memvid vid;
        REQUIRE(vid.open("/memvid_meta", true, 64, 48, 24, 30, 4));
        REQUIRE_FALSE(vid.existing());
        REQUIRE(vid.isOpen());
        REQUIRE(vid.getWidth()  == 64);
        REQUIRE(vid.getHeight() == 48);
        REQUIRE(vid.getBpp()    == 24);
        REQUIRE(vid.getFps()    == 30);
        REQUIRE(vid.getBufs()   == 4);
    }

    SECTION("Attach to existing share and verify metadata")
    {
        mmb::memvid creator, attacher;
        REQUIRE(creator.open("/memvid_attach", true, 32, 24, 24, 25, 2));
        REQUIRE(attacher.open("/memvid_attach", false, 32, 24, 24, 25, 2));
        REQUIRE(attacher.existing());
        REQUIRE(attacher.getWidth()  == 32);
        REQUIRE(attacher.getHeight() == 24);
        REQUIRE(attacher.getBufs()   == 2);
    }

    SECTION("Mismatched parameters on attach must fail")
    {
        mmb::memvid creator, bad;
        REQUIRE(creator.open("/memvid_mismatch", true, 32, 24, 24, 25, 2));
        REQUIRE_FALSE(bad.open("/memvid_mismatch", false, 64, 24, 24, 25, 2));
    }

    SECTION("fill() and getBuf() data round-trip")
    {
        mmb::memvid vid;
        REQUIRE(vid.open("/memvid_fill", true, 16, 8, 24, 30, 3));

        REQUIRE(vid.fill(0, 0xAB));
        mmb::memvid::vidview view = vid.getBuf(0);
        REQUIRE(view.m_w   == 16);
        REQUIRE(view.m_h   == 8);
        REQUIRE(view.m_ptr != nullptr);

        bool allMatch = true;
        for (int64_t i = 0; i < view.m_sw * view.m_h; i++)
            if ((unsigned char)view.m_ptr[i] != 0xAB) { allMatch = false; break; }
        REQUIRE(allMatch);

        REQUIRE(vid.fill(1, 0x00));
        mmb::memvid::vidview view1 = vid.getBuf(1);
        bool buf1Zero = true;
        for (int64_t i = 0; i < view1.m_sw * view1.m_h; i++)
            if ((unsigned char)view1.m_ptr[i] != 0x00) { buf1Zero = false; break; }
        REQUIRE(buf1Zero);

        allMatch = true;
        for (int64_t i = 0; i < view.m_sw * view.m_h; i++)
            if ((unsigned char)view.m_ptr[i] != 0xAB) { allMatch = false; break; }
        REQUIRE(allMatch);
    }

    SECTION("Pointer arithmetic: setPtr / getPtr / next")
    {
        mmb::memvid vid;
        REQUIRE(vid.open("/memvid_ptr", true, 16, 8, 24, 30, 4));

        REQUIRE(vid.setPtr(0) == 0);
        REQUIRE(vid.getPtr(0) == 0);
        REQUIRE(vid.getPtr(1) == 1);
        REQUIRE(vid.getPtr(3) == 3);

        REQUIRE(vid.setPtr(3) == 3);
        REQUIRE(vid.getPtr(1) == 0);  // wrap-around

        REQUIRE(vid.setPtr(0) == 0);
        REQUIRE(vid.next(1)   == 1);
        REQUIRE(vid.next(1)   == 2);
        REQUIRE(vid.next(2)   == 0);  // wraps at 4
    }

    SECTION("Pointer arithmetic wraps correctly for multi-step offsets")
    {
        mmb::memvid vid;
        REQUIRE(vid.open("/memvid_multiwrap", true, 16, 8, 24, 30, 4));
        REQUIRE(vid.setPtr(0) == 0);

        // getPtr: offsets beyond bufs
        REQUIRE(vid.getPtr(4)  == 0);  // full lap lands on 0
        REQUIRE(vid.getPtr(5)  == 1);  // one past a full lap
        REQUIRE(vid.getPtr(8)  == 0);  // two full laps
        REQUIRE(vid.getPtr(-5) == 3);  // negative multi-step

        // setPtr: values >= bufs or negative
        REQUIRE(vid.setPtr(5)  == 1);  // 5 % 4 == 1
        REQUIRE(vid.setPtr(-1) == 3);  // wraps to 3

        // getBuf: old code threw for idx >= 2*bufs; new code wraps
        REQUIRE_NOTHROW(vid.getBuf(8));   // 8 % 4 == 0
        REQUIRE_NOTHROW(vid.getBuf(-5));  // wraps to 3
    }

    SECTION("getPtrErr: circular distance from ptr+bias to pos")
    {
        mmb::memvid vid;
        REQUIRE(vid.open("/memvid_ptrerr", true, 16, 8, 24, 30, 8));
        REQUIRE(vid.setPtr(4) == 4);
        REQUIRE(vid.getPtrErr(4, 0) == 0);
        REQUIRE(vid.getPtrErr(5, 1) == 0);
    }

    SECTION("open_existing")
    {
        mmb::memvid creator, ex;
        REQUIRE(creator.open("/memvid_existing", true, 16, 8, 24, 30, 2));
        REQUIRE(ex.open_existing("/memvid_existing"));
        REQUIRE(ex.isOpen());
    }

    SECTION("open_existing rejects share with zeroed headers")
    {
        // A raw memmap share has all-zero bytes — header fields (bufs, blocksz,
        // etc.) are 0, which open_existing must reject.
        mmb::memmap raw;
        REQUIRE(raw.open("/memvid_zerohdr", 256, true, true));

        mmb::memvid vid;
        REQUIRE_FALSE(vid.open_existing("/memvid_zerohdr"));
    }
}


//-------------------------------------------------------------------
TEST_CASE("MemAud", "[memaud]")
{
    SECTION("Invalid parameter rejection")
    {
        mmb::memaud a;
        REQUIRE_FALSE(a.open("/memaud_bad", true,  0, 16, 44100, 30, 3)); // zero channels
        REQUIRE_FALSE(a.open("/memaud_bad", true,  2, 12, 44100, 30, 3)); // bps not 8 or 16
        REQUIRE_FALSE(a.open("/memaud_bad", true,  2, 16,     0, 30, 3)); // zero bitrate
        REQUIRE_FALSE(a.open("/memaud_bad", true,  2, 16, 44100,  0, 3)); // zero fps
        REQUIRE_FALSE(a.open("/memaud_bad", true,  2, 16, 44100, 30, 0)); // zero bufs
    }

    SECTION("Create with 8-bit and verify metadata")
    {
        mmb::memaud aud;
        REQUIRE(aud.open("/memaud_8bit", true, 1, 8, 8000, 100, 4));
        REQUIRE_FALSE(aud.existing());
        REQUIRE(aud.isOpen());
        REQUIRE(aud.getChannels() == 1);
        REQUIRE(aud.getBps()      == 8);
        REQUIRE(aud.getBitRate()  == 8000);
        REQUIRE(aud.getFps()      == 100);
        REQUIRE(aud.getBufs()     == 4);
    }

    SECTION("Create with 16-bit stereo and verify metadata")
    {
        mmb::memaud aud;
        REQUIRE(aud.open("/memaud_16bit", true, 2, 16, 44100, 30, 3));
        REQUIRE(aud.getChannels() == 2);
        REQUIRE(aud.getBps()      == 16);
        REQUIRE(aud.getBitRate()  == 44100);
        REQUIRE(aud.getFps()      == 30);
        REQUIRE(aud.getBufs()     == 3);
        REQUIRE(aud.getBufSize()  > 0);
    }

    SECTION("Attach and verify parameters match")
    {
        mmb::memaud creator, attacher;
        REQUIRE(creator.open("/memaud_attach", true, 1, 16, 16000, 50, 2));
        REQUIRE(attacher.open("/memaud_attach", false, 1, 16, 16000, 50, 2));
        REQUIRE(attacher.existing());
        REQUIRE(attacher.getChannels() == 1);
        REQUIRE(attacher.getBufs()     == 2);
    }

    SECTION("Mismatched parameters on attach must fail")
    {
        mmb::memaud creator, bad;
        REQUIRE(creator.open("/memaud_mismatch", true, 1, 16, 16000, 50, 2));
        REQUIRE_FALSE(bad.open("/memaud_mismatch", false, 2, 16, 16000, 50, 2));
    }

    SECTION("fill() and getBuf() data round-trip")
    {
        mmb::memaud aud;
        REQUIRE(aud.open("/memaud_fill", true, 1, 8, 8000, 100, 3));

        REQUIRE(aud.fill(0, 0xCC));
        mmb::memaud::audview view = aud.getBuf(0);
        REQUIRE(view.m_ptr  != nullptr);
        REQUIRE(view.m_size > 0);

        bool allMatch = true;
        for (int64_t i = 0; i < view.m_size; i++)
            if ((unsigned char)view.m_ptr[i] != 0xCC) { allMatch = false; break; }
        REQUIRE(allMatch);

        REQUIRE(aud.fill(1, 0x00));
        allMatch = true;
        for (int64_t i = 0; i < view.m_size; i++)
            if ((unsigned char)view.m_ptr[i] != 0xCC) { allMatch = false; break; }
        REQUIRE(allMatch);
    }

    SECTION("Pointer arithmetic: setPtr / getPtr / next")
    {
        mmb::memaud aud;
        REQUIRE(aud.open("/memaud_ptr", true, 1, 8, 8000, 100, 4));

        REQUIRE(aud.setPtr(0) == 0);
        REQUIRE(aud.getPtr(0) == 0);
        REQUIRE(aud.getPtr(1) == 1);

        REQUIRE(aud.setPtr(3) == 3);
        REQUIRE(aud.getPtr(1) == 0);  // wrap-around

        REQUIRE(aud.setPtr(0) == 0);
        REQUIRE(aud.next(1)   == 1);
        REQUIRE(aud.next(1)   == 2);
        REQUIRE(aud.next(2)   == 0);
    }

    SECTION("Pointer arithmetic wraps correctly for multi-step offsets")
    {
        mmb::memaud aud;
        REQUIRE(aud.open("/memaud_multiwrap", true, 1, 8, 8000, 100, 4));
        REQUIRE(aud.setPtr(0) == 0);

        // getPtr: offsets beyond bufs
        REQUIRE(aud.getPtr(4)  == 0);  // full lap lands on 0
        REQUIRE(aud.getPtr(5)  == 1);  // one past a full lap
        REQUIRE(aud.getPtr(8)  == 0);  // two full laps
        REQUIRE(aud.getPtr(-5) == 3);  // negative multi-step

        // setPtr: values >= bufs or negative
        REQUIRE(aud.setPtr(5)  == 1);  // 5 % 4 == 1
        REQUIRE(aud.setPtr(-1) == 3);  // wraps to 3

        // getBuf: old code threw for idx >= 2*bufs; new code wraps
        REQUIRE_NOTHROW(aud.getBuf(8));   // 8 % 4 == 0
        REQUIRE_NOTHROW(aud.getBuf(-5));  // wraps to 3
    }

    SECTION("getPtrErr: circular distance")
    {
        mmb::memaud aud;
        REQUIRE(aud.open("/memaud_ptrerr", true, 1, 8, 8000, 100, 8));
        REQUIRE(aud.setPtr(4) == 4);
        REQUIRE(aud.getPtrErr(4, 0) == 0);
        REQUIRE(aud.getPtrErr(5, 1) == 0);
    }

    SECTION("open_existing")
    {
        mmb::memaud creator, ex;
        REQUIRE(creator.open("/memaud_existing", true, 1, 8, 8000, 100, 2));
        REQUIRE(ex.open_existing("/memaud_existing"));
        REQUIRE(ex.isOpen());
    }

    SECTION("open_existing rejects share with zeroed headers")
    {
        mmb::memmap raw;
        REQUIRE(raw.open("/memaud_zerohdr", 256, true, true));

        mmb::memaud aud;
        REQUIRE_FALSE(aud.open_existing("/memaud_zerohdr"));
    }
}
