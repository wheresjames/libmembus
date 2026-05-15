
#include <catch2/catch_test_macros.hpp>
#include "libmembus.h"


TEST_CASE("MemAud", "[memaud]")
{
    SECTION("Invalid parameter rejection")
    {
        mmb::memaud a;
        REQUIRE_FALSE(a.open("/memaud_bad", true,  0, mmb::audio_format::s16le, 44100, 30, 3)); // zero channels
        REQUIRE_FALSE(a.open("/memaud_bad", true,  2, (mmb::audio_format)0,     44100, 30, 3)); // invalid format
        REQUIRE_FALSE(a.open("/memaud_bad", true,  2, mmb::audio_format::s16le,     0, 30, 3)); // zero sample rate
        REQUIRE_FALSE(a.open("/memaud_bad", true,  2, mmb::audio_format::s16le, 44100,  0, 3)); // zero fps
        REQUIRE_FALSE(a.open("/memaud_bad", true,  2, mmb::audio_format::s16le, 44100, 30, 0)); // zero bufs
    }

    SECTION("Create with U8 and verify metadata")
    {
        mmb::memaud aud;
        REQUIRE(aud.open("/memaud_8bit", true, 1, mmb::audio_format::u8, 8000, 100, 4));
        REQUIRE_FALSE(aud.existing());
        REQUIRE(aud.isOpen());
        REQUIRE(aud.getChannels() == 1);
        REQUIRE(aud.getFormat()   == mmb::audio_format::u8);
        REQUIRE(std::string(aud.getFormatName()) == "U8");
        REQUIRE(aud.getBytesPerSample() == 1);
        REQUIRE(aud.getSampleRate() == 8000);
        REQUIRE(aud.getFps()      == 100);
        REQUIRE(aud.getBufs()     == 4);
    }

    SECTION("Create with S16LE stereo and verify metadata")
    {
        mmb::memaud aud;
        REQUIRE(aud.open("/memaud_16bit", true, 2, mmb::audio_format::s16le, 44100, 30, 3));
        REQUIRE(aud.getChannels() == 2);
        REQUIRE(aud.getFormat()   == mmb::audio_format::s16le);
        REQUIRE(std::string(aud.getFormatName()) == "S16LE");
        REQUIRE(aud.getBytesPerSample() == 2);
        REQUIRE(aud.getSampleRate() == 44100);
        REQUIRE(aud.getFps()      == 30);
        REQUIRE(aud.getBufs()     == 3);
        REQUIRE(aud.getBufSize()  > 0);
    }

    SECTION("Supported sample formats compute expected buffer sizes")
    {
        struct sample_case { const char *name; mmb::audio_format fmt; int64_t bytes; };
        sample_case cases[] = {
            {"/memaud_fmt_u8",    mmb::audio_format::u8,    1},
            {"/memaud_fmt_s24le", mmb::audio_format::s24le, 3},
            {"/memaud_fmt_f32le", mmb::audio_format::f32le, 4},
            {"/memaud_fmt_f64le", mmb::audio_format::f64le, 8},
        };

        for (const sample_case &c : cases)
        {
            mmb::memaud aud;
            REQUIRE(aud.open(c.name, true, 2, c.fmt, 1000, 100, 2));
            REQUIRE(aud.getBytesPerSample() == c.bytes);
            REQUIRE(aud.getBufSize() == 10 * 2 * c.bytes);
            REQUIRE(aud.getBuf(0).m_format == c.fmt);
        }
    }

    SECTION("Attach and verify parameters match")
    {
        mmb::memaud creator, attacher;
        REQUIRE(creator.open("/memaud_attach", true, 1, mmb::audio_format::s16le, 16000, 50, 2));
        REQUIRE(attacher.open("/memaud_attach", false, 1, mmb::audio_format::s16le, 16000, 50, 2));
        REQUIRE(attacher.existing());
        REQUIRE(attacher.getChannels() == 1);
        REQUIRE(attacher.getBufs()     == 2);
    }

    SECTION("Mismatched parameters on attach must fail")
    {
        mmb::memaud creator, bad;
        REQUIRE(creator.open("/memaud_mismatch", true, 1, mmb::audio_format::s16le, 16000, 50, 2));
        REQUIRE_FALSE(bad.open("/memaud_mismatch", false, 2, mmb::audio_format::s16le, 16000, 50, 2));
    }

    SECTION("fill() and getBuf() data round-trip")
    {
        mmb::memaud aud;
        REQUIRE(aud.open("/memaud_fill", true, 1, mmb::audio_format::u8, 8000, 100, 3));

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
        REQUIRE(aud.open("/memaud_ptr", true, 1, mmb::audio_format::u8, 8000, 100, 4));

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
        REQUIRE(aud.open("/memaud_multiwrap", true, 1, mmb::audio_format::u8, 8000, 100, 4));
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
        REQUIRE(aud.open("/memaud_ptrerr", true, 1, mmb::audio_format::u8, 8000, 100, 8));
        REQUIRE(aud.setPtr(4) == 4);
        REQUIRE(aud.getPtrErr(4, 0) == 0);
        REQUIRE(aud.getPtrErr(5, 1) == 0);
    }

    SECTION("open_existing")
    {
        mmb::memaud creator, ex;
        REQUIRE(creator.open("/memaud_existing", true, 1, mmb::audio_format::u8, 8000, 100, 2));
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

    SECTION("open_existing rejects headers whose layout exceeds the mapped size")
    {
        mmb::memmap raw;
        REQUIRE(raw.open("/memaud_badhdr", 128, true, true));
        char *p = raw.data();
        REQUIRE(p != nullptr);

        *(int64_t*)(p + mmb::memaud::hv_size)       = 4096;
        *(int64_t*)(p + mmb::memaud::hv_ch)         = 2;
        *(int64_t*)(p + mmb::memaud::hv_format)     = (int64_t)mmb::audio_format::s16le;
        *(int64_t*)(p + mmb::memaud::hv_samplerate) = 48000;
        *(int64_t*)(p + mmb::memaud::hv_fps)        = 100;
        *(int64_t*)(p + mmb::memaud::hv_bufs)       = 2;
        *(int64_t*)(p + mmb::memaud::hv_blocksz)    = mmb::memaud::fv_last + (480 * 2 * 2);

        mmb::memaud aud;
        REQUIRE_FALSE(aud.open_existing("/memaud_badhdr"));
    }

    SECTION("Sequence counter starts at zero and advances with next()")
    {
        mmb::memaud aud;
        REQUIRE(aud.open("/memaud_seq", true, 1, mmb::audio_format::u8, 8000, 100, 4));

        REQUIRE(aud.getSeq() == 0);
        aud.next(1);
        REQUIRE(aud.getSeq() == 1);
        REQUIRE(aud.getFrameSeq(0) == 1);

        aud.next(1);
        REQUIRE(aud.getSeq() == 2);
        REQUIRE(aud.getFrameSeq(1) == 2);
        REQUIRE(aud.getFrameSeq(0) == 1);  // slot 0 not yet overwritten
    }

    SECTION("getBufSize uses ceiling division to avoid audio clock drift")
    {
        // When sampleRate is not divisible by fps, truncation would lose samples
        // each frame and cause long-running drift.  Ceiling division prevents this.
        //   100 Hz / 7 fps: floor=14, ceil=15  → 15 * 7 = 105 >= 100 (no deficit)
        //   44100 Hz / 11 fps: floor=4009, ceil=4010  → 4010 * 11 = 44110 >= 44100
        mmb::memaud aud;
        REQUIRE(aud.open("/memaud_ceil", true, 1, mmb::audio_format::u8, 100, 7, 2));
        REQUIRE(aud.getBufSize() == 15);   // ceil(100/7) = 15 samples * 1ch * 1B

        mmb::memaud aud2;
        REQUIRE(aud2.open("/memaud_ceil2", true, 1, mmb::audio_format::u8, 44100, 11, 2));
        REQUIRE(aud2.getBufSize() == 4010);  // ceil(44100/11) = 4010
    }

    SECTION("getBuf throws when bufs zeroed after open (TOCTOU guard)")
    {
        mmb::memaud writer;
        REQUIRE(writer.open("/memaud_toctou_bufs", true, 1, mmb::audio_format::u8, 8000, 100, 4));
        writer.next(1);

        mmb::memaud reader;
        REQUIRE(reader.open_existing("/memaud_toctou_bufs"));
        REQUIRE_NOTHROW(reader.getBuf(0));

        mmb::memmap raw;
        REQUIRE(raw.open("/memaud_toctou_bufs", 0, false, false));
        *(int64_t*)(raw.data() + mmb::memaud::hv_bufs) = 0;

        REQUIRE_THROWS(reader.getBuf(0));
    }

    SECTION("getBuf throws when blocksz inflated to cause out-of-bounds (TOCTOU guard)")
    {
        mmb::memaud writer;
        REQUIRE(writer.open("/memaud_toctou_oob", true, 1, mmb::audio_format::u8, 8000, 100, 2));

        mmb::memaud reader;
        REQUIRE(reader.open_existing("/memaud_toctou_oob"));
        REQUIRE_NOTHROW(reader.getBuf(0));

        mmb::memmap raw;
        REQUIRE(raw.open("/memaud_toctou_oob", 0, false, false));
        *(int64_t*)(raw.data() + mmb::memaud::hv_blocksz) = int64_t(1) << 30;

        REQUIRE_THROWS(reader.getBuf(1));
    }

    SECTION("setPts / getPts round-trip")
    {
        mmb::memaud aud;
        REQUIRE(aud.open("/memaud_pts", true, 1, mmb::audio_format::u8, 8000, 100, 4));

        // All slots start at zero
        REQUIRE(aud.getPts(0) == 0);

        REQUIRE(aud.setPts(0, 5000000LL));
        REQUIRE(aud.getPts(0) == 5000000LL);

        // Other slots unchanged
        REQUIRE(aud.getPts(1) == 0);

        // Index wraps (bufs=4): slot 4 → slot 0
        REQUIRE(aud.getPts(4) == 5000000LL);

        // Negative timestamps stored verbatim
        REQUIRE(aud.setPts(1, -42));
        REQUIRE(aud.getPts(1) == -42);
    }

    SECTION("setPts survives next() without being overwritten")
    {
        mmb::memaud aud;
        REQUIRE(aud.open("/memaud_pts_next", true, 1, mmb::audio_format::u8, 8000, 100, 3));
        REQUIRE(aud.setPts(0, 77));
        aud.next(1);  // stamps seq on slot 0, advances ptr to 1
        REQUIRE(aud.getPts(0) == 77);
    }

    SECTION("memaud_reader exposes lastPts() after readNext()")
    {
        mmb::memaud_writer aw;
        REQUIRE(aw.open("/memaud_pts_reader", 1, mmb::audio_format::u8, 8000, 100, 3));

        mmb::memaud_reader ar;
        REQUIRE(ar.open("/memaud_pts_reader"));

        REQUIRE(aw.setPts(3333333LL));
        aw.next(1);

        REQUIRE(ar.wait(100));
        ar.readNext();
        REQUIRE(ar.lastPts() == 3333333LL);
    }

    SECTION("Overrun detection: lag = getSeq() - rLastSeq >= getBufs()")
    {
        mmb::memaud aud;
        REQUIRE(aud.open("/memaud_overrun", true, 1, mmb::audio_format::u8, 8000, 100, 4));

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
