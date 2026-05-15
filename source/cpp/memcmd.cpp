
#include "libmembus-internal.h"

#include <limits>

namespace LIBMEMBUS_NS
{

// Implementation enums are in an anonymous namespace to avoid ODR conflicts
// with the identically-named enums in memmsg.cpp.
namespace
{
    enum HeaderVal
    {
        hv_size     = 0,
        hv_write    = hv_size    + sizeof(int64_t),
        hv_seq      = hv_write   + sizeof(int64_t),
        hv_id       = hv_seq     + sizeof(int64_t),
        hv_readers  = hv_id      + sizeof(int64_t),  // active reader count
        hv_mutex    = hv_readers + sizeof(int64_t),
        hv_cond     = hv_mutex   + sizeof(interprocess_mutex),
        hv_last     = hv_cond    + sizeof(interprocess_condition)
    };

    // Frame layout: [payload_len : int64][seq : int64][payload bytes...]
    enum FrameHeaderVal
    {
        fv_size = 0,
        fv_seq  = fv_size + sizeof(int64_t),
        fv_last = fv_seq  + sizeof(int64_t)
    };

    // Extra bytes allocated beyond the caller-requested size so sentinel
    // and next-slot writes never go out of bounds (includes 7-byte alignment pad each).
    const int64_t c_minOverhead = hv_last + (2 * (fv_last + 7));

    bool checkedBackingSize(int64_t size, int64_t &backingSize)
    {
        if (size <= 0 || size > std::numeric_limits<int64_t>::max() - c_minOverhead)
            return false;
        backingSize = size + c_minOverhead;
        return true;
    }

    inline int64_t frameStride(int64_t len)
    {
        return (fv_last + len + 7) & ~int64_t(7);
    }
}


void memcmd::close()
{
    char *p = m_mem.data();
    if (p && m_bReader)
        std::atomic_ref<int64_t>(*(int64_t*)(p + hv_readers))
            .fetch_sub(1, std::memory_order_relaxed);

    m_mem.close();
    m_bReader  = false;
    m_nRead    = 0;
    m_nLastSeq = -1;
}

bool memcmd::open(const std::string &sName, int64_t size, bool bReader, bool bCreate)
{
    set_last_error(errc::ok);
    close();

    int64_t backingSize = 0;
    if (!checkedBackingSize(size, backingSize))
    {
        set_last_error(errc::invalid_argument);
        return false;
    }

    if (!m_mem.open(sName, backingSize, bCreate, false))
    {
        close();
        set_last_error(errc::open_failed);
        return false;
    }

    char *p = m_mem.data();
    if (!p)
    {
        close();
        set_last_error(errc::not_open);
        return false;
    }

    if (m_mem.size() < backingSize)
    {
        close();
        set_last_error(errc::invalid_layout);
        return false;
    }

    int64_t                *pSize  = (int64_t*)(p + hv_size);
    interprocess_mutex     *pMutex = (interprocess_mutex*)(p + hv_mutex);
    interprocess_condition *pCond  = (interprocess_condition*)(p + hv_cond);

    if (!m_mem.existing())
    {
        *pSize                          = size;
        *(int64_t*)(p + hv_write)       = 0;
        *(int64_t*)(p + hv_seq)         = 0;
        *(int64_t*)(p + hv_id)          = (int64_t)std::mt19937_64(std::random_device{}())();
        *(int64_t*)(p + hv_readers)     = 0;
        new (pMutex) interprocess_mutex();
        new (pCond)  interprocess_condition();
    }
    else if (*pSize != size)
    {
        close();
        set_last_error(errc::size_mismatch);
        return false;
    }

    if (bReader)
    {
        std::atomic_ref<int64_t>(*(int64_t*)(p + hv_readers))
            .fetch_add(1, std::memory_order_relaxed);
        m_bReader = true;
    }

    set_last_error(errc::ok);
    return true;
}

bool memcmd::write(const std::string &sMsg)
{
    char *p = m_mem.data();
    if (!p)
    {
        set_last_error(errc::not_open);
        return false;
    }

    int64_t                *pSize  = (int64_t*)(p + hv_size);
    int64_t                *pWrite = (int64_t*)(p + hv_write);
    int64_t                *pSeq   = (int64_t*)(p + hv_seq);
    interprocess_mutex     *pMutex = (interprocess_mutex*)(p + hv_mutex);
    interprocess_condition *pCond  = (interprocess_condition*)(p + hv_cond);
    char                   *pBuf   = p + hv_last;

    int64_t len = (int64_t)sMsg.length();
    if (0 >= len || len >= *pSize)
    {
        set_last_error(len <= 0 ? errc::invalid_argument : errc::message_too_large);
        return false;
    }

    {
        scoped_lock<interprocess_mutex> lk(*pMutex,
            boost::get_system_time() + boost::posix_time::milliseconds(5000));
        if (!lk)
        {
            set_last_error(errc::lock_timeout);
            return false;
        }

        int64_t stride = frameStride(len);

        // Wrap the ring buffer if the message won't fit at the current position
        if (0 > *pWrite || stride > (*pSize - *pWrite))
        {
            if (0 < *pWrite && *pWrite < *pSize)
                *(int64_t*)(pBuf + *pWrite) = 0;  // sentinel at wrap point
            *pWrite = 0;
        }

        int64_t seq = ++(*pSeq);
        *(int64_t*)(pBuf + *pWrite + fv_size) = len;
        *(int64_t*)(pBuf + *pWrite + fv_seq)  = seq;
        memcpy(pBuf + *pWrite + fv_last, sMsg.c_str(), len);
        *pWrite += stride;
        *(int64_t*)(pBuf + *pWrite) = 0;  // pre-zero next slot's length field

        pCond->notify_all();
    }

    set_last_error(errc::ok);
    return true;
}

std::string memcmd::read(uint64_t wait_ms, bool *pOverrun)
{
    if (pOverrun) *pOverrun = false;

    char *p = m_mem.data();
    if (!p)
    {
        set_last_error(errc::not_open);
        return std::string();
    }

    int64_t                *pSize  = (int64_t*)(p + hv_size);
    int64_t                *pWrite = (int64_t*)(p + hv_write);
    int64_t                *pSeq   = (int64_t*)(p + hv_seq);
    char                   *pBuf   = p + hv_last;
    interprocess_mutex     *pMutex = (interprocess_mutex*)(p + hv_mutex);
    interprocess_condition *pCond  = (interprocess_condition*)(p + hv_cond);

    scoped_lock<interprocess_mutex> lk(*pMutex,
        boost::get_system_time() + boost::posix_time::milliseconds(5000));
    if (!lk)
    {
        set_last_error(errc::lock_timeout);
        return std::string();
    }

    // Clamp a corrupt or uninitialised read pointer
    if (0 > m_nRead || *pSize <= m_nRead)
    {
        m_nRead    = 0;
        m_nLastSeq = -1;
    }

    // Block until data arrives or the timeout expires
    if (m_nRead == *pWrite)
    {
        if (0 >= wait_ms)
        {
            set_last_error(errc::timeout);
            return std::string();
        }
        if (!pCond->timed_wait(lk,
                boost::get_system_time() + boost::posix_time::milliseconds(wait_ms)))
        {
            set_last_error(errc::timeout);
            return std::string();
        }
        if (m_nRead == *pWrite)
        {
            set_last_error(errc::timeout);
            return std::string();
        }
    }

    // Read the frame length; zero is the wrap sentinel — jump to start of buffer
    int64_t len = *(int64_t*)(pBuf + m_nRead + fv_size);
    if (0 >= len || frameStride(len) > *pSize - m_nRead)
    {
        m_nRead = 0;
        if (m_nRead == *pWrite)
            return std::string();
        len = *(int64_t*)(pBuf + m_nRead + fv_size);
        if (0 >= len || frameStride(len) > *pSize - m_nRead)
        {
            set_last_error(errc::invalid_layout);
            return std::string();
        }
    }

    // Detect overrun: a gap in sequence numbers means a writer lapped this reader
    int64_t frame_seq = *(int64_t*)(pBuf + m_nRead + fv_seq);
    if (m_nLastSeq >= 0 && frame_seq != m_nLastSeq + 1)
    {
        m_nRead    = *pWrite;
        m_nLastSeq = *pSeq;
        if (pOverrun) *pOverrun = true;
        set_last_error(errc::overrun);
        return std::string();
    }

    std::string val(pBuf + m_nRead + fv_last, len);
    m_nRead    += frameStride(len);
    m_nLastSeq  = frame_seq;

    set_last_error(errc::ok);
    return val;
}

int64_t memcmd::readerCount()
{
    char *p = m_mem.data();
    if (!p)
        return 0;
    return std::atomic_ref<int64_t>(*(int64_t*)(p + hv_readers))
               .load(std::memory_order_relaxed);
}

int64_t memcmd::getSessionId()
{
    char *p = m_mem.data();
    if (!p)
        return 0;
    return *(int64_t*)(p + hv_id);
}

}; // end namespace
