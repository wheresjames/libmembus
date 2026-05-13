
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

    SECTION("Sequence counter starts at zero and advances with next()")
    {
        mmb::memvid vid;
        REQUIRE(vid.open("/memvid_seq", true, 16, 8, 24, 30, 4));

        REQUIRE(vid.getSeq() == 0);
        vid.next(1);
        REQUIRE(vid.getSeq() == 1);
        REQUIRE(vid.getFrameSeq(0) == 1);  // slot 0 was stamped before advancing

        vid.next(1);
        REQUIRE(vid.getSeq() == 2);
        REQUIRE(vid.getFrameSeq(1) == 2);

        // slot 0 should still hold seq 1 (not yet overwritten)
        REQUIRE(vid.getFrameSeq(0) == 1);
    }

    SECTION("Overrun detection: lag = getSeq() - rLastSeq >= getBufs()")
    {
        // 4-slot ring buffer.  The reader is lapped when the writer is a full ring
        // ahead of the reader's last processed sequence number.
        mmb::memvid vid;
        REQUIRE(vid.open("/memvid_overrun", true, 16, 8, 24, 30, 4));

        // Writer publishes slot 0; reader records it
        vid.next(1);                             // publishes slot 0, ptr -> 1
        int64_t rPos     = 0;
        int64_t rLastSeq = vid.getFrameSeq(0);  // == 1
        REQUIRE(rLastSeq == 1);

        // Slot holds what we expect — no overrun yet
        REQUIRE(vid.getFrameSeq(rPos) == rLastSeq);
        REQUIRE((vid.getSeq() - rLastSeq) < vid.getBufs());

        // Writer advances bufs more frames so it is a full ring ahead
        for (int i = 0; i < vid.getBufs(); i++)
            vid.next(1);
        // getSeq() == 5, ptr == 1 (pointing at slot 1 — about to overwrite it)

        int64_t lag = vid.getSeq() - rLastSeq;  // 5 - 1 = 4
        REQUIRE(lag >= vid.getBufs());           // reader is lapped

        // getFrameSeq reports what is NOW in rPos (slot 0 was re-stamped to seq 5)
        // Use it to confirm the slot contents changed since the reader was there
        REQUIRE(vid.getFrameSeq(rPos) != rLastSeq);

        // Reader resyncs to current write position
        rPos     = vid.getPtr(0);
        rLastSeq = vid.getSeq();

        // Next frame after resync arrives in sync
        vid.next(1);
        REQUIRE(vid.getFrameSeq(rPos) == rLastSeq + 1);
        REQUIRE((vid.getSeq() - rLastSeq) < vid.getBufs());
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

    SECTION("Sequence counter starts at zero and advances with next()")
    {
        mmb::memaud aud;
        REQUIRE(aud.open("/memaud_seq", true, 1, 8, 8000, 100, 4));

        REQUIRE(aud.getSeq() == 0);
        aud.next(1);
        REQUIRE(aud.getSeq() == 1);
        REQUIRE(aud.getFrameSeq(0) == 1);

        aud.next(1);
        REQUIRE(aud.getSeq() == 2);
        REQUIRE(aud.getFrameSeq(1) == 2);
        REQUIRE(aud.getFrameSeq(0) == 1);  // slot 0 not yet overwritten
    }

    SECTION("Overrun detection: lag = getSeq() - rLastSeq >= getBufs()")
    {
        mmb::memaud aud;
        REQUIRE(aud.open("/memaud_overrun", true, 1, 8, 8000, 100, 4));

        aud.next(1);
        int64_t rPos     = 0;
        int64_t rLastSeq = aud.getFrameSeq(0);  // == 1
        REQUIRE(rLastSeq == 1);

        REQUIRE(aud.getFrameSeq(rPos) == rLastSeq);
        REQUIRE((aud.getSeq() - rLastSeq) < aud.getBufs());

        for (int i = 0; i < aud.getBufs(); i++)
            aud.next(1);

        int64_t lag = aud.getSeq() - rLastSeq;
        REQUIRE(lag >= aud.getBufs());
        REQUIRE(aud.getFrameSeq(rPos) != rLastSeq);

        rPos     = aud.getPtr(0);
        rLastSeq = aud.getSeq();

        aud.next(1);
        REQUIRE(aud.getFrameSeq(rPos) == rLastSeq + 1);
        REQUIRE((aud.getSeq() - rLastSeq) < aud.getBufs());
    }
}


//-------------------------------------------------------------------
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
}


//-------------------------------------------------------------------
TEST_CASE("MemKV", "[memkv]")
{
    SECTION("Create and verify schema")
    {
        mmb::memkv kv;
        REQUIRE(kv.create("/memkv_schema", 4, 31, 63));
        REQUIRE_FALSE(kv.existing());
        REQUIRE(kv.count()       == 4);
        REQUIRE(kv.maxNameLen()  == 31);
        REQUIRE(kv.maxValueLen() == 63);
        REQUIRE(kv.isOpen());
    }

    SECTION("setName and getValue by index and by name")
    {
        mmb::memkv kv;
        REQUIRE(kv.create("/memkv_names", 3, 15, 31));
        REQUIRE(kv.setName(0, "pan"));
        REQUIRE(kv.setName(1, "tilt"));
        REQUIRE(kv.setName(2, "zoom"));

        REQUIRE(kv.setValue(0, "-15"));
        REQUIRE(kv.setValue("tilt", "5"));

        REQUIRE(kv.getValue(0)      == "-15");
        REQUIRE(kv.getValue("tilt") == "5");
        REQUIRE(kv.getValue("zoom") == "");
        REQUIRE(kv.getName(1)       == "tilt");
    }

    SECTION("open() attaches without ownership; both handles can read and write")
    {
        mmb::memkv owner, other;
        REQUIRE(owner.create("/memkv_open", 2, 15, 31));
        REQUIRE(owner.setName(0, "x"));
        REQUIRE(owner.setName(1, "y"));
        REQUIRE(owner.setValue(0, "10"));

        REQUIRE(other.open("/memkv_open"));
        REQUIRE(other.existing());
        REQUIRE(other.count() == 2);
        REQUIRE(other.getValue("x") == "10");

        REQUIRE(other.setValue("y", "20"));
        REQUIRE(owner.getValue("y") == "20");
    }

    SECTION("setAll writes all slots atomically with one epoch increment")
    {
        mmb::memkv kv;
        REQUIRE(kv.create("/memkv_setall", 3, 15, 31));
        REQUIRE(kv.setName(0, "pan"));
        REQUIRE(kv.setName(1, "tilt"));
        REQUIRE(kv.setName(2, "zoom"));

        int64_t e0 = kv.getEpoch();
        REQUIRE(kv.setAll({{"pan", "-15"}, {"tilt", "5"}, {"zoom", "1.4"}}));
        REQUIRE(kv.getEpoch() == e0 + 1);

        REQUIRE(kv.getValue("pan")  == "-15");
        REQUIRE(kv.getValue("tilt") == "5");
        REQUIRE(kv.getValue("zoom") == "1.4");
    }

    SECTION("getAll returns a consistent snapshot")
    {
        mmb::memkv kv;
        REQUIRE(kv.create("/memkv_getall", 3, 15, 31));
        REQUIRE(kv.setName(0, "a"));
        REQUIRE(kv.setName(1, "b"));
        REQUIRE(kv.setName(2, "c"));
        REQUIRE(kv.setAll({{"a", "1"}, {"b", "2"}, {"c", "3"}}));

        auto all = kv.getAll();
        REQUIRE(all.size() == 3);
        REQUIRE(all["a"] == "1");
        REQUIRE(all["b"] == "2");
        REQUIRE(all["c"] == "3");
    }

    SECTION("epoch increments once per setValue, once per setAll batch")
    {
        mmb::memkv kv;
        REQUIRE(kv.create("/memkv_epoch", 3, 15, 31));
        REQUIRE(kv.setName(0, "a"));
        REQUIRE(kv.setName(1, "b"));
        REQUIRE(kv.setName(2, "c"));
        REQUIRE(kv.getEpoch() == 0);

        REQUIRE(kv.setValue(0, "x")); REQUIRE(kv.getEpoch() == 1);
        REQUIRE(kv.setValue(1, "y")); REQUIRE(kv.getEpoch() == 2);

        REQUIRE(kv.setAll({{"a", "A"}, {"b", "B"}, {"c", "C"}}));
        REQUIRE(kv.getEpoch() == 3);   // +1, not +3
    }

    SECTION("getChanged (non-blocking) returns only modified slots")
    {
        mmb::memkv kv;
        REQUIRE(kv.create("/memkv_gchanged", 3, 15, 31));
        REQUIRE(kv.setName(0, "pan"));
        REQUIRE(kv.setName(1, "tilt"));
        REQUIRE(kv.setName(2, "zoom"));
        REQUIRE(kv.setAll({{"pan", "0"}, {"tilt", "0"}, {"zoom", "1.0"}}));

        int64_t epoch = kv.getEpoch();
        REQUIRE(kv.setValue("pan", "-15"));

        auto changed = kv.getChanged(epoch);
        REQUIRE(changed.size() == 1);
        REQUIRE(changed["pan"] == "-15");
        REQUIRE(changed.count("tilt") == 0);
        REQUIRE(changed.count("zoom") == 0);
    }

    SECTION("getChanged (blocking) returns changes already available")
    {
        mmb::memkv kv;
        REQUIRE(kv.create("/memkv_gchanged_wait", 2, 15, 31));
        REQUIRE(kv.setName(0, "x"));
        REQUIRE(kv.setName(1, "y"));
        REQUIRE(kv.setAll({{"x", "0"}, {"y", "0"}}));

        int64_t epoch = kv.getEpoch();
        REQUIRE(kv.setValue("x", "42"));

        auto changed = kv.getChanged(100, epoch);
        REQUIRE(changed.size() == 1);
        REQUIRE(changed["x"] == "42");
    }

    SECTION("getChanged (blocking) returns empty on timeout")
    {
        mmb::memkv kv;
        REQUIRE(kv.create("/memkv_gchanged_timeout", 1, 15, 31));
        REQUIRE(kv.setName(0, "v"));
        REQUIRE(kv.setValue(0, "0"));

        int64_t epoch = kv.getEpoch();
        REQUIRE(kv.getChanged(50, epoch).empty());
    }

    SECTION("waitForChange returns false on timeout")
    {
        mmb::memkv kv;
        REQUIRE(kv.create("/memkv_wfc_timeout", 1, 15, 31));
        REQUIRE(kv.setName(0, "v"));
        REQUIRE(kv.setValue(0, "0"));

        int64_t epoch = kv.getEpoch();
        REQUIRE_FALSE(kv.waitForChange(50, epoch));
    }

    SECTION("waitForChange returns true immediately when epoch already advanced")
    {
        mmb::memkv kv;
        REQUIRE(kv.create("/memkv_wfc_immediate", 1, 15, 31));
        REQUIRE(kv.setName(0, "v"));

        int64_t epoch = kv.getEpoch();   // 0
        REQUIRE(kv.setValue(0, "99"));   // epoch → 1
        REQUIRE(kv.waitForChange(5000, epoch));
        REQUIRE(epoch == 1);
    }

    SECTION("findName returns correct index, -1 for unknown")
    {
        mmb::memkv kv;
        REQUIRE(kv.create("/memkv_find", 3, 15, 31));
        REQUIRE(kv.setName(0, "alpha"));
        REQUIRE(kv.setName(1, "beta"));
        REQUIRE(kv.setName(2, "gamma"));

        REQUIRE(kv.findName("alpha") == 0);
        REQUIRE(kv.findName("beta")  == 1);
        REQUIRE(kv.findName("gamma") == 2);
        REQUIRE(kv.findName("delta") == -1);
    }

    SECTION("Name too long is rejected; value too long is rejected")
    {
        mmb::memkv kv;
        REQUIRE(kv.create("/memkv_limits", 2, 4, 8));

        REQUIRE(kv.setName(0, "abcd"));           // exactly maxNameLen — OK
        REQUIRE_FALSE(kv.setName(1, "toolong"));  // 7 > 4 — rejected
        REQUIRE(kv.setName(1, "ok"));

        REQUIRE(kv.setValue(0, "12345678"));          // exactly maxValueLen — OK
        REQUIRE_FALSE(kv.setValue(0, "123456789"));   // 9 > 8 — rejected
    }

    SECTION("bNew forces recreation; fresh store has epoch 0 and empty values")
    {
        mmb::memkv fresh;
        REQUIRE(fresh.create("/memkv_bnew", 2, 15, 31, /*bNew=*/true));
        REQUIRE_FALSE(fresh.existing());
        REQUIRE(fresh.getEpoch() == 0);
        REQUIRE(fresh.setName(0, "v"));
        REQUIRE(fresh.getValue(0) == "");
    }

    SECTION("Multiple independent readers see consistent values")
    {
        mmb::memkv w, r1, r2;
        REQUIRE(w.create("/memkv_multireader", 2, 15, 31));
        REQUIRE(w.setName(0, "x"));
        REQUIRE(w.setName(1, "y"));
        REQUIRE(w.setAll({{"x", "10"}, {"y", "20"}}));

        REQUIRE(r1.open("/memkv_multireader"));
        REQUIRE(r2.open("/memkv_multireader"));

        REQUIRE(r1.getValue("x") == "10");
        REQUIRE(r2.getValue("y") == "20");

        REQUIRE(w.setValue("x", "99"));
        REQUIRE(r1.getValue("x") == "99");
        REQUIRE(r2.getValue("x") == "99");
    }

    SECTION("setAll silently skips unknown names and still updates known ones")
    {
        mmb::memkv kv;
        REQUIRE(kv.create("/memkv_skip", 2, 15, 31));
        REQUIRE(kv.setName(0, "known"));
        REQUIRE(kv.setName(1, "also_known"));

        REQUIRE(kv.setAll({{"known", "yes"}, {"mystery", "ignored"}, {"also_known", "also_yes"}}));
        REQUIRE(kv.getValue("known")      == "yes");
        REQUIRE(kv.getValue("also_known") == "also_yes");
    }
}
