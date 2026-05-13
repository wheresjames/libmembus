#pragma once

namespace LIBMEMBUS_NS
{

/** Multi-writer, multi-reader broadcast command channel.

    Any number of processes may write commands; any number may read them.
    Every reader receives every message independently (broadcast, not
    work-stealing).  Concurrent writers are serialised by an interprocess
    mutex; readers operate lock-free against each other using per-handle
    read positions.

    Typical use: consumer processes send control commands (pan, tilt, zoom)
    to the capture process.  The capture process creates the channel;
    consumers attach and write.

    @code
        // Capture process (creates the channel, receives commands)
        mmb::memcmd cmd;
        cmd.open("/cam_commands", 4096, true, true);   // bReader=true, bCreate=true
        while (running) {
            std::string c = cmd.read(100);
            if (c == "pan_left")  camera.pan(-1);
            if (c == "pan_stop")  camera.stop();
        }

        // Consumer process (attaches, sends commands)
        mmb::memcmd cmd;
        if (cmd.open("/cam_commands", 4096))            // defaults: bReader=false, bCreate=false
            cmd.write("pan_left");
    @endcode
*/
class memcmd
{
public:

    /// Constructor
    memcmd() : m_bReader(false), m_nRead(0), m_nLastSeq(-1) {}

    /// Destructor
    virtual ~memcmd() { close(); }

    /** Open or attach to a command channel.

        @param sName    Share name (POSIX: must start with /).
        @param size     Ring buffer capacity in bytes.
        @param bReader  Register this handle as a reader, incrementing the
                        shared reader count on open and decrementing it on
                        close().  Set true for the command-receiver side.
        @param bCreate  Create the share if it does not already exist.
                        The process that creates the share owns it and
                        removes it from the OS namespace on close().

        @returns Non-zero on success.
    */
    bool open(const std::string &sName, int64_t size,
              bool bReader = false, bool bCreate = false);

    /// Remove a stale command channel from the OS namespace.
    static bool remove(const std::string &sName) { return memmap::remove(sName); }

    /// Close the channel and release all resources.
    void close();

    /** Write a command.  Any open handle may call this regardless of
        whether it was opened with bReader=true or bReader=false.

        @returns Non-zero if the message was written.  Returns false if the
        payload is empty or larger than the buffer, or if the mutex could
        not be acquired within five seconds (writer crash recovery).
    */
    bool write(const std::string &sMsg);

    /** Read the next command for this handle.

        @param wait_ms   Maximum milliseconds to block waiting for a message.
                         Pass 0 for a non-blocking poll.
        @param pOverrun  If non-null, set to true when one or more messages
                         were skipped because a writer lapped this reader.
                         The read pointer is resynced to the current write
                         position; call read() again to receive the next
                         message written after the resync.

        @returns The command string, or an empty string on timeout or overrun.
    */
    std::string read(uint64_t wait_ms, bool *pOverrun = nullptr);

    /** Number of handles currently registered as readers (opened with
        bReader=true and not yet closed).  Treat as a hint: the count may
        be temporarily stale if a reader process crashed without calling
        close().
    */
    int64_t readerCount();

    /// Session ID written when the channel was created.
    int64_t getSessionId();

    /// Returns true if the channel is open.
    bool isOpen() { return m_mem.isOpen(); }

    /// Returns true if this handle attached to an already-existing share.
    bool existing() { return m_mem.existing(); }

private:

    /// Underlying memory map
    memmap  m_mem;

    /// True if this handle incremented the shared reader count on open
    bool    m_bReader;

    /// Per-handle read position (process-local, not in shared memory)
    int64_t m_nRead;

    /// Sequence number of the last message successfully read; -1 if none
    int64_t m_nLastSeq;
};

}; // end namespace
