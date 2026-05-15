#pragma once

#include <map>

namespace LIBMEMBUS_NS
{

/** Fixed-schema key-value store in shared memory.
 *
 *  One process creates the store and defines the immutable schema (slot count,
 *  maximum name length, maximum value length).  Any number of processes may
 *  then open it and read or write individual entries.
 *
 *  Writes are serialised by an interprocess mutex.  Reads use a per-slot
 *  seqlock, so getValue() never acquires any lock.
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

    /// Construct an un-opened handle.
    memkv();

    /// Destructor; calls close().
    virtual ~memkv();

    /** Create a new key-value store.
     *
     *  The caller becomes the owner and removes the share on close().
     *  Populate names with setName() before other processes call open().
     *
     *  @param sName       Share name (POSIX: must start with '/').
     *  @param count       Number of key-value slots (immutable for the lifetime
     *                     of the share).
     *  @param maxNameLen  Maximum bytes per slot name, excluding the null
     *                     terminator (immutable).
     *  @param maxValueLen Maximum bytes per slot value, excluding the null
     *                     terminator (immutable).
     *  @param bNew        Remove any existing share by this name first.
     *  @returns true on success; false on failure (see last_error()).
     */
    bool create(const std::string &sName,
                int64_t count, int64_t maxNameLen, int64_t maxValueLen,
                bool bNew = false);

    /** Remove a stale key-value store from the OS namespace.
     *  @param sName  Share name to remove.
     *  @returns true if the object was removed.
     */
    static bool remove(const std::string &sName) { return memmap::remove(sName); }

    /** Attach to an existing store without taking ownership.
     *
     *  Reads the schema (count, name/value limits) from the shared header and
     *  validates the layout.
     *
     *  @param sName  Share name (POSIX: must start with '/').
     *  @returns false if the share does not exist or has an invalid schema
     *           (errc::open_failed / errc::invalid_layout).
     */
    bool open(const std::string &sName);

    /// Close the store and release all resources.
    void close();

    /** Set the immutable name for slot @p idx.
     *
     *  Should only be called by the creator before publishing the share to
     *  other processes.  The name is null-padded to fit maxNameLen bytes.
     *
     *  @param idx   Slot index (0-based, must be < count()).
     *  @param name  Name string; must be <= maxNameLen bytes.
     *  @returns true on success; false if the handle is not open, idx is out of
     *           range, or the name is too long.
     */
    bool setName(int64_t idx, const std::string &name);

    /** Write a value by slot index.
     *
     *  Acquires the shared mutex, increments the epoch, updates the seqlock
     *  and value, then notifies waiters.  Any open handle may call this.
     *
     *  @param idx    Slot index (0-based, must be < count()).
     *  @param value  Value string; must be <= maxValueLen bytes.
     *  @returns true on success.  Returns false if not open, idx is out of range
     *           (errc::invalid_argument), value is too long
     *           (errc::message_too_large), or the mutex timed out
     *           (errc::lock_timeout).
     */
    bool setValue(int64_t idx, const std::string &value);

    /** Write a value by name.
     *
     *  Performs a linear scan with findName() to locate the slot, then calls
     *  setValue(idx, value).
     *
     *  @param name   Slot name to look up.
     *  @param value  Value string; must be <= maxValueLen bytes.
     *  @returns true on success; false if the name is not found or setValue fails.
     */
    bool setValue(const std::string &name, const std::string &value);

    /** Write every entry in @p values under one mutex acquisition.
     *
     *  All matching slots are updated atomically; one epoch increment is issued
     *  and one notify_all is fired.  Names not present in the store are silently
     *  skipped.  If no names match (or @p values is empty) the epoch is not
     *  incremented and no wakeup is issued.
     *
     *  @param values  Map of name → value pairs to write.
     *  @returns true on success; false if the mutex timed out (errc::lock_timeout).
     */
    bool setAll(const std::map<std::string, std::string> &values);

    /** Read a value by slot index.  Lock-free (seqlock).
     *
     *  Retries internally until two consecutive reads of the slot's sequence
     *  counter agree (no concurrent write in progress).  Caps retries at 1000
     *  to handle a writer crash mid-write.
     *
     *  @param idx     Slot index (0-based, must be < count()).
     *  @param pStale  If non-null, set to true when the seqlock did not settle
     *                 within 1000 retries (writer may have crashed mid-write);
     *                 the returned value may be torn.
     *  @returns The current value string; empty if the slot has never been written,
     *           the handle is not open, or idx is out of range.
     */
    std::string getValue(int64_t idx, bool *pStale = nullptr);

    /** Read a value by name.  Lock-free (seqlock).
     *
     *  Performs a linear scan with findName() to locate the slot, then calls
     *  getValue(idx, pStale).
     *
     *  @param name    Slot name to look up.
     *  @param pStale  Forwarded to the underlying getValue(idx) call.
     *  @returns The current value string, or empty if the name is not found.
     */
    std::string getValue(const std::string &name, bool *pStale = nullptr);

    /** Read all entries into a consistent epoch-stable snapshot.
     *
     *  Retries until a full pass over all slots completes with no concurrent
     *  write (the epoch does not change between the start and end of the pass).
     *  Capped at 100 attempts to prevent livelock under sustained write pressure;
     *  returns the best-effort snapshot if the cap is reached.
     *
     *  @returns Map of all slot names to their current values.
     */
    std::map<std::string, std::string> getAll();

    /** Return entries whose value changed since @p epoch (non-blocking).
     *
     *  Compares each slot's lastEpoch field against @p epoch and collects those
     *  that are strictly greater.  Updates @p epoch to the current value on return.
     *
     *  @param [in,out] epoch  On entry: the epoch after which changes are of interest.
     *                         On exit: the current epoch value.
     *  @returns Map of changed slot names to their current values; empty if nothing
     *           changed.
     */
    std::map<std::string, std::string> getChanged(int64_t &epoch);

    /** Block until any value changes or @p wait_ms elapses, then return all
     *  changed entries since @p epoch.
     *
     *  @param wait_ms          Maximum milliseconds to block.
     *  @param [in,out] epoch   On entry: the epoch after which changes are of interest.
     *                          On exit: the current epoch value.
     *  @returns Map of changed slot names to their current values; empty on timeout.
     */
    std::map<std::string, std::string> getChanged(uint64_t wait_ms, int64_t &epoch);

    /** Block until the epoch advances or @p wait_ms elapses.
     *
     *  @param wait_ms          Maximum milliseconds to block.
     *  @param [in,out] epoch   On entry: the epoch value to wait beyond.
     *                          On exit: the current epoch value.
     *  @returns true if the epoch advanced within the timeout; false on timeout.
     */
    bool waitForChange(uint64_t wait_ms, int64_t &epoch);

    /** Return the total number of setValue / setAll calls since creation.
     *
     *  Each setValue increments the epoch by 1.  Each setAll that writes at least
     *  one slot increments it by 1 regardless of how many slots were updated.
     *  @returns Current epoch value; -1 if not open.
     */
    int64_t getEpoch();

    /** Return the random session ID written when the store was created.
     *
     *  Readers should save this on open and compare periodically.  A change
     *  means the owner restarted and this handle must be closed and reopened.
     *  @returns Session ID, or 0 if not open.
     */
    int64_t getSessionId();

    /** Search for a slot by name (linear scan).
     *  @param name  Name to search for.
     *  @returns Zero-based slot index, or -1 if not found.
     */
    int64_t findName(const std::string &name);

    /** Return the name of slot @p idx.
     *  @param idx  Slot index (0-based, must be < count()).
     *  @returns The slot name string; empty if idx is out of range or not open.
     */
    std::string getName(int64_t idx);

    /** Return the total number of slots in the store.
     *  @returns Slot count from the header; 0 if not open.
     */
    int64_t count();

    /** Return the maximum name length in bytes (excluding null terminator).
     *  @returns Maximum name length from the header; 0 if not open.
     */
    int64_t maxNameLen();

    /** Return the maximum value length in bytes (excluding null terminator).
     *  @returns Maximum value length from the header; 0 if not open.
     */
    int64_t maxValueLen();

    /// Returns true if the store is currently open.
    bool isOpen()   { return m_mem.isOpen(); }

    /// Returns true if this handle attached to an already-existing share.
    bool existing() { return m_mem.existing(); }

private:

    /// The underlying memory map.
    memmap  m_mem;

    /// Byte stride between consecutive slot records.
    int64_t m_slotStride  = 0;

    /// Cached maximum name length (excluding null) from the header.
    int64_t m_maxNameLen  = 0;

    /// Cached maximum value length (excluding null) from the header.
    int64_t m_maxValueLen = 0;

    // Pointer helpers — defined in .cpp where hv_last is visible.
    int64_t *seqAt      (char *p, int64_t idx) const;
    int64_t *lastEpochAt(char *p, int64_t idx) const;
    char    *nameAt     (char *p, int64_t idx) const;
    char    *valueAt    (char *p, int64_t idx) const;

    void writeSlotLocked(char *p, int64_t idx,
                         const std::string &value, int64_t newEpoch);

    std::map<std::string, std::string> collectChangedSince(char *p, int64_t sinceEpoch);
};

}; // end namespace
