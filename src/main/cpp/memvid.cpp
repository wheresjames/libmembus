
#include "libmembus/libmembus_internal.h"

namespace LIBMEMBUS_NS
{

memvid::vidview memvid::getBuf(int64_t idx) noexcept(false)
{
    char *p = m_mem.data();
    if (!p)
        throw std::exception();

    int64_t *pSize = (int64_t*)(p + hv_size);
    int64_t *pPtr = (int64_t*)(p + hv_ptr);
    int64_t *pWidth = (int64_t*)(p + hv_width);
    int64_t *pHeight = (int64_t*)(p + hv_height);
    int64_t *pScanWidth = (int64_t*)(p + hv_scanwidth);
    int64_t *pBpp = (int64_t*)(p + hv_bpp);
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

    return memvid::vidview(m_mem.data() + hv_last + (idx * *pBlockSz) + fv_last,
                           *pScanWidth * *pHeight, *pScanWidth, *pWidth, *pHeight);
}

bool memvid::fill(int64_t idx, int col)
{
    char *p = m_mem.data();
    if (!p)
        return false;

    int64_t *pSize = (int64_t*)(p + hv_size);
    int64_t *pPtr = (int64_t*)(p + hv_ptr);
    int64_t *pWidth = (int64_t*)(p + hv_width);
    int64_t *pHeight = (int64_t*)(p + hv_height);
    int64_t *pScanWidth = (int64_t*)(p + hv_scanwidth);
    int64_t *pBpp = (int64_t*)(p + hv_bpp);
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

    memset(m_mem.data() + hv_last + (idx * *pBlockSz) + fv_last, col, *pScanWidth * *pHeight);

    return true;
}


int64_t memvid::getBufs()
{
    char *p = m_mem.data();
    if (!p)
        return 0;

    // Pointer pointer
    return *(int64_t*)(p + hv_bufs);
}

int64_t memvid::getPtr(int64_t offset)
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

int64_t memvid::setPtr(int64_t ptr)
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

int64_t memvid::next(int64_t inc)
{
    char *p = m_mem.data();
    if (!p)
        return -1;

    return setPtr(*(int64_t*)(p + hv_ptr) + inc);
}

int64_t memvid::getPtrErr(int64_t pos, int64_t bias)
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

int64_t memvid::getWidth()
{
    char *p = m_mem.data();
    if (!p)
        return -1;

    return *(int64_t*)(p + hv_width);
}

int64_t memvid::getHeight()
{
    char *p = m_mem.data();
    if (!p)
        return -1;

    return *(int64_t*)(p + hv_height);
}

int64_t memvid::getBpp()
{
    char *p = m_mem.data();
    if (!p)
        return -1;

    return *(int64_t*)(p + hv_bpp);
}

int64_t memvid::getFps()
{
    char *p = m_mem.data();
    if (!p)
        return -1;

    return *(int64_t*)(p + hv_fps);
}

void memvid::close()
{
    m_mem.close();
}

bool memvid::open_existing(const std::string &sName)
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


bool memvid::open(const std::string &sName, bool bCreate, int64_t w, int64_t h,
                  int64_t bpp, int64_t fps, int64_t bufs)
{
    close();

    // Param check
    if (0 >= w || 0 >= h || 24 != bpp || 0 >= fps || 0 >= bufs)
        return false;

    // Calculate scan width
    int64_t sw = abs(w * fitTo<int64_t>(bpp, 8));

    // Image block size
    int64_t blocksz = fv_last + (sw * h);

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
    int64_t *pWidth = (int64_t*)(p + hv_width);
    int64_t *pHeight = (int64_t*)(p + hv_height);
    int64_t *pScanWidth = (int64_t*)(p + hv_scanwidth);
    int64_t *pBpp = (int64_t*)(p + hv_bpp);
    int64_t *pFps = (int64_t*)(p + hv_fps);
    int64_t *pBufs = (int64_t*)(p + hv_bufs);
    int64_t *pBlockSz = (int64_t*)(p + hv_blocksz);

    // Initialize if not existing
    if (!m_mem.existing())
    {
        *pSize = total;
        *pPtr = 0;
        *pWidth = w;
        *pHeight = h;
        *pScanWidth = sw;
        *pBpp = bpp;
        *pFps = fps;
        *pBufs = bufs;
        *pBlockSz = blocksz;
    }

    // Exists, so makes sure it matches what we expect
    else if (*pSize != total
             || *pWidth != w
             || *pHeight != h
             || *pScanWidth != sw
             || *pBpp != bpp
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
