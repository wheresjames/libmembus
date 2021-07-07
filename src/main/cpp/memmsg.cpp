
#include "libmembus_internal.h"

namespace LIBMEMBUS_NS
{

/// Main header
enum HeaderVal
{
    hv_size         = 0,
    hv_write        = hv_size   + sizeof(int64_t),
    hv_mutex        = hv_write  + sizeof(int64_t),
    hv_cond         = hv_mutex  + sizeof(interprocess_mutex),
    hv_last         = hv_cond   + sizeof(interprocess_condition)
};

/// Frame header
enum FrameHeaderVal
{
    fv_size         = 0,
    fv_last         = fv_size + sizeof(int64_t)
};

/// Minimum buffer overhead
const int64_t c_minOverhead = hv_last + (2 * fv_last);

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
}

bool memmsg::open(const std::string &sName, int64_t size, bool bWrite, bool bCreate)
{
    close();

    // Check params
    if (0 >= size)
        return false;

    // Try to open the memory share
    if (!m_mem.open(sName, size + c_minOverhead, bCreate, bCreate))
    {
        close();
        return false;
    }

    m_bWrite = bWrite;

    char *p = m_mem.data();
    if (!p)
    {
        close();
        return false;
    }

    // Buffer vars
    int64_t *pSize = (int64_t*)(p + hv_size);
    int64_t *pWrite = (int64_t*)(p + hv_write);
    interprocess_mutex *pMutex = (interprocess_mutex*)(p + hv_mutex);
    interprocess_condition *pCond = (interprocess_condition*)(p + hv_cond);

    // Initialize if we're the first
    if (bCreate)
    {
        *pSize = size;
        *pWrite = 0;
        new (pMutex) interprocess_mutex();
        new (pCond) interprocess_condition();
    }
    else if (*pSize != size)
    {   std::cout << "Size mismatch : " << *pSize << " != " << size << std::endl;
        close();
        return false;
    }
    // else
    //     m_nRead = *pWrite;

    return true;
}

bool memmsg::write(const std::string &sMsg)
{
    if (!m_bWrite)
    {   std::cout << "No write access" << std::endl;
        return false;
    }

    char *p = m_mem.data();
    if (!p)
    {   std::cout << "Invalid buffer" << std::endl;
        false;
    }

    // Buf params
    int64_t *pSize = (int64_t*)(p + hv_size);
    int64_t *pWrite = (int64_t*)(p + hv_write);
    interprocess_mutex *pMutex = (interprocess_mutex*)(p + hv_mutex);
    interprocess_condition *pCond = (interprocess_condition*)(p + hv_cond);
    char *pBuf = (p + hv_last);

    // Invalid size or too big for the buffer?
    int64_t len = sMsg.length();
    if (0 >= len || len >= *pSize)
    {   std::cout << "Invalid length : " << len << " : max : " << *pSize << std::endl;
        return false;
    }

    {
        // Lock the lock
        scoped_lock<interprocess_mutex> lk(*pMutex);

        // Do we need to wrap the buffer?
        if (0 > *pWrite || (fv_last + len) >= (*pSize - *pWrite))
        {
            // Loop if the write pointer is reasonable
            if (0 < *pWrite && *pWrite < *pSize)
                *(int64_t*)(pBuf + *pWrite) = 0;

            // Loop the write pointer
            *pWrite = 0;
        }

        // It fits here!
        *(int64_t*)(pBuf + *pWrite + fv_size) = len;
        memcpy(pBuf + *pWrite + fv_last, sMsg.c_str(), len);

        // Increment write pointer
        *pWrite += fv_last + len;

        // Initialize next slot to zero
        *(int64_t*)(pBuf + *pWrite) = 0;
    }

    // Notify waiting threads
    pCond->notify_all();

    return true;
}

std::string memmsg::read(uint64_t wait)
{
    char *p = m_mem.data();
    if (!p)
    {   std::cout << "Invalid buffer" << std::endl;
        return std::string();
    }

    // Buf params
    int64_t *pSize = (int64_t*)(p + hv_size);
    int64_t *pWrite = (int64_t*)(p + hv_write);
    char *pBuf = (p + hv_last);

    // Validate read pointer
    if (0 > m_nRead || *pSize <= m_nRead)
    {   std::cout << "Invalid read pointer " << m_nRead << " : " << *pSize << std::endl;
        m_nRead = 0;
    }

    // Is there any unread data?
    if (m_nRead == *pWrite)
    {
        if (0 >= wait)
            return std::string();

        // Get sync objects
        interprocess_mutex *pMutex = (interprocess_mutex*)(p + hv_mutex);
        interprocess_condition *pCond = (interprocess_condition*)(p + hv_cond);
        scoped_lock<interprocess_mutex> lk(*pMutex);

        if (m_nRead == *pWrite)
            if (!pCond->timed_wait(lk, boost::get_system_time() + boost::posix_time::milliseconds(wait)))
                return std::string();
    }

    // Validate length
    int64_t len = *(int64_t*)(pBuf + m_nRead);
    if (0 >= len || (m_nRead + len) > *pSize)
    {
        // std::cout << "Looping read pointer : " << m_nRead << " : " << len << " : " << *pSize << " : " << *pWrite << std::endl;
        // fflush(stdout);

        m_nRead = 0;

        // Anything after looping?
        if (m_nRead == *pWrite)
            return std::string();

        // Read new length
        len = *(int64_t*)(pBuf + m_nRead);
        if (0 >= len || (m_nRead + len) > *pSize)
        {
            std::cout << "Invalid length : " << m_nRead << " : " << len << " : " << *pSize << " : " << *pWrite << std::endl;
            return std::string();
        }
    }

    // Data
    std::string val(pBuf + m_nRead + fv_last, len);

    // Next data pointer
    m_nRead += fv_last + len;

    return val;
}

}; // end namespace
