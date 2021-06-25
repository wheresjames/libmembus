
#include "libmembus/libmembus_internal.h"

namespace LIBMEMBUS_NS
{

memaud::audview memaud::getBuf(int64_t idx) noexcept(false)
{
    char *p = m_mem.data();
    if (!p)
        throw std::exception();

    int64_t *pSize = (int64_t*)(p + hv_size);
    int64_t *pPtr = (int64_t*)(p + hv_ptr);
    int64_t *pCh = (int64_t*)(p + hv_ch);
    int64_t *pBps = (int64_t*)(p + hv_bps);
    int64_t *pBufs = (int64_t*)(p + hv_bufs);
    int64_t *pBlockSz = (int64_t*)(p + hv_blocksz);

    // Loop index if needed
    if (0 > idx)
        idx += *pBufs;
    else if (*pBufs <= idx)
        idx -= *pBufs;

    // Ensure the index is valid
    if (0 > idx || *pBufs <= idx)
        throw std::exception();

    return memaud::audview(m_mem.data() + hv_last + (idx * *pBlockSz) + fv_last,
                           *pBlockSz - fv_last, *pCh, *pBps);
}

bool memaud::fill(int64_t idx, int col)
{
    char *p = m_mem.data();
    if (!p)
        return false;

    int64_t *pSize = (int64_t*)(p + hv_size);
    int64_t *pPtr = (int64_t*)(p + hv_ptr);
    int64_t *pCh = (int64_t*)(p + hv_ch);
    int64_t *pBps = (int64_t*)(p + hv_bps);
    int64_t *pBufs = (int64_t*)(p + hv_bufs);
    int64_t *pBlockSz = (int64_t*)(p + hv_blocksz);

    // Loop index if needed
    if (0 > idx)
        idx += *pBufs;
    else if (*pBufs <= idx)
        idx -= *pBufs;

    // Ensure the index is valid
    if (0 > idx || *pBufs <= idx)
        return false;

    memset(m_mem.data() + hv_last + (idx * *pBlockSz) + fv_last, col, *pBlockSz - fv_last);

    return true;
}


int64_t memaud::getBufs()
{
    char *p = m_mem.data();
    if (!p)
        return 0;

    // Pointer pointer
    return *(int64_t*)(p + hv_bufs);
}

int64_t memaud::getPtr(int64_t offset)
{
    char *p = m_mem.data();
    if (!p)
        return -1;

    // Pointer pointer
    int64_t *pPtr = (int64_t*)(p + hv_ptr);
    int64_t *pBufs = (int64_t*)(p + hv_bufs);

    // Pointer value
    int64_t ptr = *pPtr + offset;

    // Loop the pointer
    if (0 > ptr)
        ptr += *pBufs;
    else if (*pBufs <= ptr)
        ptr -= *pBufs;

    // Make sure the pointer is valid
    if (0 > ptr || *pBufs <= ptr)
        return -1;

    return ptr;
}

int64_t memaud::setPtr(int64_t ptr)
{
    if (0 > ptr)
        return -1;

    char *p = m_mem.data();
    if (!p)
        return -1;

    // Pointer pointer
    int64_t *pPtr = (int64_t*)(p + hv_ptr);
    int64_t *pBufs = (int64_t*)(p + hv_bufs);

    // Loop the pointer
    if (0 > ptr)
        ptr += *pBufs;
    else if (*pBufs <= ptr)
        ptr -= *pBufs;

    // Make sure the pointer is valid
    if (0 > ptr || *pBufs <= ptr)
        return -1;

    // Set the pointer
    *pPtr = ptr;

    return ptr;
}

int64_t memaud::next(int64_t inc)
{
    char *p = m_mem.data();
    if (!p)
        return -1;

    return setPtr(*(int64_t*)(p + hv_ptr) + inc);
}

int64_t memaud::getPtrErr(int64_t pos, int64_t bias)
{
    char *p = m_mem.data();
    if (!p)
        return 0;

    // Pointer pointer
    int64_t *pPtr = (int64_t*)(p + hv_ptr);
    int64_t *pBufs = (int64_t*)(p + hv_bufs);

    // Loop pointer
    int64_t ptr = *pPtr + bias;
    if (0 > ptr)
        ptr += *pBufs;
    else if (*pBufs <= ptr)
        ptr -= *pBufs;

    // Loop position
    if (0 > pos)
        pos += *pBufs;
    else if (*pBufs <= pos)
        pos -= *pBufs;

    // Calculate error
    int64_t err = 0;
    if (pos > ptr)
    {   err = pos - ptr;
        if (err > (*pBufs - err))
            err = -(*pBufs - err);
    }
    else
    {   err = ptr - pos;
        if (err > (*pBufs - err))
            err = (*pBufs - err);
        else
            err = -err;
    }

    return err;
}

int64_t memaud::getChannels()
{
    char *p = m_mem.data();
    if (!p)
        return 0;

    return *(int64_t*)(p + hv_ch);
}

int64_t memaud::getBps()
{
    char *p = m_mem.data();
    if (!p)
        return 0;

    return *(int64_t*)(p + hv_bps);
}

int64_t memaud::getBitRate()
{
    char *p = m_mem.data();
    if (!p)
        return 0;

    return *(int64_t*)(p + hv_bitrate);
}

int64_t memaud::getFps()
{
    char *p = m_mem.data();
    if (!p)
        return 0;

    return *(int64_t*)(p + hv_fps);
}

int64_t memaud::getBufSize()
{
    char *p = m_mem.data();
    if (!p)
        return 0;

    int64_t ch = *(int64_t*)(p + hv_ch);
    if (1 > ch)
        ch = 1;

    return (*(int64_t*)(p + hv_blocksz) - fv_last) / ch / 2;
}


void memaud::close()
{
    m_mem.close();
}

bool memaud::open_existing(const std::string &sName)
{
    close();

    // Attempt to open existing share
    if (!m_mem.open(sName, 0, false))
        return false;

    // Verify data pointer
    char *p = m_mem.data();
    if (!p)
    {
        close();
        return false;
    }

    return true;
}


bool memaud::open(const std::string &sName, bool bCreate, int64_t ch, int64_t bps, int64_t bitrate, int64_t fps, int64_t bufs)
{
    close();

    // Param check
    if (0 >= ch || (8 != bps && 16 != bps) || 0 >= bitrate || 0 >= fps || 0 >= bufs)
        return false;

    // Calculate sample size
    int64_t ss = fitTo<int64_t>(bps, 8);

    // Image block size
    int64_t blocksz = fv_last + ((int64_t)(bitrate / fps) * ss * ch);

    // How much total memory do we need?
    int64_t total = hv_last + (blocksz * bufs);

    // Try to open the memory share
    if (!m_mem.open(sName, total, bCreate, bCreate))
        return false;

    // Data pointer
    char *p = m_mem.data();
    if (!p)
    {
        close();
        return false;
    }

    // Headers
    int64_t *pSize = (int64_t*)(p + hv_size);
    int64_t *pPtr = (int64_t*)(p + hv_ptr);
    int64_t *pCh = (int64_t*)(p + hv_ch);
    int64_t *pBps = (int64_t*)(p + hv_bps);
    int64_t *pBitRate = (int64_t*)(p + hv_bitrate);
    int64_t *pFps = (int64_t*)(p + hv_fps);
    int64_t *pBufs = (int64_t*)(p + hv_bufs);
    int64_t *pBlockSz = (int64_t*)(p + hv_blocksz);

    // Initialize if not existing
    if (!m_mem.existing())
    {
        *pSize = total;
        *pPtr = 0;
        *pCh = ch;
        *pBps = bps;
        *pBitRate = bitrate;
        *pFps = fps;
        *pBufs = bufs;
        *pBlockSz = blocksz;
    }

    // Exists, so makes sure it matches what we expect
    else if (*pSize != total
             || *pCh != ch
             || *pBps != bps
             || *pBitRate != bitrate
             || *pFps != fps
             || *pBufs != bufs
             || *pBlockSz != blocksz)
    {
        close();
        return false;
    }

    return true;
}

}; // end namespace
