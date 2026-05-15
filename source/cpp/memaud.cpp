
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

    bool calcLayout(int64_t ch, audio_format fmt, int64_t sampleRate, int64_t fps, int64_t bufs,
                    int64_t &blocksz, int64_t &total)
    {
        int64_t sampleBytes = audio_format_bytes_per_sample(fmt);
        if (ch <= 0 || sampleBytes <= 0 || sampleRate <= 0 || fps <= 0 || bufs <= 0)
            return false;

        int64_t samplesPerFrame = (sampleRate + fps - 1) / fps;  // round up to avoid drift
        int64_t payload = 0, channelPayload = 0, blocks = 0;
        if (samplesPerFrame <= 0
            || !checkedMul(samplesPerFrame, sampleBytes, payload)
            || !checkedMul(payload, ch, channelPayload)
            || !checkedAdd(memaud::fv_last, channelPayload, blocksz)
            || !checkedMul(blocksz, bufs, blocks)
            || !checkedAdd(memaud::hv_last, blocks, total))
            return false;

        return true;
    }

    bool validateMappedLayout(char *p, int64_t mappedSize)
    {
        if (!p || mappedSize < memaud::hv_last)
            return false;

        int64_t size    = *(int64_t*)(p + memaud::hv_size);
        int64_t ch      = *(int64_t*)(p + memaud::hv_ch);
        audio_format fmt = (audio_format)*(int64_t*)(p + memaud::hv_format);
        int64_t sampleRate = *(int64_t*)(p + memaud::hv_samplerate);
        int64_t fps     = *(int64_t*)(p + memaud::hv_fps);
        int64_t bufs    = *(int64_t*)(p + memaud::hv_bufs);
        int64_t blocksz = *(int64_t*)(p + memaud::hv_blocksz);
        int64_t calcBlock = 0, calcTotal = 0;

        return calcLayout(ch, fmt, sampleRate, fps, bufs, calcBlock, calcTotal)
            && blocksz == calcBlock
            && size == calcTotal
            && size <= mappedSize;
    }
}

memaud::audview memaud::getBuf(int64_t idx) noexcept(false)
{
    char *p = m_mem.data();
    if (!p)
        throw std::exception();

    // Snapshot header values to prevent TOCTOU from a concurrent header write
    int64_t bufs    = *(int64_t*)(p + hv_bufs);
    int64_t blockSz = *(int64_t*)(p + hv_blocksz);
    int64_t ch      = *(int64_t*)(p + hv_ch);
    audio_format fmt = (audio_format)*(int64_t*)(p + hv_format);
    int64_t mapped  = m_mem.size();

    if (bufs <= 0 || blockSz <= 0 || ch <= 0 || blockSz <= fv_last)
        throw std::exception();

    idx = ((idx % bufs) + bufs) % bufs;

    int64_t slotStart = hv_last + idx * blockSz;
    int64_t dataStart = slotStart + fv_last;
    int64_t dataSize  = blockSz - fv_last;
    if (slotStart < hv_last || dataStart < slotStart
        || dataSize <= 0 || dataStart + dataSize > mapped)
        throw std::exception();

    return memaud::audview(p + dataStart, dataSize, ch, fmt);
}

bool memaud::fill(int64_t idx, int col)
{
    char *p = m_mem.data();
    if (!p)
        return false;

    // Snapshot to prevent TOCTOU
    int64_t bufs    = *(int64_t*)(p + hv_bufs);
    int64_t blockSz = *(int64_t*)(p + hv_blocksz);
    int64_t mapped  = m_mem.size();

    if (bufs <= 0 || blockSz <= 0 || blockSz <= fv_last)
        return false;

    idx = ((idx % bufs) + bufs) % bufs;

    int64_t dataStart = hv_last + idx * blockSz + fv_last;
    int64_t dataSize  = blockSz - fv_last;
    if (dataStart < (int64_t)hv_last + (int64_t)fv_last || dataSize <= 0 || dataStart + dataSize > mapped)
        return false;

    memset(p + dataStart, col, dataSize);
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

    int64_t cur = std::atomic_ref<int64_t>(*(int64_t*)(p + hv_ptr)).load(std::memory_order_acquire);

    if (0 < *pBufs && 0 < *pBlockSz)
    {
        int64_t seq = std::atomic_ref<int64_t>(*(int64_t*)(p + hv_seq))
                          .fetch_add(1, std::memory_order_relaxed) + 1;
        auto *pFrameSeq = (int64_t*)(p + hv_last + (cur * *pBlockSz) + fv_seq);
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

int64_t memaud::getSessionId()
{
    char *p = m_mem.data();
    if (!p)
        return 0;
    return *(int64_t*)(p + hv_id);
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
    if (0 >= *pBufs || 0 >= *pBlockSz)
        return -1;

    idx = ((idx % *pBufs) + *pBufs) % *pBufs;
    return std::atomic_ref<int64_t>(
               *(int64_t*)(p + hv_last + (idx * *pBlockSz) + fv_seq))
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

    return *(int64_t*)(p + hv_blocksz) - fv_last;
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

    // Validate critical header fields before trusting them for pointer arithmetic
    if (!validateMappedLayout(p, m_mem.size()))
    {
        close();
        return false;
    }

    return true;
}


bool memaud::open(const std::string &sName, bool bCreate, int64_t ch, audio_format fmt, int64_t sampleRate, int64_t fps, int64_t bufs)
{
    close();

    int64_t blocksz = 0, total = 0;
    if (!calcLayout(ch, fmt, sampleRate, fps, bufs, blocksz, total))
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
    int64_t *pCh = (int64_t*)(p + hv_ch);
    int64_t *pFormat = (int64_t*)(p + hv_format);
    int64_t *pSampleRate = (int64_t*)(p + hv_samplerate);
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
        *pCh = ch;
        *pFormat = (int64_t)fmt;
        *pSampleRate = sampleRate;
        *pFps = fps;
        *pBufs = bufs;
        *pBlockSz = blocksz;
    }

    // Exists, so makes sure it matches what we expect
    else if (!validateMappedLayout(p, m_mem.size())
             || *pSize != total
             || *pCh != ch
             || *pFormat != (int64_t)fmt
             || *pSampleRate != sampleRate
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
