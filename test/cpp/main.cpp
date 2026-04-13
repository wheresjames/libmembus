
#include <atomic>
#include <iostream>
#include <iomanip>
#include <cstring>

#include "libmembus.h"


//-------------------------------------------------------------------

template<typename T>
    T filename(const T &path, const T &seps = "/\\")
{   typename T::size_type pos = path.find_last_of(seps);
    if (T::npos == pos)
        return path;
    return T(path, pos + 1);
}

#ifdef WIN32
#   define PASSSTR "[PASS] "
#   define ERRSTR  "[ERROR] "
#   define DBGSTR  "[DEBUG] "
#else
#   define PASSSTR "[\033[1;32mPASS\033[1;0m] "
#   define ERRSTR  "[\033[1;31mERROR\033[1;0m] "
#   define DBGSTR  "[\033[92mDEBUG\033[0m] "
#endif

// Log debug message
#define debugLog(s) std::cout << DBGSTR << filename<std::string>(__FILE__) << "(" << __LINE__ << "): " << s << std::endl;

// Assert macros
#define assertTrue(s) { if (!(s)) { std::cout << (ERRSTR #s) << std::endl; return -1; } else std::cout << (PASSSTR #s) << std::endl; }
#define assertFalse(s) { if ((s)) { std::cout << (ERRSTR #s) << std::endl; return -1; } else std::cout << (PASSSTR #s) << std::endl; }
#define assertZero(s) { if ((s)) { std::cout << (ERRSTR #s) << std::endl; return -1; } else std::cout << (PASSSTR #s) << std::endl; }
#define assertNotZero(s) { if (!(s)) { std::cout << (ERRSTR #s) << std::endl; return -1; } else std::cout << (PASSSTR #s) << std::endl; }

// Quiet
#define assertTrueQ(s) { if (!(s)) { std::cout << (ERRSTR #s) << std::endl; return -1; } }
#define assertFalseQ(s) { if ((s)) { std::cout << (ERRSTR #s) << std::endl; return -1; } }


//-------------------------------------------------------------------
int Test_MemMap()
{
    // Basic create and attach
    {
        mmb::memmap creator, attacher;

        assertTrue(creator.open("/test_memmap", 256, true, true));
        assertFalse(creator.existing());
        assertTrue(creator.isOpen());
        assertTrue(creator.size() == 256);
        assertTrue(creator.name() == "/test_memmap");
        assertTrue(creator.data() != nullptr);

        assertTrue(attacher.open("/test_memmap", 256, false, false));
        assertTrue(attacher.existing());
        assertTrue(attacher.isOpen());
    }

    // Write and read back via two handles
    {
        mmb::memmap writer, reader;
        assertTrue(writer.open("/test_memmap_rw", 128, true, true));
        assertTrue(reader.open("/test_memmap_rw", 128, false, false));

        std::string msg = "Hello shared memory!";
        assertTrue(writer.write(msg) == (int64_t)msg.size());

        std::string back = reader.read(msg.size());
        assertTrue(back == msg);
    }

    // Close clears state
    {
        mmb::memmap m;
        assertTrue(m.open("/test_memmap_close", 64, true, true));
        assertTrue(m.isOpen());
        m.close();
        assertFalse(m.isOpen());
        assertTrue(m.size() == 0);
        assertTrue(m.name().empty());
    }

    // Attach while creator is alive; bNew forces recreation
    {
        // Creator must stay alive so the share persists for the attacher
        mmb::memmap creator;
        assertTrue(creator.open("/test_memmap_new", 64, true, true));
        assertFalse(creator.existing());

        // Attacher opens while creator is alive
        {
            mmb::memmap second;
            assertTrue(second.open("/test_memmap_new", 64, false, false));
            assertTrue(second.existing());
        }

        // bNew=true on same name while creator is alive: forcefully recreates
        mmb::memmap fresh;
        assertTrue(fresh.open("/test_memmap_new", 64, true, true));
        assertFalse(fresh.existing());
    }

    // Read with no size limit
    {
        mmb::memmap w, r;
        assertTrue(w.open("/test_memmap_full", 16, true, true));
        assertTrue(r.open("/test_memmap_full", 16, false, false));
        std::string payload(16, 'X');
        assertTrue(w.write(payload) == 16);
        std::string back = r.read();
        assertTrue(back.size() == 16);
    }

    return 0;
}


//-------------------------------------------------------------------
int Test_MessageQueue()
{
    mmb::memmsg tx, rx;
    std::string msg = "Message";

    tx.open("/mymsg", 1024, true, true);
    rx.open("/mymsg", 1024, false, false);

    tx.write(msg);
    std::string rmsg = rx.read(0);
    assertTrue(rmsg == msg)


    //---------------------------------------------------------------
    // Test message queue
    for(int b = 0; b < 128; b++)
    {
        mmb::memmsg tx, rx;

        assertTrueQ(tx.open("/mymsg", 64 + b, true, true));
        assertFalseQ(tx.existing());
        assertTrueQ(rx.open("/mymsg", 64 + b, false, false));
        assertTrueQ(rx.existing());

        for(int i = 0; i < 1000; i++)
        {
            std::string msg; msg += "Message "; msg += std::to_string(i);
            // debugLog(msg);
            assertTrueQ(tx.write(msg));
            std::string rmsg = rx.read(0);
            assertTrueQ(rmsg == msg)
        }
    }

    return 0;
}


//-------------------------------------------------------------------
int Test_MessageQueue_EdgeCases()
{
    // Attempt to read with timeout on empty queue
    {
        mmb::memmsg tx, rx;
        assertTrue(tx.open("/mymsg_ec", 256, true, true));
        assertTrue(rx.open("/mymsg_ec", 256, false, false));

        // No messages written — read with 0ms wait must return empty
        std::string empty = rx.read(0);
        assertTrue(empty.empty());
    }

    // Size mismatch: reader opening with wrong size must fail
    {
        mmb::memmsg tx, rx;
        assertTrue(tx.open("/mymsg_sizematch", 128, true, true));
        assertFalse(rx.open("/mymsg_sizematch", 64, false, false));
    }

    // Write on a read-only handle must fail
    {
        mmb::memmsg tx, rx;
        assertTrue(tx.open("/mymsg_readonly", 256, true, true));
        assertTrue(rx.open("/mymsg_readonly", 256, false, false));
        assertFalse(rx.write("should fail"));
    }

    // Empty string write must fail (length 0)
    {
        mmb::memmsg tx, rx;
        assertTrue(tx.open("/mymsg_empty_write", 256, true, true));
        assertTrue(rx.open("/mymsg_empty_write", 256, false, false));
        assertFalse(tx.write(""));
        assertTrue(rx.read(0).empty());
    }

    // Verify existing() flag
    {
        mmb::memmsg tx, rx;
        assertTrue(tx.open("/mymsg_existing", 256, true, true));
        assertFalse(tx.existing());
        assertTrue(rx.open("/mymsg_existing", 256, false, false));
        assertTrue(rx.existing());
    }

    // Multiple messages round-trip
    {
        mmb::memmsg tx, rx;
        assertTrue(tx.open("/mymsg_multi", 512, true, true));
        assertTrue(rx.open("/mymsg_multi", 512, false, false));

        const char *msgs[] = {"alpha", "beta", "gamma", "delta", "epsilon"};
        for (auto &m : msgs)
        {
            assertTrueQ(tx.write(m));
            std::string back = rx.read(0);
            assertTrueQ(back == m);
        }
    }

    return 0;
}


//-------------------------------------------------------------------
int Test_MemVid()
{
    // Invalid parameter rejection
    {
        mmb::memvid v;
        assertFalse(v.open("/memvid_bad", true,  0,  0, 24, 30, 3));  // zero w/h
        assertFalse(v.open("/memvid_bad", true, 64, 48,  8, 30, 3));  // bpp != 24
        assertFalse(v.open("/memvid_bad", true, 64, 48, 24,  0, 3));  // zero fps
        assertFalse(v.open("/memvid_bad", true, 64, 48, 24, 30, 0));  // zero bufs
    }

    // Create and verify metadata
    {
        mmb::memvid vid;
        assertTrue(vid.open("/memvid_meta", true, 64, 48, 24, 30, 4));
        assertFalse(vid.existing());
        assertTrue(vid.isOpen());
        assertTrue(vid.getWidth()  == 64);
        assertTrue(vid.getHeight() == 48);
        assertTrue(vid.getBpp()    == 24);
        assertTrue(vid.getFps()    == 30);
        assertTrue(vid.getBufs()   == 4);
    }

    // Attach to existing share and verify metadata matches
    {
        mmb::memvid creator, attacher;
        assertTrue(creator.open("/memvid_attach", true, 32, 24, 24, 25, 2));
        assertTrue(attacher.open("/memvid_attach", false, 32, 24, 24, 25, 2));
        assertTrue(attacher.existing());
        assertTrue(attacher.getWidth()  == 32);
        assertTrue(attacher.getHeight() == 24);
        assertTrue(attacher.getBufs()   == 2);
    }

    // Mismatched parameters on attach must fail
    {
        mmb::memvid creator, bad;
        assertTrue(creator.open("/memvid_mismatch", true, 32, 24, 24, 25, 2));
        assertFalse(bad.open("/memvid_mismatch", false, 64, 24, 24, 25, 2));
    }

    // fill() and getBuf() data round-trip
    {
        mmb::memvid vid;
        assertTrue(vid.open("/memvid_fill", true, 16, 8, 24, 30, 3));

        // Fill buffer 0 with 0xAB and verify via getBuf
        assertTrue(vid.fill(0, 0xAB));
        mmb::memvid::vidview view = vid.getBuf(0);
        assertTrue(view.m_w  == 16);
        assertTrue(view.m_h  == 8);
        assertTrue(view.m_ptr != nullptr);

        bool allMatch = true;
        for (int64_t i = 0; i < view.m_sw * view.m_h; i++)
            if ((unsigned char)view.m_ptr[i] != 0xAB) { allMatch = false; break; }
        assertTrue(allMatch);

        // Fill buffer 1 with 0x00 and verify buffer 0 is unchanged
        assertTrue(vid.fill(1, 0x00));
        mmb::memvid::vidview view1 = vid.getBuf(1);
        bool buf1Zero = true;
        for (int64_t i = 0; i < view1.m_sw * view1.m_h; i++)
            if ((unsigned char)view1.m_ptr[i] != 0x00) { buf1Zero = false; break; }
        assertTrue(buf1Zero);

        allMatch = true;
        for (int64_t i = 0; i < view.m_sw * view.m_h; i++)
            if ((unsigned char)view.m_ptr[i] != 0xAB) { allMatch = false; break; }
        assertTrue(allMatch);
    }

    // Pointer arithmetic: setPtr / getPtr / next
    {
        mmb::memvid vid;
        assertTrue(vid.open("/memvid_ptr", true, 16, 8, 24, 30, 4));

        assertTrue(vid.setPtr(0) == 0);
        assertTrue(vid.getPtr(0) == 0);
        assertTrue(vid.getPtr(1) == 1);
        assertTrue(vid.getPtr(3) == 3);

        // Wrap-around: ptr=3, offset=1 wraps to 0
        assertTrue(vid.setPtr(3) == 3);
        assertTrue(vid.getPtr(1) == 0);

        // next() increments the stored pointer
        assertTrue(vid.setPtr(0) == 0);
        assertTrue(vid.next(1)   == 1);
        assertTrue(vid.next(1)   == 2);
        assertTrue(vid.next(2)   == 0);  // wraps at 4
    }

    // getPtrErr: circular distance from ptr+bias to pos
    {
        mmb::memvid vid;
        assertTrue(vid.open("/memvid_ptrerr", true, 16, 8, 24, 30, 8));
        assertTrue(vid.setPtr(4) == 4);

        // pos == ptr+bias => error 0
        assertTrue(vid.getPtrErr(4, 0) == 0);
        assertTrue(vid.getPtrErr(5, 1) == 0);
    }

    // open_existing
    {
        mmb::memvid creator, ex;
        assertTrue(creator.open("/memvid_existing", true, 16, 8, 24, 30, 2));
        assertTrue(ex.open_existing("/memvid_existing"));
        assertTrue(ex.isOpen());
    }

    return 0;
}


//-------------------------------------------------------------------
int Test_MemAud()
{
    // Invalid parameter rejection
    {
        mmb::memaud a;
        assertFalse(a.open("/memaud_bad", true,  0, 16, 44100, 30, 3)); // zero channels
        assertFalse(a.open("/memaud_bad", true,  2, 12, 44100, 30, 3)); // bps not 8 or 16
        assertFalse(a.open("/memaud_bad", true,  2, 16,     0, 30, 3)); // zero bitrate
        assertFalse(a.open("/memaud_bad", true,  2, 16, 44100,  0, 3)); // zero fps
        assertFalse(a.open("/memaud_bad", true,  2, 16, 44100, 30, 0)); // zero bufs
    }

    // Create with 8-bit and verify metadata
    {
        mmb::memaud aud;
        assertTrue(aud.open("/memaud_8bit", true, 1, 8, 8000, 100, 4));
        assertFalse(aud.existing());
        assertTrue(aud.isOpen());
        assertTrue(aud.getChannels() == 1);
        assertTrue(aud.getBps()      == 8);
        assertTrue(aud.getBitRate()  == 8000);
        assertTrue(aud.getFps()      == 100);
        assertTrue(aud.getBufs()     == 4);
    }

    // Create with 16-bit stereo and verify metadata
    {
        mmb::memaud aud;
        assertTrue(aud.open("/memaud_16bit", true, 2, 16, 44100, 30, 3));
        assertTrue(aud.getChannels() == 2);
        assertTrue(aud.getBps()      == 16);
        assertTrue(aud.getBitRate()  == 44100);
        assertTrue(aud.getFps()      == 30);
        assertTrue(aud.getBufs()     == 3);
        assertTrue(aud.getBufSize()  > 0);
    }

    // Attach and verify parameters match
    {
        mmb::memaud creator, attacher;
        assertTrue(creator.open("/memaud_attach", true, 1, 16, 16000, 50, 2));
        assertTrue(attacher.open("/memaud_attach", false, 1, 16, 16000, 50, 2));
        assertTrue(attacher.existing());
        assertTrue(attacher.getChannels() == 1);
        assertTrue(attacher.getBufs()     == 2);
    }

    // Mismatched parameters on attach must fail
    {
        mmb::memaud creator, bad;
        assertTrue(creator.open("/memaud_mismatch", true, 1, 16, 16000, 50, 2));
        assertFalse(bad.open("/memaud_mismatch", false, 2, 16, 16000, 50, 2));
    }

    // fill() and getBuf() data round-trip
    {
        mmb::memaud aud;
        assertTrue(aud.open("/memaud_fill", true, 1, 8, 8000, 100, 3));

        assertTrue(aud.fill(0, 0xCC));
        mmb::memaud::audview view = aud.getBuf(0);
        assertTrue(view.m_ptr  != nullptr);
        assertTrue(view.m_size > 0);

        bool allMatch = true;
        for (int64_t i = 0; i < view.m_size; i++)
            if ((unsigned char)view.m_ptr[i] != 0xCC) { allMatch = false; break; }
        assertTrue(allMatch);

        // Fill buf 1 with 0x00, buf 0 must remain 0xCC
        assertTrue(aud.fill(1, 0x00));
        allMatch = true;
        for (int64_t i = 0; i < view.m_size; i++)
            if ((unsigned char)view.m_ptr[i] != 0xCC) { allMatch = false; break; }
        assertTrue(allMatch);
    }

    // Pointer arithmetic: setPtr / getPtr / next
    {
        mmb::memaud aud;
        assertTrue(aud.open("/memaud_ptr", true, 1, 8, 8000, 100, 4));

        assertTrue(aud.setPtr(0) == 0);
        assertTrue(aud.getPtr(0) == 0);
        assertTrue(aud.getPtr(1) == 1);

        // Wrap-around
        assertTrue(aud.setPtr(3) == 3);
        assertTrue(aud.getPtr(1) == 0);

        assertTrue(aud.setPtr(0) == 0);
        assertTrue(aud.next(1)   == 1);
        assertTrue(aud.next(1)   == 2);
        assertTrue(aud.next(2)   == 0);
    }

    // getPtrErr: circular distance
    {
        mmb::memaud aud;
        assertTrue(aud.open("/memaud_ptrerr", true, 1, 8, 8000, 100, 8));
        assertTrue(aud.setPtr(4) == 4);
        assertTrue(aud.getPtrErr(4, 0) == 0);
        assertTrue(aud.getPtrErr(5, 1) == 0);
    }

    // open_existing
    {
        mmb::memaud creator, ex;
        assertTrue(creator.open("/memaud_existing", true, 1, 8, 8000, 100, 2));
        assertTrue(ex.open_existing("/memaud_existing"));
        assertTrue(ex.isOpen());
    }

    return 0;
}


int main(int /*argc*/, char * /*argv*/[])
{
    int result = 0;

    // Install ctrl-c handler
    static volatile int ctrl_c_count = 0;
    mmb::install_ctrl_c_handler(&ctrl_c_count);

    std::cout << " --- TEST FOR " APPNAME "\n";
    std::cout << " --- VERSION " APPVER " [" APPBUILD "]\n";
    std::cout << " --- Starting Tests ---\n";

    // Check macros
    assertTrue(true);
    assertFalse(false);
    assertTrueQ(true);
    assertFalseQ(false);
    assertZero(0);
    assertNotZero(1);
    debugLog("Debug message");

    // Test shared memory map
    assertZero(Test_MemMap());

    // Test message queue
    assertZero(Test_MessageQueue());

    // Test message queue edge cases
    assertZero(Test_MessageQueue_EdgeCases());

    // Test video ring buffer
    assertZero(Test_MemVid());

    // Test audio ring buffer
    assertZero(Test_MemAud());

    std::cout << " --- Success ---\n";

    return 0;
}

