
#include "libmembus-internal.h"

#include <limits>

namespace LIBMEMBUS_NS
{

namespace
{
    enum HeaderVal
    {
        hv_count       = 0,
        hv_maxNameLen  = 1 * sizeof(int64_t),
        hv_maxValueLen = 2 * sizeof(int64_t),
        hv_epoch       = 3 * sizeof(int64_t),
        hv_mutex       = 4 * sizeof(int64_t),
        hv_cond        = hv_mutex + sizeof(interprocess_mutex),
        hv_last        = hv_cond  + sizeof(interprocess_condition)
    };

    // Per-slot field offsets from slot start
    const int64_t sv_seq       = 0;
    const int64_t sv_lastEpoch = sizeof(int64_t);
    const int64_t sv_name      = 2 * sizeof(int64_t);
    // value follows name: sv_name + maxNameLen + 1

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

    bool calcLayout(int64_t count, int64_t maxNameLen, int64_t maxValueLen,
                    int64_t &slotStride, int64_t &totalSize)
    {
        if (count <= 0 || maxNameLen <= 0 || maxValueLen <= 0)
            return false;

        int64_t nameBytes = 0, valueBytes = 0, dataBytes = 0, slotsBytes = 0;
        if (!checkedAdd(maxNameLen, 1, nameBytes)
            || !checkedAdd(maxValueLen, 1, valueBytes)
            || !checkedAdd(2 * (int64_t)sizeof(int64_t), nameBytes, dataBytes)
            || !checkedAdd(dataBytes, valueBytes, slotStride)
            || !checkedMul(count, slotStride, slotsBytes)
            || !checkedAdd(hv_last, slotsBytes, totalSize))
            return false;

        return true;
    }
}


// ── layout helpers ──────────────────────────────────────────────────────────

int64_t *memkv::seqAt(char *p, int64_t idx) const
{
    return (int64_t*)(p + hv_last + idx * m_slotStride + sv_seq);
}
int64_t *memkv::lastEpochAt(char *p, int64_t idx) const
{
    return (int64_t*)(p + hv_last + idx * m_slotStride + sv_lastEpoch);
}
char *memkv::nameAt(char *p, int64_t idx) const
{
    return p + hv_last + idx * m_slotStride + sv_name;
}
char *memkv::valueAt(char *p, int64_t idx) const
{
    return p + hv_last + idx * m_slotStride + sv_name + m_maxNameLen + 1;
}


// ── lifecycle ────────────────────────────────────────────────────────────────

memkv::memkv() {}
memkv::~memkv() { close(); }

void memkv::close()
{
    m_mem.close();
    m_slotStride  = 0;
    m_maxNameLen  = 0;
    m_maxValueLen = 0;
}

bool memkv::create(const std::string &sName,
                   int64_t nCount, int64_t maxNameLen, int64_t maxValueLen,
                   bool bNew)
{
    close();

    int64_t slotStride = 0, totalSize = 0;
    if (!calcLayout(nCount, maxNameLen, maxValueLen, slotStride, totalSize))
        return false;

    if (!m_mem.open(sName, totalSize, true, bNew))
        return false;

    char *p = m_mem.data();
    if (!p) { close(); return false; }

    if (!m_mem.existing())
    {
        *(int64_t*)(p + hv_count)      = nCount;
        *(int64_t*)(p + hv_maxNameLen)  = maxNameLen;
        *(int64_t*)(p + hv_maxValueLen) = maxValueLen;
        *(int64_t*)(p + hv_epoch)       = 0;
        new ((interprocess_mutex*)    (p + hv_mutex)) interprocess_mutex();
        new ((interprocess_condition*)(p + hv_cond))  interprocess_condition();
    }
    else if (*(int64_t*)(p + hv_count)      != nCount
             || *(int64_t*)(p + hv_maxNameLen)  != maxNameLen
             || *(int64_t*)(p + hv_maxValueLen) != maxValueLen
             || totalSize > m_mem.size())
    {
        std::cout << "memkv::create: schema mismatch on existing share" << std::endl;
        close();
        return false;
    }

    m_maxNameLen  = maxNameLen;
    m_maxValueLen = maxValueLen;
    m_slotStride  = slotStride;
    return true;
}

bool memkv::open(const std::string &sName)
{
    close();

    if (!m_mem.open(sName, 0, false))
        return false;

    char *p = m_mem.data();
    if (!p) { close(); return false; }

    int64_t nCount      = *(int64_t*)(p + hv_count);
    int64_t maxNameLen  = *(int64_t*)(p + hv_maxNameLen);
    int64_t maxValueLen = *(int64_t*)(p + hv_maxValueLen);

    int64_t slotStride = 0, totalSize = 0;
    if (!calcLayout(nCount, maxNameLen, maxValueLen, slotStride, totalSize)
        || totalSize > m_mem.size())
    {
        std::cout << "memkv::open: invalid schema" << std::endl;
        close();
        return false;
    }

    m_maxNameLen  = maxNameLen;
    m_maxValueLen = maxValueLen;
    m_slotStride  = slotStride;
    return true;
}


// ── schema ───────────────────────────────────────────────────────────────────

bool memkv::setName(int64_t idx, const std::string &name)
{
    char *p = m_mem.data();
    if (!p) return false;

    if (idx < 0 || idx >= *(int64_t*)(p + hv_count)) return false;
    if ((int64_t)name.length() > m_maxNameLen) return false;

    char *pName = nameAt(p, idx);
    int64_t len = (int64_t)name.length();
    memcpy(pName, name.c_str(), len);
    memset(pName + len, '\0', m_maxNameLen + 1 - len);
    return true;
}


// ── write ────────────────────────────────────────────────────────────────────

void memkv::writeSlotLocked(char *p, int64_t idx,
                             const std::string &value, int64_t newEpoch)
{
    int64_t *pSeq       = seqAt(p, idx);
    int64_t *pLastEpoch = lastEpochAt(p, idx);
    char    *pVal       = valueAt(p, idx);

    // Stamp write in progress (make counter odd)
    std::atomic_ref<int64_t>(*pSeq).fetch_add(1, std::memory_order_relaxed);
    std::atomic_thread_fence(std::memory_order_release);

    int64_t len = std::min((int64_t)value.length(), m_maxValueLen);
    memcpy(pVal, value.c_str(), len);
    memset(pVal + len, '\0', m_maxValueLen + 1 - len);
    *pLastEpoch = newEpoch;

    // Unstamp (make counter even)
    std::atomic_thread_fence(std::memory_order_release);
    std::atomic_ref<int64_t>(*pSeq).fetch_add(1, std::memory_order_relaxed);
}

bool memkv::setValue(int64_t idx, const std::string &value)
{
    char *p = m_mem.data();
    if (!p) return false;

    if (idx < 0 || idx >= *(int64_t*)(p + hv_count)) return false;
    if ((int64_t)value.length() > m_maxValueLen) return false;

    auto *pMutex  = (interprocess_mutex*)(p + hv_mutex);
    auto *pCond   = (interprocess_condition*)(p + hv_cond);
    int64_t *pEpoch = (int64_t*)(p + hv_epoch);

    scoped_lock<interprocess_mutex> lk(*pMutex,
        boost::get_system_time() + boost::posix_time::milliseconds(5000));
    if (!lk) return false;

    int64_t newEpoch = *pEpoch + 1;
    writeSlotLocked(p, idx, value, newEpoch);
    *pEpoch = newEpoch;
    pCond->notify_all();
    return true;
}

bool memkv::setValue(const std::string &name, const std::string &value)
{
    int64_t idx = findName(name);
    return (idx >= 0) && setValue(idx, value);
}

bool memkv::setAll(const std::map<std::string, std::string> &values)
{
    char *p = m_mem.data();
    if (!p) return false;

    auto *pMutex  = (interprocess_mutex*)(p + hv_mutex);
    auto *pCond   = (interprocess_condition*)(p + hv_cond);
    int64_t *pEpoch = (int64_t*)(p + hv_epoch);

    scoped_lock<interprocess_mutex> lk(*pMutex,
        boost::get_system_time() + boost::posix_time::milliseconds(5000));
    if (!lk) return false;

    int64_t newEpoch = *pEpoch + 1;

    for (auto &[name, value] : values)
    {
        int64_t idx = findName(name);
        if (idx < 0) continue;
        if ((int64_t)value.length() > m_maxValueLen) continue;
        writeSlotLocked(p, idx, value, newEpoch);
    }

    *pEpoch = newEpoch;
    pCond->notify_all();
    return true;
}


// ── read ─────────────────────────────────────────────────────────────────────

std::string memkv::getValue(int64_t idx, bool *pStale)
{
    if (pStale) *pStale = false;

    char *p = m_mem.data();
    if (!p) { if (pStale) *pStale = true; return {}; }

    if (idx < 0 || idx >= *(int64_t*)(p + hv_count))
    {   if (pStale) *pStale = true; return {}; }

    int64_t *pSeq = seqAt(p, idx);
    char    *pVal = valueAt(p, idx);

    std::string result(m_maxValueLen + 1, '\0');

    for (int retry = 0; retry < 1000; retry++)
    {
        int64_t seq1 = std::atomic_ref<int64_t>(*pSeq).load(std::memory_order_acquire);
        if (seq1 & 1) continue;  // write in progress

        memcpy(result.data(), pVal, m_maxValueLen + 1);
        std::atomic_thread_fence(std::memory_order_acquire);
        int64_t seq2 = std::atomic_ref<int64_t>(*pSeq).load(std::memory_order_acquire);

        if (seq1 == seq2)
        {
            result.resize(strnlen(result.data(), m_maxValueLen));
            return result;
        }
    }

    // Seqlock stuck — writer likely crashed mid-write
    if (pStale) *pStale = true;
    result.resize(strnlen(result.data(), m_maxValueLen));
    return result;
}

std::string memkv::getValue(const std::string &name, bool *pStale)
{
    int64_t idx = findName(name);
    if (idx < 0) { if (pStale) *pStale = true; return {}; }
    return getValue(idx, pStale);
}

std::map<std::string, std::string> memkv::getAll()
{
    char *p = m_mem.data();
    if (!p) return {};

    int64_t cnt     = *(int64_t*)(p + hv_count);
    int64_t *pEpoch = (int64_t*)(p + hv_epoch);

    std::map<std::string, std::string> result;
    int64_t e1, e2;

    do
    {
        e1 = std::atomic_ref<int64_t>(*pEpoch).load(std::memory_order_acquire);
        result.clear();
        for (int64_t i = 0; i < cnt; i++)
            result[getName(i)] = getValue(i);
        e2 = std::atomic_ref<int64_t>(*pEpoch).load(std::memory_order_acquire);
    } while (e1 != e2);

    return result;
}


// ── change detection ─────────────────────────────────────────────────────────

std::map<std::string, std::string> memkv::collectChangedSince(char *p, int64_t sinceEpoch)
{
    std::map<std::string, std::string> result;
    if (!p) return result;

    int64_t cnt = *(int64_t*)(p + hv_count);

    for (int64_t i = 0; i < cnt; i++)
    {
        int64_t slotEpoch = std::atomic_ref<int64_t>(
            *lastEpochAt(p, i)).load(std::memory_order_acquire);

        if (slotEpoch > sinceEpoch)
            result[getName(i)] = getValue(i);
    }

    return result;
}

std::map<std::string, std::string> memkv::getChanged(int64_t &epoch)
{
    char *p = m_mem.data();
    if (!p) return {};

    int64_t sinceEpoch = epoch;
    epoch = std::atomic_ref<int64_t>(*(int64_t*)(p + hv_epoch))
                .load(std::memory_order_acquire);

    return collectChangedSince(p, sinceEpoch);
}

std::map<std::string, std::string> memkv::getChanged(uint64_t wait_ms, int64_t &epoch)
{
    int64_t sinceEpoch = epoch;
    waitForChange(wait_ms, epoch);
    return collectChangedSince(m_mem.data(), sinceEpoch);
}

bool memkv::waitForChange(uint64_t wait_ms, int64_t &epoch)
{
    char *p = m_mem.data();
    if (!p) return false;

    int64_t  sinceEpoch = epoch;
    int64_t *pEpoch     = (int64_t*)(p + hv_epoch);
    auto    *pMutex     = (interprocess_mutex*)(p + hv_mutex);
    auto    *pCond      = (interprocess_condition*)(p + hv_cond);

    scoped_lock<interprocess_mutex> lk(*pMutex,
        boost::get_system_time() + boost::posix_time::milliseconds(5000));
    if (!lk)
    {
        epoch = std::atomic_ref<int64_t>(*pEpoch).load(std::memory_order_acquire);
        return false;
    }

    if (*pEpoch > sinceEpoch) { epoch = *pEpoch; return true; }
    if (0 >= wait_ms)         { epoch = *pEpoch; return false; }

    pCond->timed_wait(lk,
        boost::get_system_time() + boost::posix_time::milliseconds(wait_ms));

    epoch = *pEpoch;
    return (*pEpoch > sinceEpoch);
}

int64_t memkv::getEpoch()
{
    char *p = m_mem.data();
    if (!p) return -1;
    return std::atomic_ref<int64_t>(*(int64_t*)(p + hv_epoch))
               .load(std::memory_order_acquire);
}


// ── lookup / metadata ────────────────────────────────────────────────────────

int64_t memkv::findName(const std::string &name)
{
    char *p = m_mem.data();
    if (!p) return -1;

    int64_t cnt = *(int64_t*)(p + hv_count);
    for (int64_t i = 0; i < cnt; i++)
        if (strncmp(nameAt(p, i), name.c_str(), m_maxNameLen) == 0)
            return i;

    return -1;
}

std::string memkv::getName(int64_t idx)
{
    char *p = m_mem.data();
    if (!p) return {};

    if (idx < 0 || idx >= *(int64_t*)(p + hv_count)) return {};

    char *pName = nameAt(p, idx);
    return std::string(pName, strnlen(pName, m_maxNameLen));
}

int64_t memkv::count()
{
    char *p = m_mem.data(); return p ? *(int64_t*)(p + hv_count)      : 0;
}
int64_t memkv::maxNameLen()
{
    char *p = m_mem.data(); return p ? *(int64_t*)(p + hv_maxNameLen)  : 0;
}
int64_t memkv::maxValueLen()
{
    char *p = m_mem.data(); return p ? *(int64_t*)(p + hv_maxValueLen) : 0;
}

}; // end namespace
