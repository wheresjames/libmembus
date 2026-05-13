
#include "libmembus-internal.h"

#include <chrono>
#include <limits>
#include <thread>

namespace LIBMEMBUS_NS
{

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

    bool calcLayout(int64_t ch, int64_t bps, int64_t bitrate, int64_t fps, int64_t bufs,
                    int64_t &blocksz, int64_t &total)
    {
        if (ch <= 0 || (bps != 8 && bps != 16) || bitrate <= 0 || fps <= 0 || bufs <= 0)
            return false;

        int64_t samplesPerFrame = bitrate / fps;
        int64_t sampleBytes = memaud::fitTo<int64_t>(bps, 8);
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
        int64_t bps     = *(int64_t*)(p + memaud::hv_bps);
        int64_t bitrate = *(int64_t*)(p + memaud::hv_bitrate);
        int64_t fps     = *(int64_t*)(p + memaud::hv_fps);
        int64_t bufs    = *(int64_t*)(p + memaud::hv_bufs);
        int64_t blocksz = *(int64_t*)(p + memaud::hv_blocksz);
        int64_t calcBlock = 0, calcTotal = 0;

        return calcLayout(ch, bps, bitrate, fps, bufs, calcBlock, calcTotal)
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

    int64_t *pSize = (int64_t*)(p + hv_size);
    int64_t *pPtr = (int64_t*)(p + hv_ptr);
    int64_t *pCh = (int64_t*)(p + hv_ch);
    int64_t *pBps = (int64_t*)(p + hv_bps);
    int64_t *pBufs = (int64_t*)(p + hv_bufs);
    int64_t *pBlockSz = (int64_t*)(p + hv_blocksz);

    // Wrap index with full modulo so any offset is valid
    if (0 >= *pBufs)
        throw std::exception();
    idx = ((idx % *pBufs) + *pBufs) % *pBufs;

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

    // Wrap index with full modulo so any offset is valid
    if (0 >= *pBufs)
        return false;
    idx = ((idx % *pBufs) + *pBufs) % *pBufs;

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

    if (!m_mem.open(sName, 0, false))
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


bool memaud::open(const std::string &sName, bool bCreate, int64_t ch, int64_t bps, int64_t bitrate, int64_t fps, int64_t bufs)
{
    close();

    int64_t blocksz = 0, total = 0;
    if (!calcLayout(ch, bps, bitrate, fps, bufs, blocksz, total))
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
        *(int64_t*)(p + hv_seq) = 0;
        *(int64_t*)(p + hv_id)  = (int64_t)std::mt19937_64(std::random_device{}())();
        *pCh = ch;
        *pBps = bps;
        *pBitRate = bitrate;
        *pFps = fps;
        *pBufs = bufs;
        *pBlockSz = blocksz;
    }

    // Exists, so makes sure it matches what we expect
    else if (!validateMappedLayout(p, m_mem.size())
             || *pSize != total
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
