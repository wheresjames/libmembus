
#include <catch2/catch_test_macros.hpp>
#include "libmembus.h"


TEST_CASE("MemVid", "[memvid]")
{
    SECTION("Invalid parameter rejection")
    {
        mmb::memvid v;
        REQUIRE_FALSE(v.open("/memvid_bad", true,  0,  0, mmb::video_format::rgb24, 30, 3));  // zero w/h
        REQUIRE_FALSE(v.open("/memvid_bad", true, 64, 48, (mmb::video_format)0, 30, 3));  // invalid format
        REQUIRE_FALSE(v.open("/memvid_bad", true, 64, 48, mmb::video_format::rgb24,  0, 3));  // zero fps
        REQUIRE_FALSE(v.open("/memvid_bad", true, 64, 48, mmb::video_format::rgb24, 30, 0));  // zero bufs
        REQUIRE_FALSE(v.open("/memvid_bad", true, 63, 48, mmb::video_format::yuyv422, 30, 3));  // 422 requires even width
    }

    SECTION("Create and verify metadata")
    {
        mmb::memvid vid;
        REQUIRE(vid.open("/memvid_meta", true, 64, 48, mmb::video_format::rgb24, 30, 4));
        REQUIRE_FALSE(vid.existing());
        REQUIRE(vid.isOpen());
        REQUIRE(vid.getWidth()  == 64);
        REQUIRE(vid.getHeight() == 48);
        REQUIRE(vid.getFormat() == mmb::video_format::rgb24);
        REQUIRE(std::string(vid.getFormatName()) == "RGB24");
        REQUIRE(vid.getFps()    == 30);
        REQUIRE(vid.getBufs()   == 4);
    }

    SECTION("Supported packed formats compute expected scan widths")
    {
        struct format_case { const char *name; mmb::video_format fmt; int64_t bytes; };
        format_case cases[] = {
            {"/memvid_fmt_gray8",   mmb::video_format::gray8,   1},
            {"/memvid_fmt_bgr24",   mmb::video_format::bgr24,   3},
            {"/memvid_fmt_rgba32",  mmb::video_format::rgba32,  4},
            {"/memvid_fmt_bgra32",  mmb::video_format::bgra32,  4},
            {"/memvid_fmt_yuyv422", mmb::video_format::yuyv422, 2},
            {"/memvid_fmt_uyvy422", mmb::video_format::uyvy422, 2},
        };

        for (const format_case &c : cases)
        {
            mmb::memvid vid;
            REQUIRE(vid.open(c.name, true, 16, 8, c.fmt, 30, 2));
            REQUIRE(vid.getFormat() == c.fmt);
            REQUIRE(vid.getBuf(0).m_sw == 16 * c.bytes);
            REQUIRE(vid.getBuf(0).m_size == 16 * 8 * c.bytes);
            REQUIRE(vid.getBuf(0).m_format == c.fmt);
        }
    }

    SECTION("Attach to existing share and verify metadata")
    {
        mmb::memvid creator, attacher;
        REQUIRE(creator.open("/memvid_attach", true, 32, 24, mmb::video_format::rgb24, 25, 2));
        REQUIRE(attacher.open("/memvid_attach", false, 32, 24, mmb::video_format::rgb24, 25, 2));
        REQUIRE(attacher.existing());
        REQUIRE(attacher.getWidth()  == 32);
        REQUIRE(attacher.getHeight() == 24);
        REQUIRE(attacher.getBufs()   == 2);
    }

    SECTION("Mismatched parameters on attach must fail")
    {
        mmb::memvid creator, bad;
        REQUIRE(creator.open("/memvid_mismatch", true, 32, 24, mmb::video_format::rgb24, 25, 2));
        REQUIRE_FALSE(bad.open("/memvid_mismatch", false, 64, 24, mmb::video_format::rgb24, 25, 2));
    }

    SECTION("fill() and getBuf() data round-trip")
    {
        mmb::memvid vid;
        REQUIRE(vid.open("/memvid_fill", true, 16, 8, mmb::video_format::rgb24, 30, 3));

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
        REQUIRE(vid.open("/memvid_ptr", true, 16, 8, mmb::video_format::rgb24, 30, 4));

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
        REQUIRE(vid.open("/memvid_multiwrap", true, 16, 8, mmb::video_format::rgb24, 30, 4));
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
        REQUIRE(vid.open("/memvid_ptrerr", true, 16, 8, mmb::video_format::rgb24, 30, 8));
        REQUIRE(vid.setPtr(4) == 4);
        REQUIRE(vid.getPtrErr(4, 0) == 0);
        REQUIRE(vid.getPtrErr(5, 1) == 0);
    }

    SECTION("open_existing")
    {
        mmb::memvid creator, ex;
        REQUIRE(creator.open("/memvid_existing", true, 16, 8, mmb::video_format::rgb24, 30, 2));
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

    SECTION("open_existing rejects headers whose layout exceeds the mapped size")
    {
        mmb::memmap raw;
        REQUIRE(raw.open("/memvid_badhdr", 128, true, true));
        char *p = raw.data();
        REQUIRE(p != nullptr);

        *(int64_t*)(p + mmb::memvid::hv_size)      = 4096;
        *(int64_t*)(p + mmb::memvid::hv_width)     = 64;
        *(int64_t*)(p + mmb::memvid::hv_height)    = 64;
        *(int64_t*)(p + mmb::memvid::hv_scanwidth) = 192;
        *(int64_t*)(p + mmb::memvid::hv_format)    = (int64_t)mmb::video_format::rgb24;
        *(int64_t*)(p + mmb::memvid::hv_fps)       = 30;
        *(int64_t*)(p + mmb::memvid::hv_bufs)      = 2;
        *(int64_t*)(p + mmb::memvid::hv_blocksz)   = mmb::memvid::fv_last + (192 * 64);

        mmb::memvid vid;
        REQUIRE_FALSE(vid.open_existing("/memvid_badhdr"));
    }

    SECTION("Sequence counter starts at zero and advances with next()")
    {
        mmb::memvid vid;
        REQUIRE(vid.open("/memvid_seq", true, 16, 8, mmb::video_format::rgb24, 30, 4));

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

    SECTION("getBuf throws when bufs zeroed after open (TOCTOU guard)")
    {
        // Simulate a malicious/buggy peer modifying hv_bufs to 0 after
        // open_existing() has already validated the layout.
        mmb::memvid writer;
        REQUIRE(writer.open("/memvid_toctou_bufs", true, 16, 8, mmb::video_format::rgb24, 30, 4));
        writer.next(1);

        mmb::memvid reader;
        REQUIRE(reader.open_existing("/memvid_toctou_bufs"));
        REQUIRE_NOTHROW(reader.getBuf(0));  // clean before corruption

        mmb::memmap raw;
        REQUIRE(raw.open("/memvid_toctou_bufs", 0, false, false));
        *(int64_t*)(raw.data() + mmb::memvid::hv_bufs) = 0;

        REQUIRE_THROWS(reader.getBuf(0));
    }

    SECTION("getBuf throws when blocksz inflated to cause out-of-bounds (TOCTOU guard)")
    {
        // Simulate a peer inflating hv_blocksz so slot 1 would map far outside
        // the mapped region; the bounds check added to getBuf() must catch it.
        mmb::memvid writer;
        REQUIRE(writer.open("/memvid_toctou_oob", true, 16, 8, mmb::video_format::rgb24, 30, 2));

        mmb::memvid reader;
        REQUIRE(reader.open_existing("/memvid_toctou_oob"));
        REQUIRE_NOTHROW(reader.getBuf(0));

        mmb::memmap raw;
        REQUIRE(raw.open("/memvid_toctou_oob", 0, false, false));
        *(int64_t*)(raw.data() + mmb::memvid::hv_blocksz) = int64_t(1) << 30;

        REQUIRE_THROWS(reader.getBuf(1));
    }

    SECTION("Overrun detection: lag = getSeq() - rLastSeq >= getBufs()")
    {
        // 4-slot ring buffer.  The reader is lapped when the writer is a full ring
        // ahead of the reader's last processed sequence number.
        mmb::memvid vid;
        REQUIRE(vid.open("/memvid_overrun", true, 16, 8, mmb::video_format::rgb24, 30, 4));

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
