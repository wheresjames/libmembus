
#include "libmembus-internal.h"

#include <chrono>
#include <limits>
#include <thread>

namespace LIBMEMBUS_NS
{

const char *audio_format_name(audio_format fmt)
{
    switch (fmt)
    {
    case audio_format::u8:    return "U8";
    case audio_format::s16le: return "S16LE";
    case audio_format::s24le: return "S24LE";
    case audio_format::s32le: return "S32LE";
    case audio_format::f32le: return "F32LE";
    case audio_format::f64le: return "F64LE";
    case audio_format::userType: return "USERTYPE";
    }
    return "unknown";
}

int64_t audio_format_bytes_per_sample(audio_format fmt)
{
    switch (fmt)
    {
    case audio_format::u8:    return 1;
    case audio_format::s16le: return 2;
    case audio_format::s24le: return 3;
    case audio_format::s32le: return 4;
    case audio_format::f32le: return 4;
    case audio_format::f64le: return 8;
    case audio_format::userType: return 0;
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

    bool alignUp(int64_t v, int64_t unit, int64_t &out)
    {
        if (v < 0 || unit <= 0) return false;
        int64_t t = 0;
        if (!checkedAdd(v, unit - 1, t)) return false;
        out = (t / unit) * unit;
        return true;
    }

    // Compute the SoA layout from a known per-slot payload byte count.
    bool computeLayout(int64_t payload, int64_t align, int64_t bufs,
                       int64_t frameStride, int64_t metasize,
                       int64_t &fvHdr, int64_t &blocksz,
                       int64_t &useroffset, int64_t &dataoffset, int64_t &total)
    {
        if (payload <= 0 || bufs <= 0 || !isPow2(align) || align < 8
            || frameStride < 0 || metasize < 0)
            return false;

        int64_t hdrPlusPayload = 0;
        if (!alignUp(memaud::fv_last, align, fvHdr)
            || !checkedAdd(fvHdr, payload, hdrPlusPayload)
            || !alignUp(hdrPlusPayload, align, blocksz))
            return false;

        int64_t mainuserBase = 0, userEnd = 0, userSpan = 0, dataEndAlign = 0, blocks = 0;
        if (!alignUp(memaud::hv_last, (int64_t)8, mainuserBase)
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

    // Compute per-slot payload bytes. Opaque userType buffers have no sample
    // width, so the caller supplies the fixed slot payload size explicitly.
    bool calcPayload(int64_t ch, audio_format fmt, int64_t sampleRate, int64_t fps,
                     int64_t payloadSize, int64_t &payload)
    {
        if (fmt == audio_format::userType)
        {
            if (ch <= 0 || sampleRate <= 0 || fps <= 0 || payloadSize <= 0)
                return false;
            payload = payloadSize;
            return true;
        }

        int64_t sampleBytes = audio_format_bytes_per_sample(fmt);
        if (ch <= 0 || sampleBytes <= 0 || sampleRate <= 0 || fps <= 0)
            return false;
        int64_t samplesPerFrame = (sampleRate + fps - 1) / fps;  // round up to avoid drift
        int64_t chBytes = 0;
        if (samplesPerFrame <= 0
            || !checkedMul(samplesPerFrame, sampleBytes, chBytes)
            || !checkedMul(chBytes, ch, payload))
            return false;
        return true;
    }

    bool validateMappedLayout(char *p, int64_t mappedSize)
    {
        if (!p || mappedSize < memaud::hv_last)
            return false;

        if (*(int64_t*)(p + memaud::hv_magic)   != memaud::k_magic
            || *(int64_t*)(p + memaud::hv_type) != memaud::k_type
            || *(int64_t*)(p + memaud::hv_version) != memaud::k_version)
            return false;

        int64_t size       = *(int64_t*)(p + memaud::hv_size);
        int64_t ch         = *(int64_t*)(p + memaud::hv_ch);
        audio_format fmt   = (audio_format)*(int64_t*)(p + memaud::hv_format);
        int64_t sampleRate = *(int64_t*)(p + memaud::hv_samplerate);
        int64_t fps        = *(int64_t*)(p + memaud::hv_fps);
        int64_t bufs       = *(int64_t*)(p + memaud::hv_bufs);
        int64_t align      = *(int64_t*)(p + memaud::hv_align);
        int64_t frameExtra = *(int64_t*)(p + memaud::hv_frameextra);
        int64_t metasize   = *(int64_t*)(p + memaud::hv_metasize);
        int64_t blocksz    = *(int64_t*)(p + memaud::hv_blocksz);
        int64_t useroffset = *(int64_t*)(p + memaud::hv_useroffset);
        int64_t dataoffset = *(int64_t*)(p + memaud::hv_dataoffset);
        int64_t payloadsz  = *(int64_t*)(p + memaud::hv_payloadsize);

        int64_t payload = 0, cFvHdr = 0, cBlock = 0, cUser = 0, cData = 0, cTotal = 0;
        return calcPayload(ch, fmt, sampleRate, fps, payloadsz, payload)
            && computeLayout(payload, align, bufs, frameExtra, metasize,
                             cFvHdr, cBlock, cUser, cData, cTotal)
            && payloadsz == payload
            && blocksz == cBlock
            && useroffset == cUser
            && dataoffset == cData
            && size == cTotal
            && size <= mappedSize;
    }

    // Recompute per-slot payload bytes from a live header.
    int64_t payloadFromHeader(char *p)
    {
        int64_t ch         = *(int64_t*)(p + memaud::hv_ch);
        audio_format fmt   = (audio_format)*(int64_t*)(p + memaud::hv_format);
        int64_t sampleRate = *(int64_t*)(p + memaud::hv_samplerate);
        int64_t fps        = *(int64_t*)(p + memaud::hv_fps);
        int64_t payloadsz  = *(int64_t*)(p + memaud::hv_payloadsize);
        int64_t payload = 0;
        return calcPayload(ch, fmt, sampleRate, fps, payloadsz, payload) ? payload : 0;
    }
}

memaud::audview memaud::getBuf(int64_t idx) noexcept(false)
{
    char *p = m_mem.data();
    if (!p)
        throw std::exception();

    int64_t bufs    = *(int64_t*)(p + hv_bufs);
    int64_t blockSz = *(int64_t*)(p + hv_blocksz);
    int64_t ch      = *(int64_t*)(p + hv_ch);
    int64_t align   = *(int64_t*)(p + hv_align);
    int64_t dataoff = *(int64_t*)(p + hv_dataoffset);
    audio_format fmt = (audio_format)*(int64_t*)(p + hv_format);
    int64_t mapped  = m_mem.size();

    int64_t fvHdr = 0, payload = payloadFromHeader(p);
    if (bufs <= 0 || blockSz <= 0 || ch <= 0 || payload <= 0
        || !isPow2(align) || dataoff < hv_last || !alignUp(fv_last, align, fvHdr))
        throw std::exception();

    idx = ((idx % bufs) + bufs) % bufs;

    int64_t slotStart = dataoff + idx * blockSz;
    int64_t dataStart = slotStart + fvHdr;
    if (slotStart < dataoff || dataStart < slotStart
        || dataStart + payload > mapped)
        throw std::exception();

    return memaud::audview(p + dataStart, payload, ch, fmt);
}

bool memaud::fill(int64_t idx, int col)
{
    char *p = m_mem.data();
    if (!p)
        return false;

    int64_t bufs    = *(int64_t*)(p + hv_bufs);
    int64_t blockSz = *(int64_t*)(p + hv_blocksz);
    int64_t align   = *(int64_t*)(p + hv_align);
    int64_t dataoff = *(int64_t*)(p + hv_dataoffset);
    int64_t mapped  = m_mem.size();

    int64_t fvHdr = 0, payload = payloadFromHeader(p);
    if (bufs <= 0 || blockSz <= 0 || payload <= 0
        || !isPow2(align) || dataoff < hv_last || !alignUp(fv_last, align, fvHdr))
        return false;

    idx = ((idx % bufs) + bufs) % bufs;

    int64_t dataStart = dataoff + idx * blockSz + fvHdr;
    if (dataStart < dataoff + fvHdr || dataStart + payload > mapped)
        return false;

    memset(p + dataStart, col, payload);
    return true;
}


int64_t memaud::getBufs()
{
    char *p = m_mem.data();
    if (!p)
        return 0;
    return *(int64_t*)(p + hv_bufs);
}

int64_t memaud::getPtr(int64_t offset)
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

int64_t memaud::setPtr(int64_t ptr)
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

int64_t memaud::next(int64_t inc)
{
    char *p = m_mem.data();
    if (!p)
        return -1;

    int64_t *pBufs    = (int64_t*)(p + hv_bufs);
    int64_t *pBlockSz = (int64_t*)(p + hv_blocksz);
    int64_t dataoff   = *(int64_t*)(p + hv_dataoffset);

    int64_t cur = std::atomic_ref<int64_t>(*(int64_t*)(p + hv_ptr)).load(std::memory_order_acquire);

    if (0 < *pBufs && 0 < *pBlockSz && dataoff >= hv_last)
    {
        int64_t seq = std::atomic_ref<int64_t>(*(int64_t*)(p + hv_seq))
                          .fetch_add(1, std::memory_order_relaxed) + 1;
        auto *pFrameSeq = (int64_t*)(p + dataoff + (cur * *pBlockSz) + fv_seq);
        std::atomic_ref<int64_t>(*pFrameSeq).store(seq, std::memory_order_release);
    }

    return setPtr(cur + inc);
}

bool memaud::waitForFrame(uint64_t wait_ms, int64_t lastSeq)
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
    int64_t *slotFieldA(char *p, int64_t mappedSize, int64_t dataoff,
                        int64_t fieldOffset, int64_t bufs, int64_t blockSz, int64_t idx)
    {
        if (bufs <= 0 || blockSz <= 0 || dataoff < memaud::hv_last) return nullptr;
        idx = ((idx % bufs) + bufs) % bufs;
        int64_t off = dataoff + idx * blockSz + fieldOffset;
        if (off < 0 || off + (int64_t)sizeof(int64_t) > mappedSize) return nullptr;
        return reinterpret_cast<int64_t *>(p + off);
    }
}

bool memaud::setPts(int64_t idx, int64_t pts)
{
    char *p = m_mem.data();
    if (!p) return false;
    int64_t bufs = *(int64_t*)(p + hv_bufs), blockSz = *(int64_t*)(p + hv_blocksz);
    int64_t dataoff = *(int64_t*)(p + hv_dataoffset);
    int64_t *field = slotFieldA(p, m_mem.size(), dataoff, fv_pts, bufs, blockSz, idx);
    if (!field) return false;
    std::atomic_ref<int64_t>(*field).store(pts, std::memory_order_relaxed);
    return true;
}

int64_t memaud::getPts(int64_t idx)
{
    char *p = m_mem.data();
    if (!p) return 0;
    int64_t bufs = *(int64_t*)(p + hv_bufs), blockSz = *(int64_t*)(p + hv_blocksz);
    int64_t dataoff = *(int64_t*)(p + hv_dataoffset);
    int64_t *field = slotFieldA(p, m_mem.size(), dataoff, fv_pts, bufs, blockSz, idx);
    if (!field) return 0;
    return std::atomic_ref<int64_t>(*field).load(std::memory_order_relaxed);
}

int64_t memaud::getUserLen(int64_t idx)
{
    char *p = m_mem.data();
    if (!p) return 0;
    int64_t bufs = *(int64_t*)(p + hv_bufs), blockSz = *(int64_t*)(p + hv_blocksz);
    int64_t dataoff = *(int64_t*)(p + hv_dataoffset);
    int64_t stride  = *(int64_t*)(p + hv_frameextra);
    int64_t *field = slotFieldA(p, m_mem.size(), dataoff, fv_userlen, bufs, blockSz, idx);
    if (!field) return 0;
    int64_t len = std::atomic_ref<int64_t>(*field).load(std::memory_order_relaxed);
    // Clamp to the buffer stride so a corrupted length can't drive an OOB read.
    if (len < 0) return 0;
    return (len > stride) ? stride : len;
}

int64_t memaud::getFrameExtra()
{
    char *p = m_mem.data();
    if (!p) return 0;
    return *(int64_t*)(p + hv_frameextra);
}

bool memaud::setUserData(int64_t idx, const void *data, int64_t len)
{
    char *p = m_mem.data();
    if (!p || len < 0) return false;

    int64_t bufs    = *(int64_t*)(p + hv_bufs);
    int64_t blockSz = *(int64_t*)(p + hv_blocksz);
    int64_t stride  = *(int64_t*)(p + hv_frameextra);
    int64_t useroff = *(int64_t*)(p + hv_useroffset);
    int64_t dataoff = *(int64_t*)(p + hv_dataoffset);
    int64_t mapped  = m_mem.size();
    if (bufs <= 0 || stride <= 0 || len > stride) return false;

    idx = ((idx % bufs) + bufs) % bufs;
    int64_t off = useroff + idx * stride;
    if (off < hv_last || off + stride > dataoff || off + stride > mapped) return false;

    if (len > 0 && data) memcpy(p + off, data, len);

    int64_t *field = slotFieldA(p, mapped, dataoff, fv_userlen, bufs, blockSz, idx);
    if (!field) return false;
    std::atomic_ref<int64_t>(*field).store(len, std::memory_order_relaxed);
    return true;
}

const char *memaud::getUserData(int64_t idx)
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

int64_t memaud::getSessionId()
{
    char *p = m_mem.data();
    if (!p)
        return 0;
    return *(int64_t*)(p + hv_id);
}

int64_t memaud::getVersion()
{
    char *p = m_mem.data();
    if (!p) return -1;
    return *(int64_t*)(p + hv_version);
}

int64_t memaud::getAlign()
{
    char *p = m_mem.data();
    if (!p) return 0;
    return *(int64_t*)(p + hv_align);
}

uint32_t memaud::getFourcc()
{
    char *p = m_mem.data();
    if (!p) return 0;
    return (uint32_t)*(int64_t*)(p + hv_fourcc);
}

bool memaud::getGuid(uint8_t out[16])
{
    char *p = m_mem.data();
    if (!p) return false;
    memcpy(out, p + hv_guid_lo, 16);
    int64_t lo = *(int64_t*)(p + hv_guid_lo), hi = *(int64_t*)(p + hv_guid_hi);
    return (lo | hi) != 0;
}

const char *memaud::getMeta()
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

int64_t memaud::getMetaSize()
{
    char *p = m_mem.data();
    if (!p) return 0;
    return *(int64_t*)(p + hv_metasize);
}

int64_t memaud::getSeq()
{
    char *p = m_mem.data();
    if (!p)
        return -1;
    return std::atomic_ref<int64_t>(*(int64_t*)(p + hv_seq)).load(std::memory_order_acquire);
}

int64_t memaud::getFrameSeq(int64_t idx)
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

int64_t memaud::getPtrErr(int64_t pos, int64_t bias)
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

int64_t memaud::getChannels()
{
    char *p = m_mem.data();
    if (!p)
        return 0;
    return *(int64_t*)(p + hv_ch);
}

audio_format memaud::getFormat()
{
    char *p = m_mem.data();
    if (!p)
        return (audio_format)0;
    return (audio_format)*(int64_t*)(p + hv_format);
}

const char *memaud::getFormatName()
{
    return audio_format_name(getFormat());
}

int64_t memaud::getBytesPerSample()
{
    return audio_format_bytes_per_sample(getFormat());
}

int64_t memaud::getSampleRate()
{
    char *p = m_mem.data();
    if (!p)
        return 0;
    return *(int64_t*)(p + hv_samplerate);
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
    return payloadFromHeader(p);
}


void memaud::close()
{
    m_mem.close();
}

bool memaud::open_existing(const std::string &sName)
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


bool memaud::open(const std::string &sName, bool bCreate, int64_t ch, audio_format fmt,
                  int64_t sampleRate, int64_t fps, int64_t bufs,
                  int64_t align, int64_t frameextra,
                  uint32_t fourcc, const uint8_t *guid,
                  const void *meta, int64_t metasz, int64_t payloadSize)
{
    close();

    if (metasz < 0 || frameextra < 0 || payloadSize < 0)
        return false;

    if (align <= 0) align = k_defAlign;

    int64_t frameStride = 0;
    if (frameextra > 0 && !alignUp(frameextra, (int64_t)8, frameStride))
        return false;

    int64_t payload = 0, fvHdr = 0, blocksz = 0, useroffset = 0, dataoffset = 0, total = 0;
    if (!calcPayload(ch, fmt, sampleRate, fps, payloadSize, payload)
        || !computeLayout(payload, align, bufs, frameStride, metasz,
                          fvHdr, blocksz, useroffset, dataoffset, total))
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
        *(int64_t*)(p + hv_ch)         = ch;
        *(int64_t*)(p + hv_samplerate) = sampleRate;
        *(int64_t*)(p + hv_format)     = (int64_t)fmt;
        *(int64_t*)(p + hv_fps)        = fps;
        *(int64_t*)(p + hv_blocksz)    = blocksz;
        *(int64_t*)(p + hv_frameextra) = frameStride;
        *(int64_t*)(p + hv_useroffset) = useroffset;
        *(int64_t*)(p + hv_dataoffset) = dataoffset;
        *(int64_t*)(p + hv_payloadsize) = payload;

        if (metasz > 0 && meta)
        {
            int64_t base = 0;
            if (alignUp(hv_last, (int64_t)8, base) && base + metasz <= total)
                memcpy(p + base, meta, metasz);
        }
    }
    else if (!validateMappedLayout(p, m_mem.size())
             || *(int64_t*)(p + hv_size)       != total
             || *(int64_t*)(p + hv_ch)         != ch
             || *(int64_t*)(p + hv_format)     != (int64_t)fmt
             || *(int64_t*)(p + hv_samplerate) != sampleRate
             || *(int64_t*)(p + hv_fps)        != fps
             || *(int64_t*)(p + hv_bufs)       != bufs
             || *(int64_t*)(p + hv_blocksz)    != blocksz
             || *(int64_t*)(p + hv_payloadsize) != payload)
    {
        close();
        return false;
    }

    return true;
}

}; // end namespace
