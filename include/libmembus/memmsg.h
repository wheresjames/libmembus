#pragma once


namespace LIBMEMBUS_NS
{

/** Single-producer, multi-consumer broadcast message queue in shared memory.
 *
 *  One process opens with bWrite=true and calls write() to publish messages.
 *  Any number of independent reader processes open with bWrite=false; each
 *  maintains its own private read position so all readers receive all messages
 *  independently (broadcast, not work-stealing).
 *
 *  Messages are framed with a length and a monotonically increasing sequence
 *  number.  A reader that falls so far behind that the writer has wrapped the
 *  ring and overwritten unread frames will detect the gap via the sequence
 *  numbers, resync, and surface errc::overrun.
 *
 *  @code
 *      mmb::memmsg tx, rx;
 *      tx.open("/mymsg", 128, true, true);   // writer, create
 *      rx.open("/mymsg", 128, false, false); // reader, attach
 *
 *      tx.write("hello");
 *
 *      bool overrun = false;
 *      std::string msg = rx.read(100, &overrun);
 *  @endcode
 */
class memmsg
{

public:

    /// Construct an un-opened handle.
    memmsg() : m_bWrite(false), m_nRead(0), m_nLastSeq(-1) {};

    /// Destructor; calls close().
    virtual ~memmsg() { close(); }

    /** Open or attach to a message queue.
     *
     *  Both sides must specify the same @p size or the attach will fail with
     *  errc::size_mismatch.  Attach also fails when the existing share is too
     *  small to hold the minimum queue layout.
     *
     *  @param sName    Share name (POSIX: must start with '/').
     *  @param size     Logical ring-buffer capacity in bytes.
     *  @param bWrite   Open for writing.  Pass false to open as a read-only
     *                  consumer; write() will then return false with errc::access_denied.
     *  @param bCreate  Create the share if it does not already exist.
     *  @returns true on success; false on failure (see last_error()).
     */
    bool open(const std::string &sName, int64_t size, bool bWrite, bool bCreate);

    /** Remove a stale message queue from the OS namespace.
     *  @param sName  Share name to remove.
     *  @returns true if the object was removed.
     */
    static bool remove(const std::string &sName) { return memmap::remove(sName); }

    /// Close the queue and release all resources.
    void close();

    /** Write a message into the ring buffer.
     *
     *  The message is framed with a length and an incrementing sequence number
     *  before being written.  Frame records are padded to 8-byte alignment.
     *
     *  @param sMsg  Message payload; must be non-empty and shorter than the
     *               logical ring-buffer capacity.
     *  @returns true on success.  Returns false if the handle was not opened for
     *           writing (errc::access_denied), the payload is empty
     *           (errc::invalid_argument), too large (errc::message_too_large),
     *           or the mutex timed out (errc::lock_timeout).
     */
    bool write(const std::string &sMsg);

    /** Read the next message from the ring buffer.
     *
     *  Acquires the shared mutex for the duration of the read so that the
     *  writer cannot wrap the buffer underneath this call.
     *
     *  @param wait      Maximum milliseconds to block waiting for a message.
     *                   Pass 0 for a non-blocking poll.
     *  @param pOverrun  If non-null, set to true when one or more messages were
     *                   skipped because the writer lapped this reader.  The read
     *                   position is resynced to the current write position; call
     *                   read() again to receive the next message written after
     *                   the resync.
     *  @returns The message string, or an empty string on timeout (errc::timeout)
     *           or overrun (errc::overrun).
     */
    std::string read(uint64_t wait, bool *pOverrun = nullptr);

    /** Non-blocking check: returns true if at least one message is waiting.
     *
     *  Reads the write pointer without acquiring the mutex, so the result may
     *  be momentarily stale.  False wakeups (spurious true) and missed wakeups
     *  (spurious false) are both possible but rare and acceptable in
     *  mmb::select() polling loops.
     *
     *  @returns true if m_nRead differs from the current write position.
     */
    bool poll();

    /** Returns true if this handle attached to an already-existing share.
     *  @returns false if this handle created the queue; true if it attached.
     */
    bool existing() { return m_mem.existing(); }

    /// Returns true if the queue is currently open.
    bool isOpen() { return m_mem.isOpen(); }

    /** Returns the random session ID written when the queue was created.
     *
     *  Readers should save this on open and compare periodically.  A change
     *  indicates the writer restarted and this handle must be closed and reopened.
     *  @returns Session ID, or 0 if the queue is not open.
     */
    int64_t getSessionId();

private:

    /// The underlying memory map.
    memmap      m_mem;

    /// True if this handle was opened with bWrite=true.
    bool        m_bWrite;

    /// Byte offset of the next frame to read within the ring buffer data area.
    int64_t     m_nRead;

    /// Sequence number of the last message successfully read; -1 if none yet.
    int64_t     m_nLastSeq;

};

}; // end namespace
