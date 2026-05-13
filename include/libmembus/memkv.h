#pragma once

#include <map>

namespace LIBMEMBUS_NS
{

/** Fixed-schema key-value store in shared memory.
 *
 *  One process creates the store and defines the schema (slot count, max
 *  name length, max value length).  Any number of processes may then open
 *  it and read or write individual entries.
 *
 *  Writes are serialised by an interprocess mutex.  Reads use a per-slot
 *  seqlock so getValue() never acquires any lock.
 *
 *  @code
 *      // Owner process
 *      mmb::memkv kv;
 *      kv.create("/cam_state", 4, 31, 63);
 *      kv.setName(0, "pan");   kv.setValue(0, "0");
 *      kv.setName(1, "tilt");  kv.setValue(1, "0");
 *      kv.setName(2, "zoom");  kv.setValue(2, "1.0");
 *      kv.setName(3, "focus"); kv.setValue(3, "auto");
 *
 *      // Any other process
 *      mmb::memkv kv;
 *      kv.open("/cam_state");
 *      int64_t epoch = kv.getEpoch();
 *      while (running) {
 *          auto changed = kv.getChanged(100, epoch);
 *          for (auto &[k, v] : changed) process(k, v);
 *      }
 *  @endcode
 */
class memkv
{
public:

    memkv();
    virtual ~memkv();

    /** Create a new key-value store.  The caller becomes the owner and
     *  removes the share on close().  Populate names via setName() before
     *  other processes call open().
     *  @param bNew  Remove any existing share by this name first. */
    bool create(const std::string &sName,
                int64_t count, int64_t maxNameLen, int64_t maxValueLen,
                bool bNew = false);

    /** Attach to an existing share without taking ownership.
     *  Returns false if no share exists. */
    bool open(const std::string &sName);

    void close();

    /** Set the immutable name for slot @p idx.
     *  Should only be called by the creator before publishing the share. */
    bool setName(int64_t idx, const std::string &name);

    /** Write a value by slot index.  Any open handle may call this. */
    bool setValue(int64_t idx, const std::string &value);

    /** Write a value by name. */
    bool setValue(const std::string &name, const std::string &value);

    /** Write every entry in the map under one mutex acquisition.
     *  All slots update atomically; one epoch increment; one notify_all.
     *  Names not present in the store are silently skipped. */
    bool setAll(const std::map<std::string, std::string> &values);

    /** Read a value by slot index.  Lock-free (seqlock).
     *  @param pStale  Set true when the seqlock is stuck after 1000 retries
     *                 (writer likely crashed mid-write); value may be torn. */
    std::string getValue(int64_t idx, bool *pStale = nullptr);

    /** Read a value by name. */
    std::string getValue(const std::string &name, bool *pStale = nullptr);

    /** Read all entries into a consistent snapshot.
     *  Retries internally until a full pass completes with no concurrent
     *  write (epoch-stable), so all values reflect the same generation. */
    std::map<std::string, std::string> getAll();

    /** Return entries whose value changed since @p epoch (non-blocking).
     *  Updates @p epoch to the current value.  Returns an empty map if
     *  nothing changed. */
    std::map<std::string, std::string> getChanged(int64_t &epoch);

    /** Block until any value changes or @p wait_ms elapses, then return
     *  all entries that changed since @p epoch.
     *  Updates @p epoch on return.  Empty map means timeout with no change. */
    std::map<std::string, std::string> getChanged(uint64_t wait_ms, int64_t &epoch);

    /** Block until the epoch advances or @p wait_ms elapses.
     *  Updates @p epoch on return.  Returns true if a change occurred. */
    bool waitForChange(uint64_t wait_ms, int64_t &epoch);

    /** Total number of setValue / setAll calls since creation. */
    int64_t getEpoch();

    /** Linear scan for slot index by name.  Returns -1 if not found. */
    int64_t findName(const std::string &name);

    std::string getName(int64_t idx);
    int64_t count();
    int64_t maxNameLen();
    int64_t maxValueLen();
    bool isOpen()   { return m_mem.isOpen(); }
    bool existing() { return m_mem.existing(); }

private:

    memmap  m_mem;
    int64_t m_slotStride  = 0;
    int64_t m_maxNameLen  = 0;
    int64_t m_maxValueLen = 0;

    // Pointer helpers — defined in .cpp where hv_last is visible
    int64_t *seqAt      (char *p, int64_t idx) const;
    int64_t *lastEpochAt(char *p, int64_t idx) const;
    char    *nameAt     (char *p, int64_t idx) const;
    char    *valueAt    (char *p, int64_t idx) const;

    void writeSlotLocked(char *p, int64_t idx,
                         const std::string &value, int64_t newEpoch);

    std::map<std::string, std::string> collectChangedSince(char *p, int64_t sinceEpoch);
};

}; // end namespace
