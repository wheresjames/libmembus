
#include <catch2/catch_test_macros.hpp>
#include "libmembus.h"

#include <string>

TEST_CASE("MemPkt", "[mempkt]")
{
    SECTION("Invalid parameter rejection")
    {
        mmb::mempkt pk;
        REQUIRE_FALSE(pk.open("/mempkt_bad", true, 0, 4096, 512));      // zero bufs
        REQUIRE_FALSE(pk.open("/mempkt_bad", true, 4, 0, 512));         // zero arena
        REQUIRE_FALSE(pk.open("/mempkt_bad", true, 4, 4096, 0));        // zero maxrec
        REQUIRE_FALSE(pk.open("/mempkt_bad", true, 4, 256, 4096));      // maxrec > arena
        REQUIRE_FALSE(pk.open("/mempkt_bad", true, 4, 4096, 512, 7));   // align not power of two
    }

    SECTION("Create and verify metadata")
    {
        mmb::mempkt pk;
        REQUIRE(pk.open("/mempkt_meta", true, 8, 65536, 4096, 64, 0x4753504au /*'JPSG'*/));
        REQUIRE(pk.isOpen());
        REQUIRE(pk.getBufs() == 8);
        REQUIRE(pk.getArenaSize() == 65536);
        REQUIRE(pk.getMaxRec() == 4096);
        REQUIRE(pk.getFourcc() == 0x4753504au);
        REQUIRE(pk.getSeq() == 0);
        REQUIRE(pk.getWcursor() == 0);
    }

    SECTION("Single-process write / read round-trip")
    {
        mmb::mempkt_writer w;
        REQUIRE(w.open("/mempkt_rt", 8, 65536, 4096));

        mmb::mempkt_reader r;
        REQUIRE(r.open("/mempkt_rt"));

        std::string a = "hello world";
        std::string b(1000, 'x');
        REQUIRE(w.write(a, mmb::pkt_kind::video, 1, 111) >= 0);
        REQUIRE(w.write(b, mmb::pkt_kind::audio, 2, 222) >= 0);

        std::string payload, meta;
        mmb::mempkt::recinfo info;
        bool overrun = false;

        REQUIRE(r.wait(100));
        REQUIRE(r.readNext(payload, meta, info, &overrun));
        REQUIRE_FALSE(overrun);
        REQUIRE(payload == a);
        REQUIRE(info.kind == (int64_t)mmb::pkt_kind::video);
        REQUIRE(info.track == 1);
        REQUIRE(info.pts == 111);

        REQUIRE(r.readNext(payload, meta, info, &overrun));
        REQUIRE(payload == b);
        REQUIRE(info.kind == (int64_t)mmb::pkt_kind::audio);
        REQUIRE(info.pts == 222);
    }

    SECTION("Per-record metadata round-trip")
    {
        mmb::mempkt_writer w;
        REQUIRE(w.open("/mempkt_meta_rec", 8, 65536, 4096));
        mmb::mempkt_reader r;
        REQUIRE(r.open("/mempkt_meta_rec"));

        std::string payload = "PAYLOAD";
        std::string sidecar = "{\"ts\":42}";
        REQUIRE(w.write(payload.data(), (int64_t)payload.size(),
                        mmb::pkt_kind::data, 0, 0,
                        sidecar.data(), (int64_t)sidecar.size()) >= 0);

        std::string gotP, gotM;
        mmb::mempkt::recinfo info;
        REQUIRE(r.wait(100));
        REQUIRE(r.readNext(gotP, gotM, info));
        REQUIRE(gotP == payload);
        REQUIRE(gotM == sidecar);
        REQUIRE(info.userlen == (int64_t)sidecar.size());
    }

    SECTION("Records larger than maxrec are rejected")
    {
        mmb::mempkt_writer w;
        REQUIRE(w.open("/mempkt_toobig", 8, 65536, 512));
        std::string big(1000, 'z');
        REQUIRE(w.write(big) == -1);
    }

    SECTION("Variable-length records wrap the arena and stay readable")
    {
        // Small arena forces many wraps; a reader kept in lock-step must see
        // every record intact (no torn reads).
        mmb::mempkt_writer w;
        REQUIRE(w.open("/mempkt_wrap", 16, 8192, 1024));
        mmb::mempkt_reader r;
        REQUIRE(r.open("/mempkt_wrap"));

        std::string payload, meta;
        mmb::mempkt::recinfo info;
        for (int i = 0; i < 200; ++i)
        {
            std::string rec((i % 300) + 1, (char)('A' + (i % 26)));
            REQUIRE(w.write(rec, mmb::pkt_kind::data, 0, i) >= 0);

            bool overrun = false;
            REQUIRE(r.readNext(payload, meta, info, &overrun));
            REQUIRE_FALSE(overrun);
            REQUIRE(payload == rec);
            REQUIRE(info.pts == i);
        }
    }

    SECTION("Overrun is reported when the writer laps the reader")
    {
        mmb::mempkt_writer w;
        REQUIRE(w.open("/mempkt_overrun", 4, 65536, 1024));
        mmb::mempkt_reader r;
        REQUIRE(r.open("/mempkt_overrun"));

        // Write far more than the descriptor ring holds without reading.
        for (int i = 0; i < 20; ++i)
            REQUIRE(w.write(std::string(64, 'q')) >= 0);

        std::string payload, meta;
        mmb::mempkt::recinfo info;
        bool overrun = false;
        REQUIRE_FALSE(r.readNext(payload, meta, info, &overrun));
        REQUIRE(overrun);
    }

    SECTION("open_existing rejects a zeroed / non-libmembus share")
    {
        mmb::memmap raw;
        REQUIRE(raw.open("/mempkt_zero", 4096, true, true));
        mmb::mempkt pk;
        REQUIRE_FALSE(pk.open_existing("/mempkt_zero"));
    }

    SECTION("getRecord stays bounds-safe against post-open header corruption (TOCTOU)")
    {
        mmb::mempkt_writer w;
        REQUIRE(w.open("/mempkt_toctou", 8, 65536, 4096));
        for (int i = 0; i < 3; ++i)
            REQUIRE(w.write(std::string(100, 'a'), mmb::pkt_kind::data, 0, i) >= 0);

        mmb::mempkt reader;
        REQUIRE(reader.open_existing("/mempkt_toctou"));
        std::string pl, mt;
        mmb::mempkt::recinfo info;
        REQUIRE(reader.getRecord(0, pl, mt, info));  // clean before corruption

        mmb::memmap raw;
        REQUIRE(raw.open("/mempkt_toctou", 0, false, false));

        // Inflate the descriptor stride so idx*stride would overflow / point OOB.
        *(int64_t*)(raw.data() + mmb::mempkt::hv_descstride) = int64_t(1) << 40;
        REQUIRE_FALSE(reader.getRecord(2, pl, mt, info));   // must not crash or read OOB
        REQUIRE(reader.getFrameSeq(2) == -1);

        // Zeroing bufs must be handled gracefully.
        *(int64_t*)(raw.data() + mmb::mempkt::hv_descstride) = 64;
        *(int64_t*)(raw.data() + mmb::mempkt::hv_bufs) = 0;
        REQUIRE_FALSE(reader.getRecord(0, pl, mt, info));
    }
}
