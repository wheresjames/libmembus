
#include <catch2/catch_test_macros.hpp>
#include "libmembus.h"


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

    SECTION("setAll does not increment epoch when no slots are written")
    {
        // An empty map or an all-unknown map must not cause spurious epoch
        // increments, which would generate spurious wakeups in waitForChange().
        mmb::memkv kv;
        REQUIRE(kv.create("/memkv_setall_noop", 2, 15, 31));
        REQUIRE(kv.setName(0, "a"));
        REQUIRE(kv.setName(1, "b"));
        REQUIRE(kv.setAll({{"a", "1"}, {"b", "2"}}));

        int64_t e0 = kv.getEpoch();

        REQUIRE(kv.setAll({}));
        REQUIRE(kv.getEpoch() == e0);   // empty map — epoch unchanged

        REQUIRE(kv.setAll({{"unknown", "x"}, {"also_unknown", "y"}}));
        REQUIRE(kv.getEpoch() == e0);   // all names absent — epoch unchanged

        REQUIRE(kv.setAll({{"a", "new"}}));
        REQUIRE(kv.getEpoch() == e0 + 1);  // one real write — epoch incremented once
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

    SECTION("open rejects headers whose schema exceeds the mapped size")
    {
        mmb::memmap raw;
        REQUIRE(raw.open("/memkv_badhdr", 64, true, true));
        char *p = raw.data();
        REQUIRE(p != nullptr);

        ((int64_t*)p)[0] = 2;      // count
        ((int64_t*)p)[1] = 1024;   // maxNameLen
        ((int64_t*)p)[2] = 1024;   // maxValueLen

        mmb::memkv kv;
        REQUIRE_FALSE(kv.open("/memkv_badhdr"));
    }
}
