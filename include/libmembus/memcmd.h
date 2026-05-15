#pragma once

namespace LIBMEMBUS_NS
{

/** Multi-producer, multi-consumer broadcast command channel.
 *
 *  Any number of processes may write commands; any number may read them.
 *  Every reader receives every command independently (broadcast, not
 *  work-stealing).  Concurrent writers are serialised by an interprocess
 *  mutex; readers operate lock-free against each other using per-handle
 *  read positions.
 *
 *  Typical use: consumer processes send control commands (pan, tilt, zoom)
 *  to the capture process.  The capture process creates the channel;
 *  consumers attach and write.
 *
 *  @code
 *      // Capture process (creates the channel, receives commands)
 *      mmb::memcmd cmd;
 *      cmd.open("/cam_commands", 4096, true, true);   // bReader=true, bCreate=true
 *      while (running) {
 *          std::string c = cmd.read(100);
 *          if (c == "pan_left")  camera.pan(-1);
 *          if (c == "pan_stop")  camera.stop();
 *      }
 *
 *      // Consumer process (attaches, sends commands)
 *      mmb::memcmd cmd;
 *      if (cmd.open("/cam_commands", 4096))            // defaults: bReader=false, bCreate=false
 *          cmd.write("pan_left");
 *  @endcode
 */
class memcmd
{
public:

    /// Construct an un-opened handle.
    memcmd() : m_bReader(false), m_nRead(0), m_nLastSeq(-1) {}

    /// Destructor; calls close().
    virtual ~memcmd() { close(); }

    /** Open or attach to a command channel.
     *
     *  Both sides must specify the same @p size or the attach will fail with
     *  errc::size_mismatch.  Attach also fails when the existing share is too
     *  small to hold the minimum channel layout.
     *
     *  Frame records are padded to 8-byte alignment; shares created by older
     *  library versions are not wire-compatible.
     *
     *  @param sName    Share name (POSIX: must start with '/').
     *  @param size     Ring buffer capacity in bytes.
     *  @param bReader  Register this handle as a reader, incrementing the shared
     *                  reader count on open and decrementing it on close().  Set
     *                  true for the command-receiver side.
     *  @param bCreate  Create the share if it does not already exist.  The process
     *                  that creates the share owns it and removes it on close().
     *  @returns true on success; false on failure (see last_error()).
     */
    bool open(const std::string &sName, int64_t size,
              bool bReader = false, bool bCreate = false);

    /** Remove a stale command channel from the OS namespace.
     *  @param sName  Share name to remove.
     *  @returns true if the object was removed.
     */
    static bool remove(const std::string &sName) { return memmap::remove(sName); }

    /** Close the channel and release all resources.
     *
     *  If this handle was opened with bReader=true, the shared reader count is
     *  decremented atomically before the mapping is released.
     */
    void close();

    /** Write a command.
     *
     *  Any open handle may call this regardless of whether it was opened with
     *  bReader=true or bReader=false.  Frame records are padded to 8-byte
     *  alignment.
     *
     *  @param sMsg  Command payload; must be non-empty and shorter than the
     *               ring buffer capacity.
     *  @returns true on success.  Returns false if the payload is empty
     *           (errc::invalid_argument), too large (errc::message_too_large),
     *           not open (errc::not_open), or the mutex could not be acquired
     *           within 5 seconds (errc::lock_timeout).
     */
    bool write(const std::string &sMsg);

    /** Read the next command for this handle.
     *
     *  @param wait_ms   Maximum milliseconds to block waiting for a command.
     *                   Pass 0 for a non-blocking poll.
     *  @param pOverrun  If non-null, set to true when one or more commands were
     *                   skipped because a writer lapped this reader.  The read
     *                   position is resynced to the current write position; call
     *                   read() again to receive the next command written after
     *                   the resync.
     *  @returns The command string, or an empty string on timeout (errc::timeout)
     *           or overrun (errc::overrun).
     */
    std::string read(uint64_t wait_ms, bool *pOverrun = nullptr);

    /** Return the number of handles currently registered as readers.
     *
     *  Treat as a hint: the count may be temporarily stale if a reader process
     *  crashed without calling close().
     *  @returns Current reader count, or 0 if not open.
     */
    int64_t readerCount();

    /** Return the random session ID written when the channel was created.
     *
     *  Readers should save this on open and compare periodically.  A change
     *  means the creator restarted and this handle must be closed and reopened.
     *  @returns Session ID, or 0 if not open.
     */
    int64_t getSessionId();

    /// Returns true if the channel is currently open.
    bool isOpen() { return m_mem.isOpen(); }

    /// Returns true if this handle attached to an already-existing share.
    bool existing() { return m_mem.existing(); }

private:

    /// The underlying memory map.
    memmap  m_mem;

    /// True if this handle incremented the shared reader count on open.
    bool    m_bReader;

    /// Per-handle read position in bytes (process-local, not in shared memory).
    int64_t m_nRead;

    /// Sequence number of the last command successfully read; -1 if none yet.
    int64_t m_nLastSeq;
};

}; // end namespace
