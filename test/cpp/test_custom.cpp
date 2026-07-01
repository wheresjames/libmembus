
#include <catch2/catch_test_macros.hpp>
#include "libmembus.h"

#include <cstring>
#include <string>

TEST_CASE("CustomTypes", "[custom]")
{
    SECTION("memvid userType uses caller-supplied scan width")
    {
        mmb::memvid vid;
        // Opaque format: bpp is unknown, so scanwidth must be supplied.
        REQUIRE_FALSE(vid.open("/mv_user_bad", true, 32, 16, mmb::video_format::userType, 30, 3));
        REQUIRE(vid.open("/mv_user", true, 32, 16, mmb::video_format::userType, 30, 3,
                         /*scanwidth=*/100));
        REQUIRE(vid.getFormat() == mmb::video_format::userType);
        REQUIRE(vid.getBuf(0).m_sw == 100);
        REQUIRE(vid.getBuf(0).m_size == 100 * 16);
        REQUIRE(vid.fill(0, 0x7F));
        auto v = vid.getBuf(0);
        REQUIRE((unsigned char)v.m_ptr[0] == 0x7F);
        REQUIRE((unsigned char)v.m_ptr[v.m_size - 1] == 0x7F);
    }

    SECTION("Payload alignment: pixel data starts on the requested boundary")
    {
        // Odd geometry that would misalign an unrounded slot stride.
        mmb::memvid vid;
        REQUIRE(vid.open("/mv_align", true, 641, 481, mmb::video_format::gray8, 30, 3,
                         /*scanwidth=*/0, /*align=*/64));
        REQUIRE(vid.getAlign() == 64);
        for (int i = 0; i < 3; ++i)
        {
            auto v = vid.getBuf(i);
            REQUIRE((reinterpret_cast<uintptr_t>(v.m_ptr) % 64) == 0);
        }
    }

    SECTION("Identity: fourcc and GUID round-trip")
    {
        uint8_t guid[16];
        for (int i = 0; i < 16; ++i) guid[i] = (uint8_t)(i + 1);

        mmb::memvid vid;
        REQUIRE(vid.open("/mv_id", true, 16, 8, mmb::video_format::userType, 30, 2,
                         /*scanwidth=*/16, /*align=*/0, /*frameextra=*/0,
                         /*fourcc=*/0x4750454du /*'MPEG'*/, guid));
        REQUIRE(vid.getFourcc() == 0x4750454du);

        uint8_t out[16] = {0};
        REQUIRE(vid.getGuid(out));
        REQUIRE(std::memcmp(out, guid, 16) == 0);

        // Reader sees the same identity.
        mmb::memvid rd;
        REQUIRE(rd.open_existing("/mv_id"));
        REQUIRE(rd.getFourcc() == 0x4750454du);
        uint8_t out2[16] = {0};
        REQUIRE(rd.getGuid(out2));
        REQUIRE(std::memcmp(out2, guid, 16) == 0);
    }

    SECTION("Main user metadata buffer round-trip")
    {
        std::string sdp = "v=0\r\no=- 0 0 IN IP4 127.0.0.1\r\n";
        mmb::memvid vid;
        REQUIRE(vid.open("/mv_meta", true, 16, 8, mmb::video_format::rgb24, 30, 2,
                         0, 0, 0, 0, nullptr, sdp.data(), (int64_t)sdp.size()));
        REQUIRE(vid.getMetaSize() == (int64_t)sdp.size());

        mmb::memvid rd;
        REQUIRE(rd.open_existing("/mv_meta"));
        REQUIRE(rd.getMetaSize() == (int64_t)sdp.size());
        REQUIRE(std::string(rd.getMeta(), rd.getMetaSize()) == sdp);
    }

    SECTION("Per-frame user buffer round-trip and survives next()")
    {
        mmb::memvid vid;
        REQUIRE(vid.open("/mv_frameuser", true, 16, 8, mmb::video_format::gray8, 30, 3,
                         0, 0, /*frameextra=*/32));
        REQUIRE(vid.getFrameExtra() >= 32);

        std::string tag = "roi:1,2,3,4";
        REQUIRE(vid.setUserData(0, tag.data(), (int64_t)tag.size()));
        REQUIRE(vid.getUserLen(0) == (int64_t)tag.size());
        REQUIRE(std::string(vid.getUserData(0), vid.getUserLen(0)) == tag);

        // Oversized write rejected.
        std::string big(64, 'x');
        REQUIRE_FALSE(vid.setUserData(0, big.data(), (int64_t)big.size()));

        vid.next(1);  // stamps seq only; user buffer must be untouched
        REQUIRE(vid.getUserLen(0) == (int64_t)tag.size());
        REQUIRE(std::string(vid.getUserData(0), vid.getUserLen(0)) == tag);

        // A corrupted per-frame length must be clamped to the buffer stride so a
        // caller reading getUserData()+getUserLen() cannot be driven out of bounds.
        mmb::memmap raw;
        REQUIRE(raw.open("/mv_frameuser", 0, false, false));
        int64_t dataoff = *(int64_t*)(raw.data() + mmb::memvid::hv_dataoffset);
        int64_t blocksz = *(int64_t*)(raw.data() + mmb::memvid::hv_blocksz);
        *(int64_t*)(raw.data() + dataoff + 0 * blocksz + mmb::memvid::fv_userlen) = int64_t(1) << 40;
        REQUIRE(vid.getUserLen(0) <= vid.getFrameExtra());
    }

    SECTION("memaud carries identity and per-frame user data too")
    {
        mmb::memaud aud;
        REQUIRE(aud.open("/ma_user", true, 2, mmb::audio_format::s16le, 48000, 100, 4,
                         0, /*frameextra=*/16, 0x20746d70u));
        REQUIRE(aud.getFourcc() == 0x20746d70u);
        std::string t = "seq7";
        REQUIRE(aud.setUserData(1, t.data(), (int64_t)t.size()));
        REQUIRE(std::string(aud.getUserData(1), aud.getUserLen(1)) == t);
    }

    SECTION("Cross-type guard: a share of one class cannot be opened as another")
    {
        mmb::memvid vid;
        REQUIRE(vid.open("/xtype_vid", true, 16, 8, mmb::video_format::rgb24, 30, 2));
        mmb::memaud aud;
        REQUIRE_FALSE(aud.open_existing("/xtype_vid"));   // memvid share, not memaud
        mmb::mempkt pk;
        REQUIRE_FALSE(pk.open_existing("/xtype_vid"));    // memvid share, not mempkt

        mmb::memaud acreate;
        REQUIRE(acreate.open("/xtype_aud", true, 2, mmb::audio_format::s16le, 48000, 100, 4));
        mmb::memvid vbad;
        REQUIRE_FALSE(vbad.open_existing("/xtype_aud"));  // memaud share, not memvid

        mmb::mempkt pcreate;
        REQUIRE(pcreate.open("/xtype_pkt", true, 8, 65536, 4096));
        mmb::memvid vbad2;
        REQUIRE_FALSE(vbad2.open_existing("/xtype_pkt")); // mempkt share, not memvid
    }
}
