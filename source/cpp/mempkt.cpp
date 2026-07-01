
#include "libmembus-internal.h"

#include <chrono>
#include <limits>
#include <thread>

namespace LIBMEMBUS_NS
{

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

    // Compute the descriptor-ring + arena layout.  arenasz is the (already
    // align-rounded) arena size.
    bool computeLayout(int64_t bufs, int64_t arenasz, int64_t align, int64_t metasize,
                       int64_t &descstride, int64_t &descoffset,
                       int64_t &arenaoffset, int64_t &total)
    {
        if (bufs <= 0 || arenasz <= 0 || !isPow2(align) || align < 8 || metasize < 0)
            return false;

        if (!alignUp(mempkt::dv_last, (int64_t)8, descstride))
            return false;

        int64_t mainuserBase = 0, descEnd = 0, descSpan = 0, descEndAlign = 0;
        if (!alignUp(mempkt::hv_last, (int64_t)8, mainuserBase)
            || !checkedAdd(mainuserBase, metasize, descEnd)
            || !alignUp(descEnd, (int64_t)8, descoffset)
            || !checkedMul(descstride, bufs, descSpan)
            || !checkedAdd(descoffset, descSpan, descEnd)
            || !alignUp(descEnd, align, descEndAlign)
            || !alignUp(descEndAlign, k_page, arenaoffset)
            || !checkedAdd(arenaoffset, arenasz, total))
            return false;

        return true;
    }

    bool validateMappedLayout(char *p, int64_t mappedSize)
    {
        if (!p || mappedSize < mempkt::hv_last)
            return false;

        if (*(int64_t*)(p + mempkt::hv_magic)   != mempkt::k_magic
            || *(int64_t*)(p + mempkt::hv_type) != mempkt::k_type
            || *(int64_t*)(p + mempkt::hv_version) != mempkt::k_version)
            return false;

        int64_t size        = *(int64_t*)(p + mempkt::hv_size);
        int64_t bufs        = *(int64_t*)(p + mempkt::hv_bufs);
        int64_t align       = *(int64_t*)(p + mempkt::hv_align);
        int64_t metasize    = *(int64_t*)(p + mempkt::hv_metasize);
        int64_t arenasz     = *(int64_t*)(p + mempkt::hv_arenasz);
        int64_t maxrec      = *(int64_t*)(p + mempkt::hv_maxrec);
        int64_t descstride  = *(int64_t*)(p + mempkt::hv_descstride);
        int64_t descoffset  = *(int64_t*)(p + mempkt::hv_descoffset);
        int64_t arenaoffset = *(int64_t*)(p + mempkt::hv_arenaoffset);

        int64_t cStride = 0, cDesc = 0, cArena = 0, cTotal = 0, maxStride = 0;
        return maxrec > 0 && arenasz > 0
            && computeLayout(bufs, arenasz, align, metasize, cStride, cDesc, cArena, cTotal)
            && descstride == cStride
            && descoffset == cDesc
            && arenaoffset == cArena
            && size == cTotal
            && size <= mappedSize
            && alignUp(maxrec, align, maxStride)
            && maxStride <= arenasz;
    }
}

int64_t mempkt::setPtr(int64_t ptr)
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

int64_t mempkt::getPtr(int64_t offset)
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

int64_t mempkt::getBufs()
{
    char *p = m_mem.data();
    if (!p) return 0;
    return *(int64_t*)(p + hv_bufs);
}

int64_t mempkt::getSeq()
{
    char *p = m_mem.data();
    if (!p) return -1;
    return std::atomic_ref<int64_t>(*(int64_t*)(p + hv_seq)).load(std::memory_order_acquire);
}

int64_t mempkt::getFrameSeq(int64_t idx)
{
    char *p = m_mem.data();
    if (!p) return -1;
    int64_t bufs = *(int64_t*)(p + hv_bufs);
    int64_t descstride = *(int64_t*)(p + hv_descstride);
    int64_t descoffset = *(int64_t*)(p + hv_descoffset);
    if (bufs <= 0 || descstride <= 0 || descoffset < hv_last) return -1;
    idx = ((idx % bufs) + bufs) % bufs;

    // Overflow-safe offset math (hv_descstride may have been corrupted post-open).
    int64_t slotOff = 0, off = 0;
    if (!checkedMul(idx, descstride, slotOff)
        || !checkedAdd(descoffset, slotOff, off)
        || !checkedAdd(off, dv_seq, off)
        || off < descoffset
        || off + (int64_t)sizeof(int64_t) > m_mem.size())
        return -1;
    return std::atomic_ref<int64_t>(*(int64_t*)(p + off)).load(std::memory_order_acquire);
}

bool mempkt::waitForFrame(uint64_t wait_ms, int64_t lastSeq)
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

int64_t mempkt::write(const void *payload, int64_t len, pkt_kind kind,
                      int64_t track, int64_t pts, const void *meta, int64_t metalen)
{
    char *p = m_mem.data();
    if (!p || len < 0 || metalen < 0)
        return -1;

    int64_t bufs        = *(int64_t*)(p + hv_bufs);
    int64_t arenasz     = *(int64_t*)(p + hv_arenasz);
    int64_t maxrec      = *(int64_t*)(p + hv_maxrec);
    int64_t align       = *(int64_t*)(p + hv_align);
    int64_t descstride  = *(int64_t*)(p + hv_descstride);
    int64_t descoffset  = *(int64_t*)(p + hv_descoffset);
    int64_t arenaoffset = *(int64_t*)(p + hv_arenaoffset);
    int64_t mapped      = m_mem.size();

    int64_t recordBytes = 0, stride = 0;
    if (bufs <= 0 || arenasz <= 0 || !isPow2(align) || descstride <= 0
        || descoffset < hv_last || arenaoffset < descoffset
        || !checkedAdd(len, metalen, recordBytes) || recordBytes <= 0)
        return -1;

    if (recordBytes > maxrec)
    {
        set_last_error(errc::message_too_large);
        return -1;
    }
    if (!alignUp(recordBytes, align, stride) || stride > arenasz)
        return -1;

    // Reserve arena space; never split a record across the wrap (pad to end).
    int64_t wc   = std::atomic_ref<int64_t>(*(int64_t*)(p + hv_wcursor)).load(std::memory_order_relaxed);
    int64_t phys = wc % arenasz;
    if (phys + stride > arenasz)
    {
        wc  += (arenasz - phys);   // pad to end; jump to next wrap boundary
        phys = 0;
    }
    int64_t start = wc;

    char *arena = p + arenaoffset;
    if (arenaoffset + phys + recordBytes > mapped)
        return -1;

    // Copy metadata then payload (metadata precedes payload within the record).
    if (metalen > 0 && meta)    memcpy(arena + phys, meta, metalen);
    if (len > 0 && payload)     memcpy(arena + phys + metalen, payload, len);

    int64_t cur = std::atomic_ref<int64_t>(*(int64_t*)(p + hv_ptr)).load(std::memory_order_acquire);
    char *d = p + descoffset + cur * descstride;
    *(int64_t*)(d + dv_offset)  = phys;
    *(int64_t*)(d + dv_len)     = len;
    *(int64_t*)(d + dv_wcursor) = start;
    *(int64_t*)(d + dv_pts)     = pts;
    *(int64_t*)(d + dv_kind)    = (int64_t)kind;
    *(int64_t*)(d + dv_track)   = track;
    *(int64_t*)(d + dv_userlen) = metalen;

    // Publish the arena bytes (release) before anyone can observe the new cursor.
    std::atomic_ref<int64_t>(*(int64_t*)(p + hv_wcursor)).store(start + stride, std::memory_order_release);

    // Stamp the descriptor sequence last (release), then advance the write pointer.
    int64_t seq = std::atomic_ref<int64_t>(*(int64_t*)(p + hv_seq))
                      .fetch_add(1, std::memory_order_relaxed) + 1;
    std::atomic_ref<int64_t>(*(int64_t*)(d + dv_seq)).store(seq, std::memory_order_release);

    return setPtr(cur + 1);
}

bool mempkt::getRecord(int64_t idx, std::string &payload, std::string &meta, recinfo &info)
{
    char *p = m_mem.data();
    if (!p) return false;

    int64_t bufs        = *(int64_t*)(p + hv_bufs);
    int64_t arenasz     = *(int64_t*)(p + hv_arenasz);
    int64_t descstride  = *(int64_t*)(p + hv_descstride);
    int64_t descoffset  = *(int64_t*)(p + hv_descoffset);
    int64_t arenaoffset = *(int64_t*)(p + hv_arenaoffset);
    int64_t mapped      = m_mem.size();
    if (bufs <= 0 || arenasz <= 0 || descstride <= 0 || descoffset < hv_last)
        return false;

    idx = ((idx % bufs) + bufs) % bufs;

    // Overflow-safe descriptor address (hv_descstride may have been corrupted).
    int64_t slotOff = 0, dbase = 0;
    if (!checkedMul(idx, descstride, slotOff)
        || !checkedAdd(descoffset, slotOff, dbase)
        || dbase < descoffset
        || dbase + dv_last > mapped)
        return false;
    char *d = p + dbase;

    // Seqlock read of the descriptor: fields must be bracketed by a stable,
    // non-zero dv_seq (else the writer is rewriting this slot).
    int64_t offset = 0, len = 0, wc = 0, userlen = 0, seq1 = 0;
    bool stable = false;
    for (int attempt = 0; attempt < 8; ++attempt)
    {
        seq1 = std::atomic_ref<int64_t>(*(int64_t*)(d + dv_seq)).load(std::memory_order_acquire);
        if (seq1 <= 0) return false;   // never published
        offset      = *(int64_t*)(d + dv_offset);
        len         = *(int64_t*)(d + dv_len);
        wc          = *(int64_t*)(d + dv_wcursor);
        info.pts    = *(int64_t*)(d + dv_pts);
        info.kind   = *(int64_t*)(d + dv_kind);
        info.track  = *(int64_t*)(d + dv_track);
        userlen     = *(int64_t*)(d + dv_userlen);
        int64_t seq2 = std::atomic_ref<int64_t>(*(int64_t*)(d + dv_seq)).load(std::memory_order_acquire);
        if (seq1 == seq2) { stable = true; break; }
    }
    if (!stable) return false;

    // Bounds-check the record within the arena (records never wrap).  All adds
    // are overflow-checked in case arenasz / offsets were corrupted post-open.
    int64_t recEnd = 0, absEnd = 0;
    if (offset < 0 || len < 0 || userlen < 0
        || !checkedAdd(userlen, len, recEnd)
        || !checkedAdd(offset, recEnd, recEnd)
        || recEnd > arenasz
        || arenaoffset < hv_last
        || !checkedAdd(arenaoffset, offset, absEnd)
        || !checkedAdd(absEnd, userlen, absEnd)
        || !checkedAdd(absEnd, len, absEnd)
        || absEnd > mapped)
        return false;

    // Copy metadata + payload out, then re-check the write cursor.
    char *arena = p + arenaoffset;
    meta.assign(arena + offset, userlen);
    payload.assign(arena + offset + userlen, len);

    int64_t now = std::atomic_ref<int64_t>(*(int64_t*)(p + hv_wcursor)).load(std::memory_order_acquire);
    if (now - wc > arenasz)
        return false;   // writer lapped these bytes mid-copy

    info.seq = seq1;
    info.wcursor = wc;
    info.len = len;
    info.userlen = userlen;
    return true;
}

int64_t mempkt::getArenaSize()
{
    char *p = m_mem.data();
    if (!p) return 0;
    return *(int64_t*)(p + hv_arenasz);
}

int64_t mempkt::getMaxRec()
{
    char *p = m_mem.data();
    if (!p) return 0;
    return *(int64_t*)(p + hv_maxrec);
}

int64_t mempkt::getWcursor()
{
    char *p = m_mem.data();
    if (!p) return 0;
    return std::atomic_ref<int64_t>(*(int64_t*)(p + hv_wcursor)).load(std::memory_order_acquire);
}

int64_t mempkt::getSessionId()
{
    char *p = m_mem.data();
    if (!p) return 0;
    return *(int64_t*)(p + hv_id);
}

int64_t mempkt::getVersion()
{
    char *p = m_mem.data();
    if (!p) return -1;
    return *(int64_t*)(p + hv_version);
}

uint32_t mempkt::getFourcc()
{
    char *p = m_mem.data();
    if (!p) return 0;
    return (uint32_t)*(int64_t*)(p + hv_fourcc);
}

bool mempkt::getGuid(uint8_t out[16])
{
    char *p = m_mem.data();
    if (!p) return false;
    memcpy(out, p + hv_guid_lo, 16);
    int64_t lo = *(int64_t*)(p + hv_guid_lo), hi = *(int64_t*)(p + hv_guid_hi);
    return (lo | hi) != 0;
}

const char *mempkt::getMeta()
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

int64_t mempkt::getMetaSize()
{
    char *p = m_mem.data();
    if (!p) return 0;
    return *(int64_t*)(p + hv_metasize);
}

void mempkt::close()
{
    m_mem.close();
}

bool mempkt::open_existing(const std::string &sName)
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

bool mempkt::open(const std::string &sName, bool bCreate,
                  int64_t bufs, int64_t arenasz, int64_t maxrec,
                  int64_t align, uint32_t fourcc, const uint8_t *guid,
                  const void *meta, int64_t metasz)
{
    close();

    if (bufs <= 0 || arenasz <= 0 || maxrec <= 0 || metasz < 0)
        return false;

    if (align <= 0) align = k_defAlign;
    if (!isPow2(align) || align < 8)
        return false;

    // Round the arena up to an alignment multiple so wrap boundaries stay aligned.
    int64_t arenaRounded = 0, maxStride = 0;
    if (!alignUp(arenasz, align, arenaRounded)
        || !alignUp(maxrec, align, maxStride)
        || maxStride > arenaRounded)   // a single record must always fit
        return false;

    int64_t descstride = 0, descoffset = 0, arenaoffset = 0, total = 0;
    if (!computeLayout(bufs, arenaRounded, align, metasz,
                      descstride, descoffset, arenaoffset, total))
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
        *(int64_t*)(p + hv_magic)       = k_magic;
        *(int64_t*)(p + hv_type)        = k_type;
        *(int64_t*)(p + hv_version)     = k_version;
        *(int64_t*)(p + hv_size)        = total;
        *(int64_t*)(p + hv_ptr)         = 0;
        *(int64_t*)(p + hv_seq)         = 0;
        *(int64_t*)(p + hv_id)          = (int64_t)std::mt19937_64(std::random_device{}())();
        *(int64_t*)(p + hv_bufs)        = bufs;
        *(int64_t*)(p + hv_fourcc)      = (int64_t)fourcc;
        *(int64_t*)(p + hv_guid_lo)     = 0;
        *(int64_t*)(p + hv_guid_hi)     = 0;
        if (guid) memcpy(p + hv_guid_lo, guid, 16);
        *(int64_t*)(p + hv_align)       = align;
        *(int64_t*)(p + hv_metasize)    = metasz;
        for (int i = 0; i < 8; ++i) *(int64_t*)(p + hv_reserved0 + i * sizeof(int64_t)) = 0;
        *(int64_t*)(p + hv_arenasz)     = arenaRounded;
        *(int64_t*)(p + hv_maxrec)      = maxrec;
        *(int64_t*)(p + hv_wcursor)     = 0;
        *(int64_t*)(p + hv_descstride)  = descstride;
        *(int64_t*)(p + hv_descoffset)  = descoffset;
        *(int64_t*)(p + hv_arenaoffset) = arenaoffset;

        // Descriptor sequences start at 0 (never-published) — zero the ring.
        memset(p + descoffset, 0, descstride * bufs);

        if (metasz > 0 && meta)
        {
            int64_t base = 0;
            if (alignUp(hv_last, (int64_t)8, base) && base + metasz <= total)
                memcpy(p + base, meta, metasz);
        }
    }
    else if (!validateMappedLayout(p, m_mem.size())
             || *(int64_t*)(p + hv_size)    != total
             || *(int64_t*)(p + hv_bufs)    != bufs
             || *(int64_t*)(p + hv_arenasz) != arenaRounded
             || *(int64_t*)(p + hv_maxrec)  != maxrec)
    {
        close();
        return false;
    }

    return true;
}

}; // end namespace
