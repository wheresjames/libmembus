
#include "libmembus-internal.h"

#include <limits>

namespace LIBMEMBUS_NS
{

/// Main header
enum HeaderVal
{
    hv_size         = 0,
    hv_write        = hv_size   + sizeof(int64_t),
    hv_seq          = hv_write  + sizeof(int64_t),   // monotonic write-sequence counter
    hv_id           = hv_seq    + sizeof(int64_t),   // random session ID
    hv_mutex        = hv_id     + sizeof(int64_t),
    hv_cond         = hv_mutex  + sizeof(interprocess_mutex),
    hv_last         = hv_cond   + sizeof(interprocess_condition)
};

/// Frame header: [length][sequence][payload...]
enum FrameHeaderVal
{
    fv_size         = 0,
    fv_seq          = fv_size + sizeof(int64_t),
    fv_last         = fv_seq  + sizeof(int64_t)
};

/// Minimum buffer overhead
const int64_t c_minOverhead = hv_last + (2 * fv_last);

namespace
{
    bool checkedBackingSize(int64_t size, int64_t &backingSize)
    {
        if (size <= 0 || size > std::numeric_limits<int64_t>::max() - c_minOverhead)
            return false;
        backingSize = size + c_minOverhead;
        return true;
    }
}

void memmsg::close()
{
    // char *p = m_mem.data();
    // if (p)
    // {   interprocess_mutex *pMutex = (interprocess_mutex*)(p + hv_mutex);
    //     interprocess_condition *pCond = (interprocess_condition*)(p + hv_cond);
    //     pMutex->~interprocess_mutex();
    //     pCond->~interprocess_condition();
    // }

    m_mem.close();
    m_bWrite = false;
    m_nRead = 0;
    m_nLastSeq = -1;
}

bool memmsg::open(const std::string &sName, int64_t size, bool bWrite, bool bCreate)
{
    set_last_error(errc::ok);
    close();

    int64_t backingSize = 0;
    if (!checkedBackingSize(size, backingSize))
    {
        set_last_error(errc::invalid_argument);
        return false;
    }

    // Try to open the memory share
    if (!m_mem.open(sName, backingSize, bCreate, false))
    {
        close();
        set_last_error(errc::open_failed);
        return false;
    }

    m_bWrite = bWrite;

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

    // Buffer vars
    int64_t *pSize  = (int64_t*)(p + hv_size);
    int64_t *pWrite = (int64_t*)(p + hv_write);
    int64_t *pSeq   = (int64_t*)(p + hv_seq);
    interprocess_mutex     *pMutex = (interprocess_mutex*)(p + hv_mutex);
    interprocess_condition *pCond  = (interprocess_condition*)(p + hv_cond);

    // Initialize only when we created the share, not when attaching to an existing one
    if (!m_mem.existing())
    {
        *pSize  = size;
        *pWrite = 0;
        *pSeq   = 0;
        *(int64_t*)(p + hv_id) = (int64_t)std::mt19937_64(std::random_device{}())();
        new (pMutex) interprocess_mutex();
        new (pCond) interprocess_condition();
    }
    else if (*pSize != size)
    {   std::cout << "Size mismatch : " << *pSize << " != " << size << std::endl;
        close();
        set_last_error(errc::size_mismatch);
        return false;
    }
    // else
    //     m_nRead = *pWrite;

    set_last_error(errc::ok);
    return true;
}

bool memmsg::write(const std::string &sMsg)
{
    if (!m_bWrite)
    {   std::cout << "No write access" << std::endl;
        set_last_error(errc::access_denied);
        return false;
    }

    char *p = m_mem.data();
    if (!p)
    {   std::cout << "Invalid buffer" << std::endl;
        set_last_error(errc::not_open);
        return false;
    }

    // Buf params
    int64_t *pSize  = (int64_t*)(p + hv_size);
    int64_t *pWrite = (int64_t*)(p + hv_write);
    int64_t *pSeq   = (int64_t*)(p + hv_seq);
    interprocess_mutex     *pMutex = (interprocess_mutex*)(p + hv_mutex);
    interprocess_condition *pCond  = (interprocess_condition*)(p + hv_cond);
    char *pBuf = (p + hv_last);

    // Invalid size or too big for the buffer?
    int64_t len = sMsg.length();
    if (0 >= len || len >= *pSize)
    {   std::cout << "Invalid length : " << len << " : max : " << *pSize << std::endl;
        set_last_error(len <= 0 ? errc::invalid_argument : errc::message_too_large);
        return false;
    }

    {
        // Timed lock: if the writer crashed holding the mutex the readers must not
        // block forever.  Five seconds is generous for any legitimate write call.
        scoped_lock<interprocess_mutex> lk(*pMutex,
            boost::get_system_time() + boost::posix_time::milliseconds(5000));
        if (!lk)
        {   std::cout << "memmsg::write failed to acquire lock" << std::endl;
            set_last_error(errc::lock_timeout);
            return false;
        }

        // Do we need to wrap the buffer?
        if (0 > *pWrite || (fv_last + len) >= (*pSize - *pWrite))
        {
            // Loop if the write pointer is reasonable
            if (0 < *pWrite && *pWrite < *pSize)
                *(int64_t*)(pBuf + *pWrite) = 0;

            // Loop the write pointer
            *pWrite = 0;
        }

        // It fits here! Write length, then sequence number, then payload.
        int64_t seq = ++(*pSeq);
        *(int64_t*)(pBuf + *pWrite + fv_size) = len;
        *(int64_t*)(pBuf + *pWrite + fv_seq)  = seq;
        memcpy(pBuf + *pWrite + fv_last, sMsg.c_str(), len);

        // Increment write pointer
        *pWrite += fv_last + len;

        // Initialize next slot to zero
        *(int64_t*)(pBuf + *pWrite) = 0;

        // Notify under the lock to prevent lost wakeups
        pCond->notify_all();
    }

    set_last_error(errc::ok);
    return true;
}

std::string memmsg::read(uint64_t wait, bool *pOverrun)
{
    if (pOverrun) *pOverrun = false;

    char *p = m_mem.data();
    if (!p)
    {   std::cout << "Invalid buffer" << std::endl;
        set_last_error(errc::not_open);
        return std::string();
    }

    // Buf params
    int64_t *pSize  = (int64_t*)(p + hv_size);
    int64_t *pWrite = (int64_t*)(p + hv_write);
    int64_t *pSeq   = (int64_t*)(p + hv_seq);
    char    *pBuf   = (p + hv_last);
    interprocess_mutex     *pMutex = (interprocess_mutex*)(p + hv_mutex);
    interprocess_condition *pCond  = (interprocess_condition*)(p + hv_cond);

    // Hold the lock for the entire read so the writer cannot wrap the buffer
    // underneath us while we are inspecting or copying data.  The timed variant
    // prevents blocking forever if the writer crashed while holding the lock.
    scoped_lock<interprocess_mutex> lk(*pMutex,
        boost::get_system_time() + boost::posix_time::milliseconds(5000));
    if (!lk)
    {   std::cout << "memmsg::read failed to acquire lock" << std::endl;
        set_last_error(errc::lock_timeout);
        return std::string();
    }

    // Validate read pointer
    if (0 > m_nRead || *pSize <= m_nRead)
    {   m_nRead = 0; m_nLastSeq = -1; }

    // Wait for data if the buffer is empty
    if (m_nRead == *pWrite)
    {
        if (0 >= wait)
        {
            set_last_error(errc::timeout);
            return std::string();
        }

        if (!pCond->timed_wait(lk, boost::get_system_time() + boost::posix_time::milliseconds(wait)))
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

    // Validate length; check len before adding m_nRead to avoid integer overflow
    int64_t len = *(int64_t*)(pBuf + m_nRead + fv_size);
    if (0 >= len || len > *pSize - m_nRead)
    {
        // Zero length is the wrap sentinel — jump to the start of the buffer
        m_nRead = 0;

        if (m_nRead == *pWrite)
            return std::string();

        len = *(int64_t*)(pBuf + m_nRead + fv_size);
        if (0 >= len || len > *pSize - m_nRead)
        {
            std::cout << "Invalid length : " << m_nRead << " : " << len << " : " << *pSize << " : " << *pWrite << std::endl;
            set_last_error(errc::invalid_layout);
            return std::string();
        }
    }

    // Detect overrun: a sequence gap means the writer lapped this reader.
    int64_t frame_seq = *(int64_t*)(pBuf + m_nRead + fv_seq);
    if (m_nLastSeq >= 0 && frame_seq != m_nLastSeq + 1)
    {
        // Resync to the current write position so the next call reads a fresh message
        m_nRead    = *pWrite;
        m_nLastSeq = *pSeq;
        if (pOverrun) *pOverrun = true;
        set_last_error(errc::overrun);
        return std::string();
    }

    std::string val(pBuf + m_nRead + fv_last, len);
    m_nRead    += fv_last + len;
    m_nLastSeq  = frame_seq;

    set_last_error(errc::ok);
    return val;
}

int64_t memmsg::getSessionId()
{
    char *p = m_mem.data();
    if (!p)
        return 0;
    return *(int64_t*)(p + hv_id);
}

}; // end namespace
