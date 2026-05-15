
#include "libmembus-internal.h"

#include <chrono>
#include <limits>
#include <thread>

namespace LIBMEMBUS_NS
{

const char *video_format_name(video_format fmt)
{
    switch (fmt)
    {
    case video_format::gray8:   return "GRAY8";
    case video_format::rgb24:   return "RGB24";
    case video_format::bgr24:   return "BGR24";
    case video_format::rgba32:  return "RGBA32";
    case video_format::bgra32:  return "BGRA32";
    case video_format::yuyv422: return "YUYV422";
    case video_format::uyvy422: return "UYVY422";
    }
    return "unknown";
}

int64_t video_format_bytes_per_pixel(video_format fmt)
{
    switch (fmt)
    {
    case video_format::gray8:   return 1;
    case video_format::rgb24:   return 3;
    case video_format::bgr24:   return 3;
    case video_format::rgba32:  return 4;
    case video_format::bgra32:  return 4;
    case video_format::yuyv422: return 2;
    case video_format::uyvy422: return 2;
    }
    return 0;
}

namespace
{
    bool checkedAdd(int64_t a, int64_t b, int64_t &out)
    {
        if (a < 0 || b < 0 || a > std::numeric_limits<int64_t>::max() - b)
            return false;
        out = a + b;
        return true;
    }

    bool checkedMul(int64_t a, int64_t b, int64_t &out)
    {
        if (a < 0 || b < 0 || (a != 0 && b > std::numeric_limits<int64_t>::max() / a))
            return false;
        out = a * b;
        return true;
    }

    bool calcLayout(int64_t w, int64_t h, video_format fmt, int64_t bufs,
                    int64_t &sw, int64_t &blocksz, int64_t &total)
    {
        int64_t bytesPerPixel = video_format_bytes_per_pixel(fmt);
        if (w <= 0 || h <= 0 || bytesPerPixel <= 0 || bufs <= 0)
            return false;
        if ((fmt == video_format::yuyv422 || fmt == video_format::uyvy422) && (w % 2) != 0)
            return false;

        int64_t payload = 0, blocks = 0;
        if (!checkedMul(w, bytesPerPixel, sw)
            || !checkedMul(sw, h, payload)
            || !checkedAdd(memvid::fv_last, payload, blocksz)
            || !checkedMul(blocksz, bufs, blocks)
            || !checkedAdd(memvid::hv_last, blocks, total))
            return false;

        return true;
    }

    bool validateMappedLayout(char *p, int64_t mappedSize)
    {
        if (!p || mappedSize < memvid::hv_last)
            return false;

        int64_t size      = *(int64_t*)(p + memvid::hv_size);
        int64_t width     = *(int64_t*)(p + memvid::hv_width);
        int64_t height    = *(int64_t*)(p + memvid::hv_height);
        int64_t scanWidth = *(int64_t*)(p + memvid::hv_scanwidth);
        video_format fmt  = (video_format)*(int64_t*)(p + memvid::hv_format);
        int64_t fps       = *(int64_t*)(p + memvid::hv_fps);
        int64_t bufs      = *(int64_t*)(p + memvid::hv_bufs);
        int64_t blocksz   = *(int64_t*)(p + memvid::hv_blocksz);
        int64_t calcSw = 0, calcBlock = 0, calcTotal = 0;

        return fps > 0
            && calcLayout(width, height, fmt, bufs, calcSw, calcBlock, calcTotal)
            && scanWidth == calcSw
            && blocksz == calcBlock
            && size == calcTotal
            && size <= mappedSize;
    }
}

memvid::vidview memvid::getBuf(int64_t idx) noexcept(false)
{
    char *p = m_mem.data();
    if (!p)
        throw std::exception();

    // Snapshot header values to prevent TOCTOU from a concurrent header write
    int64_t bufs    = *(int64_t*)(p + hv_bufs);
    int64_t blockSz = *(int64_t*)(p + hv_blocksz);
    int64_t w       = *(int64_t*)(p + hv_width);
    int64_t h       = *(int64_t*)(p + hv_height);
    int64_t sw      = *(int64_t*)(p + hv_scanwidth);
    video_format fmt = (video_format)*(int64_t*)(p + hv_format);
    int64_t mapped  = m_mem.size();

    if (bufs <= 0 || blockSz <= 0 || w <= 0 || h <= 0 || sw <= 0)
        throw std::exception();

    idx = ((idx % bufs) + bufs) % bufs;

    // Verify the slot's pixel region lies entirely within the mapped region
    int64_t slotStart = hv_last + idx * blockSz;
    int64_t dataStart = slotStart + fv_last;
    int64_t dataSize  = sw * h;
    if (slotStart < hv_last || dataStart < slotStart
        || dataSize < 0 || dataStart + dataSize > mapped)
        throw std::exception();

    return memvid::vidview(p + dataStart, dataSize, sw, w, h, fmt);
}

bool memvid::fill(int64_t idx, int col)
{
    char *p = m_mem.data();
    if (!p)
        return false;

    // Snapshot to prevent TOCTOU
    int64_t bufs    = *(int64_t*)(p + hv_bufs);
    int64_t blockSz = *(int64_t*)(p + hv_blocksz);
    int64_t sw      = *(int64_t*)(p + hv_scanwidth);
    int64_t h       = *(int64_t*)(p + hv_height);
    int64_t mapped  = m_mem.size();

    if (bufs <= 0 || blockSz <= 0 || sw <= 0 || h <= 0)
        return false;

    idx = ((idx % bufs) + bufs) % bufs;

    int64_t dataStart = hv_last + idx * blockSz + fv_last;
    int64_t dataSize  = sw * h;
    if (dataStart < (int64_t)hv_last + (int64_t)fv_last || dataSize < 0 || dataStart + dataSize > mapped)
        return false;

    memset(p + dataStart, col, dataSize);
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

    int64_t *pBufs = (int64_t*)(p + hv_bufs);
    if (0 >= *pBufs)
        return -1;

    int64_t cur = std::atomic_ref<int64_t>(*(int64_t*)(p + hv_ptr)).load(std::memory_order_acquire);
    return ((cur + offset) % *pBufs + *pBufs) % *pBufs;
}

int64_t memvid::setPtr(int64_t ptr)
{
    char *p = m_mem.data();
    if (!p)
        return -1;

    int64_t *pBufs = (int64_t*)(p + hv_bufs);
    if (0 >= *pBufs)
        return -1;

    ptr = ((ptr % *pBufs) + *pBufs) % *pBufs;
    std::atomic_ref<int64_t>(*(int64_t*)(p + hv_ptr)).store(ptr, std::memory_order_release);
    return ptr;
}

int64_t memvid::next(int64_t inc)
{
    char *p = m_mem.data();
    if (!p)
        return -1;

    int64_t *pBufs    = (int64_t*)(p + hv_bufs);
    int64_t *pBlockSz = (int64_t*)(p + hv_blocksz);

    int64_t cur = std::atomic_ref<int64_t>(*(int64_t*)(p + hv_ptr)).load(std::memory_order_acquire);

    // Stamp the frame being published with the next sequence number before
    // advancing the pointer.  Readers compare frame_seq to getSeq() to detect
    // overrun: lapped = (getSeq() - getFrameSeq(rPos)) >= getBufs().
    if (0 < *pBufs && 0 < *pBlockSz)
    {
        int64_t seq = std::atomic_ref<int64_t>(*(int64_t*)(p + hv_seq))
                          .fetch_add(1, std::memory_order_relaxed) + 1;
        auto *pFrameSeq = (int64_t*)(p + hv_last + (cur * *pBlockSz) + fv_seq);
        std::atomic_ref<int64_t>(*pFrameSeq).store(seq, std::memory_order_release);
    }

    return setPtr(cur + inc);
}

bool memvid::waitForFrame(uint64_t wait_ms, int64_t lastSeq)
{
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(wait_ms);
    do
    {
        if (getSeq() > lastSeq)
        {
            set_last_error(errc::ok);
            return true;
        }

        if (wait_ms == 0)
            break;

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    } while (std::chrono::steady_clock::now() < deadline);

    set_last_error(errc::timeout);
    return false;
}

namespace
{
    // Shared helper: validates slot bounds and returns a pointer to the
    // requested field within the frame header, or nullptr on failure.
    int64_t *slotField(char *p, int64_t mappedSize,
                       int64_t hv_last_val, int64_t fieldOffset,
                       int64_t bufs, int64_t blockSz, int64_t idx)
    {
        if (bufs <= 0 || blockSz <= 0) return nullptr;
        idx = ((idx % bufs) + bufs) % bufs;
        int64_t off = hv_last_val + idx * blockSz + fieldOffset;
        if (off < 0 || off + (int64_t)sizeof(int64_t) > mappedSize) return nullptr;
        return reinterpret_cast<int64_t *>(p + off);
    }
}

bool memvid::setVpts(int64_t idx, int64_t pts)
{
    char *p = m_mem.data();
    if (!p) return false;
    int64_t bufs = *(int64_t*)(p + hv_bufs), blockSz = *(int64_t*)(p + hv_blocksz);
    int64_t *field = slotField(p, m_mem.size(), hv_last, fv_vpts, bufs, blockSz, idx);
    if (!field) return false;
    std::atomic_ref<int64_t>(*field).store(pts, std::memory_order_relaxed);
    return true;
}

bool memvid::setApts(int64_t idx, int64_t pts)
{
    char *p = m_mem.data();
    if (!p) return false;
    int64_t bufs = *(int64_t*)(p + hv_bufs), blockSz = *(int64_t*)(p + hv_blocksz);
    int64_t *field = slotField(p, m_mem.size(), hv_last, fv_apts, bufs, blockSz, idx);
    if (!field) return false;
    std::atomic_ref<int64_t>(*field).store(pts, std::memory_order_relaxed);
    return true;
}

int64_t memvid::getVpts(int64_t idx)
{
    char *p = m_mem.data();
    if (!p) return 0;
    int64_t bufs = *(int64_t*)(p + hv_bufs), blockSz = *(int64_t*)(p + hv_blocksz);
    int64_t *field = slotField(p, m_mem.size(), hv_last, fv_vpts, bufs, blockSz, idx);
    if (!field) return 0;
    return std::atomic_ref<int64_t>(*field).load(std::memory_order_relaxed);
}

int64_t memvid::getApts(int64_t idx)
{
    char *p = m_mem.data();
    if (!p) return 0;
    int64_t bufs = *(int64_t*)(p + hv_bufs), blockSz = *(int64_t*)(p + hv_blocksz);
    int64_t *field = slotField(p, m_mem.size(), hv_last, fv_apts, bufs, blockSz, idx);
    if (!field) return 0;
    return std::atomic_ref<int64_t>(*field).load(std::memory_order_relaxed);
}

int64_t memvid::getSessionId()
{
    char *p = m_mem.data();
    if (!p)
        return 0;
    return *(int64_t*)(p + hv_id);
}

int64_t memvid::getSeq()
{
    char *p = m_mem.data();
    if (!p)
        return -1;
    return std::atomic_ref<int64_t>(*(int64_t*)(p + hv_seq)).load(std::memory_order_acquire);
}

int64_t memvid::getFrameSeq(int64_t idx)
{
    char *p = m_mem.data();
    if (!p)
        return -1;

    int64_t *pBufs    = (int64_t*)(p + hv_bufs);
    int64_t *pBlockSz = (int64_t*)(p + hv_blocksz);
    if (0 >= *pBufs || 0 >= *pBlockSz)
        return -1;

    idx = ((idx % *pBufs) + *pBufs) % *pBufs;
    return std::atomic_ref<int64_t>(
               *(int64_t*)(p + hv_last + (idx * *pBlockSz) + fv_seq))
               .load(std::memory_order_acquire);
}

int64_t memvid::getPtrErr(int64_t pos, int64_t bias)
{
    char *p = m_mem.data();
    if (!p)
        return 0;

    int64_t *pBufs = (int64_t*)(p + hv_bufs);
    if (0 >= *pBufs)
        return 0;

    int64_t cur = std::atomic_ref<int64_t>(*(int64_t*)(p + hv_ptr)).load(std::memory_order_acquire);

    // Wrap both values with full modulo to handle any magnitude of offset
    int64_t ptr = ((cur + bias) % *pBufs + *pBufs) % *pBufs;
    pos = ((pos % *pBufs) + *pBufs) % *pBufs;

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

video_format memvid::getFormat()
{
    char *p = m_mem.data();
    if (!p)
        return (video_format)0;

    return (video_format)*(int64_t*)(p + hv_format);
}

const char *memvid::getFormatName()
{
    return video_format_name(getFormat());
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

    if (!m_mem.open(sName, 0, false, false, /*bReadOnly=*/true))
        return false;

    char *p = m_mem.data();
    if (!p)
    {
        close();
        return false;
    }

    // Validate critical header fields before trusting them for pointer arithmetic
    if (!validateMappedLayout(p, m_mem.size()))
    {
        close();
        return false;
    }

    return true;
}


bool memvid::open(const std::string &sName, bool bCreate, int64_t w, int64_t h,
                  video_format fmt, int64_t fps, int64_t bufs)
{
    close();

    int64_t sw = 0, blocksz = 0, total = 0;
    if (0 >= fps || !calcLayout(w, h, fmt, bufs, sw, blocksz, total))
        return false;

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
    int64_t *pFormat = (int64_t*)(p + hv_format);
    int64_t *pFps = (int64_t*)(p + hv_fps);
    int64_t *pBufs = (int64_t*)(p + hv_bufs);
    int64_t *pBlockSz = (int64_t*)(p + hv_blocksz);

    // Initialize if not existing
    if (!m_mem.existing())
    {
        *pSize = total;
        *pPtr = 0;
        *(int64_t*)(p + hv_seq) = 0;
        *(int64_t*)(p + hv_id)  = (int64_t)std::mt19937_64(std::random_device{}())();
        *pWidth = w;
        *pHeight = h;
        *pScanWidth = sw;
        *pFormat = (int64_t)fmt;
        *pFps = fps;
        *pBufs = bufs;
        *pBlockSz = blocksz;
    }

    // Exists, so makes sure it matches what we expect
    else if (!validateMappedLayout(p, m_mem.size())
             || *pSize != total
             || *pWidth != w
             || *pHeight != h
             || *pScanWidth != sw
             || *pFormat != (int64_t)fmt
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
