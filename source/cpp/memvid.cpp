
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
    case video_format::userType: return "USERTYPE";
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
    case video_format::userType: return 0;
    }
    return 0;
}

namespace
{
    const int64_t k_page = 4096;

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

    bool isPow2(int64_t a) { return a > 0 && (a & (a - 1)) == 0; }

    // Round up to a multiple of unit (unit need not be a power of two).
    bool alignUp(int64_t v, int64_t unit, int64_t &out)
    {
        if (v < 0 || unit <= 0) return false;
        int64_t t = 0;
        if (!checkedAdd(v, unit - 1, t)) return false;
        out = (t / unit) * unit;
        return true;
    }

    // Compute the SoA layout from a known scan width.  Used by both open() (which
    // derives sw from the format) and validation (which reads sw from the header).
    bool computeLayout(int64_t sw, int64_t h, int64_t align, int64_t bufs,
                       int64_t frameStride, int64_t metasize,
                       int64_t &fvHdr, int64_t &blocksz,
                       int64_t &useroffset, int64_t &dataoffset, int64_t &total)
    {
        if (sw <= 0 || h <= 0 || bufs <= 0 || !isPow2(align) || align < 8
            || frameStride < 0 || metasize < 0)
            return false;

        int64_t payload = 0, hdrPlusPayload = 0;
        if (!checkedMul(sw, h, payload)
            || !alignUp(memvid::fv_last, align, fvHdr)
            || !checkedAdd(fvHdr, payload, hdrPlusPayload)
            || !alignUp(hdrPlusPayload, align, blocksz))
            return false;

        int64_t mainuserBase = 0, userEnd = 0, userSpan = 0, dataEndAlign = 0, blocks = 0;
        if (!alignUp(memvid::hv_last, (int64_t)8, mainuserBase)
            || !checkedAdd(mainuserBase, metasize, userEnd)
            || !alignUp(userEnd, (int64_t)8, useroffset)
            || !checkedMul(frameStride, bufs, userSpan)
            || !checkedAdd(useroffset, userSpan, userEnd)
            || !alignUp(userEnd, align, dataEndAlign)
            || !alignUp(dataEndAlign, k_page, dataoffset)
            || !checkedMul(blocksz, bufs, blocks)
            || !checkedAdd(dataoffset, blocks, total))
            return false;

        return true;
    }

    // Derive scan width and layout for open() from format + geometry.
    bool calcCreateLayout(int64_t w, int64_t h, video_format fmt, int64_t bufs,
                          int64_t scanwidthIn, int64_t align, int64_t frameStride,
                          int64_t metasize, int64_t &sw, int64_t &fvHdr,
                          int64_t &blocksz, int64_t &useroffset,
                          int64_t &dataoffset, int64_t &total)
    {
        if (w <= 0 || h <= 0 || bufs <= 0)
            return false;

        if (fmt == video_format::userType)
        {
            if (scanwidthIn <= 0) return false;   // opaque formats must supply geometry
            sw = scanwidthIn;
        }
        else
        {
            int64_t bpp = video_format_bytes_per_pixel(fmt);
            if (bpp <= 0) return false;
            if ((fmt == video_format::yuyv422 || fmt == video_format::uyvy422) && (w % 2) != 0)
                return false;
            int64_t derived = 0;
            if (!checkedMul(w, bpp, derived)) return false;
            sw = (scanwidthIn > 0) ? scanwidthIn : derived;
            if (sw < derived) return false;       // scan width may pad but not shrink a row
        }

        return computeLayout(sw, h, align, bufs, frameStride, metasize,
                             fvHdr, blocksz, useroffset, dataoffset, total);
    }

    bool validateMappedLayout(char *p, int64_t mappedSize)
    {
        if (!p || mappedSize < memvid::hv_last)
            return false;

        if (*(int64_t*)(p + memvid::hv_magic)   != memvid::k_magic
            || *(int64_t*)(p + memvid::hv_type) != memvid::k_type
            || *(int64_t*)(p + memvid::hv_version) != memvid::k_version)
            return false;

        int64_t size       = *(int64_t*)(p + memvid::hv_size);
        int64_t width      = *(int64_t*)(p + memvid::hv_width);
        int64_t height     = *(int64_t*)(p + memvid::hv_height);
        int64_t scanWidth  = *(int64_t*)(p + memvid::hv_scanwidth);
        int64_t fps        = *(int64_t*)(p + memvid::hv_fps);
        int64_t bufs       = *(int64_t*)(p + memvid::hv_bufs);
        int64_t align      = *(int64_t*)(p + memvid::hv_align);
        int64_t frameExtra = *(int64_t*)(p + memvid::hv_frameextra);
        int64_t metasize   = *(int64_t*)(p + memvid::hv_metasize);
        int64_t blocksz    = *(int64_t*)(p + memvid::hv_blocksz);
        int64_t useroffset = *(int64_t*)(p + memvid::hv_useroffset);
        int64_t dataoffset = *(int64_t*)(p + memvid::hv_dataoffset);

        int64_t cFvHdr = 0, cBlock = 0, cUser = 0, cData = 0, cTotal = 0;
        return fps > 0 && width > 0 && height > 0
            && computeLayout(scanWidth, height, align, bufs, frameExtra, metasize,
                             cFvHdr, cBlock, cUser, cData, cTotal)
            && blocksz == cBlock
            && useroffset == cUser
            && dataoffset == cData
            && size == cTotal
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
    int64_t align   = *(int64_t*)(p + hv_align);
    int64_t dataoff = *(int64_t*)(p + hv_dataoffset);
    video_format fmt = (video_format)*(int64_t*)(p + hv_format);
    int64_t mapped  = m_mem.size();

    int64_t fvHdr = 0;
    if (bufs <= 0 || blockSz <= 0 || w <= 0 || h <= 0 || sw <= 0
        || !isPow2(align) || dataoff < hv_last || !alignUp(fv_last, align, fvHdr))
        throw std::exception();

    idx = ((idx % bufs) + bufs) % bufs;

    // Verify the slot's pixel region lies entirely within the mapped region
    int64_t slotStart = dataoff + idx * blockSz;
    int64_t dataStart = slotStart + fvHdr;
    int64_t dataSize  = sw * h;
    if (slotStart < dataoff || dataStart < slotStart
        || dataSize < 0 || dataStart + dataSize > mapped)
        throw std::exception();

    return memvid::vidview(p + dataStart, dataSize, sw, w, h, fmt);
}

bool memvid::fill(int64_t idx, int col)
{
    char *p = m_mem.data();
    if (!p)
        return false;

    int64_t bufs    = *(int64_t*)(p + hv_bufs);
    int64_t blockSz = *(int64_t*)(p + hv_blocksz);
    int64_t sw      = *(int64_t*)(p + hv_scanwidth);
    int64_t h       = *(int64_t*)(p + hv_height);
    int64_t align   = *(int64_t*)(p + hv_align);
    int64_t dataoff = *(int64_t*)(p + hv_dataoffset);
    int64_t mapped  = m_mem.size();

    int64_t fvHdr = 0;
    if (bufs <= 0 || blockSz <= 0 || sw <= 0 || h <= 0
        || !isPow2(align) || dataoff < hv_last || !alignUp(fv_last, align, fvHdr))
        return false;

    idx = ((idx % bufs) + bufs) % bufs;

    int64_t dataStart = dataoff + idx * blockSz + fvHdr;
    int64_t dataSize  = sw * h;
    if (dataStart < dataoff + fvHdr || dataSize < 0 || dataStart + dataSize > mapped)
        return false;

    memset(p + dataStart, col, dataSize);
    return true;
}


int64_t memvid::getBufs()
{
    char *p = m_mem.data();
    if (!p)
        return 0;
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
    int64_t dataoff   = *(int64_t*)(p + hv_dataoffset);

    int64_t cur = std::atomic_ref<int64_t>(*(int64_t*)(p + hv_ptr)).load(std::memory_order_acquire);

    // Stamp the frame being published with the next sequence number before
    // advancing the pointer.
    if (0 < *pBufs && 0 < *pBlockSz && dataoff >= hv_last)
    {
        int64_t seq = std::atomic_ref<int64_t>(*(int64_t*)(p + hv_seq))
                          .fetch_add(1, std::memory_order_relaxed) + 1;
        auto *pFrameSeq = (int64_t*)(p + dataoff + (cur * *pBlockSz) + fv_seq);
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
    // Validates slot bounds and returns a pointer to the requested field within
    // the frame header (relative to the stored data-region base), or nullptr.
    int64_t *slotField(char *p, int64_t mappedSize, int64_t dataoff,
                       int64_t fieldOffset, int64_t bufs, int64_t blockSz, int64_t idx)
    {
        if (bufs <= 0 || blockSz <= 0 || dataoff < memvid::hv_last) return nullptr;
        idx = ((idx % bufs) + bufs) % bufs;
        int64_t off = dataoff + idx * blockSz + fieldOffset;
        if (off < 0 || off + (int64_t)sizeof(int64_t) > mappedSize) return nullptr;
        return reinterpret_cast<int64_t *>(p + off);
    }
}

bool memvid::setVpts(int64_t idx, int64_t pts)
{
    char *p = m_mem.data();
    if (!p) return false;
    int64_t bufs = *(int64_t*)(p + hv_bufs), blockSz = *(int64_t*)(p + hv_blocksz);
    int64_t dataoff = *(int64_t*)(p + hv_dataoffset);
    int64_t *field = slotField(p, m_mem.size(), dataoff, fv_vpts, bufs, blockSz, idx);
    if (!field) return false;
    std::atomic_ref<int64_t>(*field).store(pts, std::memory_order_relaxed);
    return true;
}

bool memvid::setApts(int64_t idx, int64_t pts)
{
    char *p = m_mem.data();
    if (!p) return false;
    int64_t bufs = *(int64_t*)(p + hv_bufs), blockSz = *(int64_t*)(p + hv_blocksz);
    int64_t dataoff = *(int64_t*)(p + hv_dataoffset);
    int64_t *field = slotField(p, m_mem.size(), dataoff, fv_apts, bufs, blockSz, idx);
    if (!field) return false;
    std::atomic_ref<int64_t>(*field).store(pts, std::memory_order_relaxed);
    return true;
}

int64_t memvid::getVpts(int64_t idx)
{
    char *p = m_mem.data();
    if (!p) return 0;
    int64_t bufs = *(int64_t*)(p + hv_bufs), blockSz = *(int64_t*)(p + hv_blocksz);
    int64_t dataoff = *(int64_t*)(p + hv_dataoffset);
    int64_t *field = slotField(p, m_mem.size(), dataoff, fv_vpts, bufs, blockSz, idx);
    if (!field) return 0;
    return std::atomic_ref<int64_t>(*field).load(std::memory_order_relaxed);
}

int64_t memvid::getApts(int64_t idx)
{
    char *p = m_mem.data();
    if (!p) return 0;
    int64_t bufs = *(int64_t*)(p + hv_bufs), blockSz = *(int64_t*)(p + hv_blocksz);
    int64_t dataoff = *(int64_t*)(p + hv_dataoffset);
    int64_t *field = slotField(p, m_mem.size(), dataoff, fv_apts, bufs, blockSz, idx);
    if (!field) return 0;
    return std::atomic_ref<int64_t>(*field).load(std::memory_order_relaxed);
}

int64_t memvid::getUserLen(int64_t idx)
{
    char *p = m_mem.data();
    if (!p) return 0;
    int64_t bufs = *(int64_t*)(p + hv_bufs), blockSz = *(int64_t*)(p + hv_blocksz);
    int64_t dataoff = *(int64_t*)(p + hv_dataoffset);
    int64_t stride  = *(int64_t*)(p + hv_frameextra);
    int64_t *field = slotField(p, m_mem.size(), dataoff, fv_userlen, bufs, blockSz, idx);
    if (!field) return 0;
    int64_t len = std::atomic_ref<int64_t>(*field).load(std::memory_order_relaxed);
    // Clamp to the buffer stride so a corrupted length can't drive an OOB read
    // through getUserData().
    if (len < 0) return 0;
    return (len > stride) ? stride : len;
}

int64_t memvid::getFrameExtra()
{
    char *p = m_mem.data();
    if (!p) return 0;
    return *(int64_t*)(p + hv_frameextra);
}

bool memvid::setUserData(int64_t idx, const void *data, int64_t len)
{
    char *p = m_mem.data();
    if (!p || len < 0) return false;

    int64_t bufs      = *(int64_t*)(p + hv_bufs);
    int64_t blockSz   = *(int64_t*)(p + hv_blocksz);
    int64_t stride    = *(int64_t*)(p + hv_frameextra);
    int64_t useroff   = *(int64_t*)(p + hv_useroffset);
    int64_t dataoff   = *(int64_t*)(p + hv_dataoffset);
    int64_t mapped    = m_mem.size();
    if (bufs <= 0 || stride <= 0 || len > stride) return false;

    idx = ((idx % bufs) + bufs) % bufs;
    int64_t off = useroff + idx * stride;
    if (off < hv_last || off + stride > dataoff || off + stride > mapped) return false;

    if (len > 0 && data) memcpy(p + off, data, len);

    // Publish the length in the data-slot frame header (under the fv_seq barrier).
    int64_t *field = slotField(p, mapped, dataoff, fv_userlen, bufs, blockSz, idx);
    if (!field) return false;
    std::atomic_ref<int64_t>(*field).store(len, std::memory_order_relaxed);
    return true;
}

const char *memvid::getUserData(int64_t idx)
{
    char *p = m_mem.data();
    if (!p) return nullptr;

    int64_t bufs    = *(int64_t*)(p + hv_bufs);
    int64_t stride  = *(int64_t*)(p + hv_frameextra);
    int64_t useroff = *(int64_t*)(p + hv_useroffset);
    int64_t dataoff = *(int64_t*)(p + hv_dataoffset);
    int64_t mapped  = m_mem.size();
    if (bufs <= 0 || stride <= 0) return nullptr;

    idx = ((idx % bufs) + bufs) % bufs;
    int64_t off = useroff + idx * stride;
    if (off < hv_last || off + stride > dataoff || off + stride > mapped) return nullptr;
    return p + off;
}

int64_t memvid::getSessionId()
{
    char *p = m_mem.data();
    if (!p)
        return 0;
    return *(int64_t*)(p + hv_id);
}

int64_t memvid::getVersion()
{
    char *p = m_mem.data();
    if (!p) return -1;
    return *(int64_t*)(p + hv_version);
}

int64_t memvid::getAlign()
{
    char *p = m_mem.data();
    if (!p) return 0;
    return *(int64_t*)(p + hv_align);
}

uint32_t memvid::getFourcc()
{
    char *p = m_mem.data();
    if (!p) return 0;
    return (uint32_t)*(int64_t*)(p + hv_fourcc);
}

bool memvid::getGuid(uint8_t out[16])
{
    char *p = m_mem.data();
    if (!p) return false;
    memcpy(out, p + hv_guid_lo, 16);
    int64_t lo = *(int64_t*)(p + hv_guid_lo), hi = *(int64_t*)(p + hv_guid_hi);
    return (lo | hi) != 0;
}

const char *memvid::getMeta()
{
    char *p = m_mem.data();
    if (!p) return nullptr;
    int64_t metasize = *(int64_t*)(p + hv_metasize);
    if (metasize <= 0) return nullptr;
    int64_t base = 0;
    if (!alignUp(hv_last, (int64_t)8, base)) return nullptr;
    if (base + metasize > m_mem.size()) return nullptr;
    return p + base;
}

int64_t memvid::getMetaSize()
{
    char *p = m_mem.data();
    if (!p) return 0;
    return *(int64_t*)(p + hv_metasize);
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
    int64_t dataoff   = *(int64_t*)(p + hv_dataoffset);
    if (0 >= *pBufs || 0 >= *pBlockSz || dataoff < hv_last)
        return -1;

    idx = ((idx % *pBufs) + *pBufs) % *pBufs;
    return std::atomic_ref<int64_t>(
               *(int64_t*)(p + dataoff + (idx * *pBlockSz) + fv_seq))
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

    int64_t ptr = ((cur + bias) % *pBufs + *pBufs) % *pBufs;
    pos = ((pos % *pBufs) + *pBufs) % *pBufs;

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

    if (!validateMappedLayout(p, m_mem.size()))
    {
        set_last_error(errc::invalid_layout);
        close();
        return false;
    }

    return true;
}


bool memvid::open(const std::string &sName, bool bCreate, int64_t w, int64_t h,
                  video_format fmt, int64_t fps, int64_t bufs,
                  int64_t scanwidth, int64_t align, int64_t frameextra,
                  uint32_t fourcc, const uint8_t *guid,
                  const void *meta, int64_t metasz)
{
    close();

    if (0 >= fps || metasz < 0 || frameextra < 0)
        return false;

    if (align <= 0) align = k_defAlign;

    // Per-frame user-buffer stride is aligned to 8 for field alignment.
    int64_t frameStride = 0;
    if (frameextra > 0 && !alignUp(frameextra, (int64_t)8, frameStride))
        return false;

    int64_t sw = 0, fvHdr = 0, blocksz = 0, useroffset = 0, dataoffset = 0, total = 0;
    if (!calcCreateLayout(w, h, fmt, bufs, scanwidth, align, frameStride, metasz,
                          sw, fvHdr, blocksz, useroffset, dataoffset, total))
        return false;

    if (!m_mem.open(sName, total, bCreate, bCreate))
        return false;

    char *p = m_mem.data();
    if (!p)
    {
        close();
        return false;
    }

    if (!m_mem.existing())
    {
        *(int64_t*)(p + hv_magic)      = k_magic;
        *(int64_t*)(p + hv_type)       = k_type;
        *(int64_t*)(p + hv_version)    = k_version;
        *(int64_t*)(p + hv_size)       = total;
        *(int64_t*)(p + hv_ptr)        = 0;
        *(int64_t*)(p + hv_seq)        = 0;
        *(int64_t*)(p + hv_id)         = (int64_t)std::mt19937_64(std::random_device{}())();
        *(int64_t*)(p + hv_bufs)       = bufs;
        *(int64_t*)(p + hv_fourcc)     = (int64_t)fourcc;
        *(int64_t*)(p + hv_guid_lo)    = 0;
        *(int64_t*)(p + hv_guid_hi)    = 0;
        if (guid) memcpy(p + hv_guid_lo, guid, 16);
        *(int64_t*)(p + hv_align)      = align;
        *(int64_t*)(p + hv_metasize)   = metasz;
        for (int i = 0; i < 8; ++i) *(int64_t*)(p + hv_reserved0 + i * sizeof(int64_t)) = 0;
        *(int64_t*)(p + hv_width)      = w;
        *(int64_t*)(p + hv_height)     = h;
        *(int64_t*)(p + hv_scanwidth)  = sw;
        *(int64_t*)(p + hv_format)     = (int64_t)fmt;
        *(int64_t*)(p + hv_fps)        = fps;
        *(int64_t*)(p + hv_blocksz)    = blocksz;
        *(int64_t*)(p + hv_frameextra) = frameStride;
        *(int64_t*)(p + hv_useroffset) = useroffset;
        *(int64_t*)(p + hv_dataoffset) = dataoffset;

        if (metasz > 0 && meta)
        {
            int64_t base = 0;
            if (alignUp(hv_last, (int64_t)8, base) && base + metasz <= total)
                memcpy(p + base, meta, metasz);
        }
    }
    else if (!validateMappedLayout(p, m_mem.size())
             || *(int64_t*)(p + hv_size)      != total
             || *(int64_t*)(p + hv_width)     != w
             || *(int64_t*)(p + hv_height)    != h
             || *(int64_t*)(p + hv_scanwidth) != sw
             || *(int64_t*)(p + hv_format)    != (int64_t)fmt
             || *(int64_t*)(p + hv_fps)       != fps
             || *(int64_t*)(p + hv_bufs)      != bufs
             || *(int64_t*)(p + hv_blocksz)   != blocksz)
    {
        close();
        return false;
    }

    return true;
}

}; // end namespace
