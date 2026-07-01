#pragma once

namespace LIBMEMBUS_NS
{

/** Convenience wrapper: creates and writes to a single-producer message queue.
 *
 *  Combines the common pattern of removing any stale share, creating a fresh
 *  one, and writing messages into a single thin class.  Use memmsg directly
 *  when you need finer control (e.g. bNew=false).
 */
class memmsg_writer
{
public:

    /** Create a message queue, optionally removing any pre-existing share first.
     *  @param name   Share name (POSIX: must start with '/').
     *  @param size   Logical ring-buffer capacity in bytes.
     *  @param bNew   Remove any existing share by this name before creating.
     *                Defaults to true so each open starts with a clean ring.
     *  @returns true on success.
     */
    bool open(const std::string &name, int64_t size, bool bNew = true)
    {
        if (bNew)
            memmsg::remove(name);
        return m_q.open(name, size, true, true);
    }

    /** Write a message into the queue.
     *  @param msg  Message payload.
     *  @returns true on success; false if not open or payload is invalid.
     */
    bool write(const std::string &msg) { return m_q.write(msg); }

    /// Close the queue and release all resources.
    void close() { m_q.close(); }

    /** Return the session ID written when the queue was created.
     *  @returns Session ID, or 0 if not open.
     */
    int64_t getSessionId() { return m_q.getSessionId(); }

    /// Return a reference to the underlying memmsg object.
    memmsg &raw() { return m_q; }

private:
    memmsg m_q;
};


/** Convenience wrapper: attaches to and reads from a message queue.
 *
 *  Opens a pre-existing queue in read-only mode (bWrite=false).
 */
class memmsg_reader
{
public:

    /** Attach to an existing message queue.
     *  @param name  Share name (POSIX: must start with '/').
     *  @param size  Logical ring-buffer capacity in bytes; must match the writer.
     *  @returns true on success.
     */
    bool open(const std::string &name, int64_t size)
    { return m_q.open(name, size, false, false); }

    /** Read the next message.
     *  @param wait_ms   Maximum milliseconds to block; 0 for non-blocking.
     *  @param pOverrun  If non-null, set to true when the reader was lapped.
     *  @returns The message string, or empty on timeout or overrun.
     */
    std::string read(uint64_t wait_ms, bool *pOverrun = nullptr)
    { return m_q.read(wait_ms, pOverrun); }

    /// Close the queue and release all resources.
    void close() { m_q.close(); }

    /** Return the session ID written when the queue was created.
     *  @returns Session ID, or 0 if not open.
     */
    int64_t getSessionId() { return m_q.getSessionId(); }

    /// Return a reference to the underlying memmsg object.
    memmsg &raw() { return m_q; }

private:
    memmsg m_q;
};


/** Convenience wrapper: attaches to and writes commands on a command channel.
 *
 *  Opens the channel without registering as a reader (bReader=false).
 */
class memcmd_sender
{
public:

    /** Attach to a command channel.
     *  @param name     Share name (POSIX: must start with '/').
     *  @param size     Ring buffer capacity in bytes; must match the receiver.
     *  @param bCreate  Create the channel if it does not exist yet.
     *  @returns true on success.
     */
    bool open(const std::string &name, int64_t size, bool bCreate = false)
    { return m_cmd.open(name, size, false, bCreate); }

    /** Write a command.
     *  @param cmd  Command payload.
     *  @returns true on success; false if not open or the payload is invalid.
     */
    bool write(const std::string &cmd) { return m_cmd.write(cmd); }

    /// Close the channel and release all resources.
    void close() { m_cmd.close(); }

    /** Return the session ID written when the channel was created.
     *  @returns Session ID, or 0 if not open.
     */
    int64_t getSessionId() { return m_cmd.getSessionId(); }

    /// Return a reference to the underlying memcmd object.
    memcmd &raw() { return m_cmd; }

private:
    memcmd m_cmd;
};


/** Convenience wrapper: creates (or attaches to) a command channel and reads commands.
 *
 *  Registers this handle as a reader (bReader=true) so it is counted in
 *  readerCount() and receives all commands independently.
 */
class memcmd_receiver
{
public:

    /** Create or attach to a command channel, registering as a reader.
     *
     *  When bCreate=true and bNew=true any existing share is removed first.
     *
     *  @param name     Share name (POSIX: must start with '/').
     *  @param size     Ring buffer capacity in bytes.
     *  @param bCreate  Create the channel if it does not exist.  Defaults to true
     *                  since the receiver typically owns the channel.
     *  @param bNew     Remove any pre-existing share before creating.  Defaults to
     *                  true so each startup begins with a clean channel.
     *  @returns true on success.
     */
    bool open(const std::string &name, int64_t size, bool bCreate = true, bool bNew = true)
    {
        if (bCreate && bNew)
            memcmd::remove(name);
        return m_cmd.open(name, size, true, bCreate);
    }

    /** Read the next command.
     *  @param wait_ms   Maximum milliseconds to block; 0 for non-blocking.
     *  @param pOverrun  If non-null, set to true when the reader was lapped.
     *  @returns The command string, or empty on timeout or overrun.
     */
    std::string read(uint64_t wait_ms, bool *pOverrun = nullptr)
    { return m_cmd.read(wait_ms, pOverrun); }

    /** Return the number of handles currently registered as readers.
     *  @returns Current reader count.
     */
    int64_t readerCount() { return m_cmd.readerCount(); }

    /// Close the channel and release all resources.
    void close() { m_cmd.close(); }

    /** Return the session ID written when the channel was created.
     *  @returns Session ID, or 0 if not open.
     */
    int64_t getSessionId() { return m_cmd.getSessionId(); }

    /// Return a reference to the underlying memcmd object.
    memcmd &raw() { return m_cmd; }

private:
    memcmd m_cmd;
};


/** Convenience wrapper: creates a video ring buffer and publishes frames.
 *
 *  Calls open() with bCreate=true; any existing share with the same name is
 *  removed and recreated.
 */
class memvid_writer
{
public:

    /** Create a video ring buffer.
     *  @param name  Share name (POSIX: must start with '/').
     *  @param w     Frame width in pixels.
     *  @param h     Frame height in pixels.
     *  @param fmt   Pixel format.
     *  @param fps   Nominal frames per second.
     *  @param bufs  Number of frame slots in the ring.
     *  @returns true on success.
     */
    bool open(const std::string &name, int64_t w, int64_t h,
              video_format fmt, int64_t fps, int64_t bufs)
    { return m_vid.open(name, true, w, h, fmt, fps, bufs); }

    /** Fill a frame slot with a constant byte value.
     *  @param idx  Slot index; wrapped modulo getBufs().
     *  @param col  Byte fill value.
     *  @returns true on success.
     */
    bool fill(int64_t idx, int col) { return m_vid.fill(idx, col); }

    /** Write the video presentation timestamp for the current write slot.
     *  @param pts  Timestamp value; call before next().
     *  @returns true on success.
     */
    bool setVpts(int64_t pts) { return m_vid.setVpts(m_vid.getPtr(0), pts); }

    /** Write the audio presentation timestamp for the current write slot.
     *  @param pts  Timestamp value; call before next().
     *  @returns true on success.
     */
    bool setApts(int64_t pts) { return m_vid.setApts(m_vid.getPtr(0), pts); }

    /** Stamp the current slot's sequence number and advance the write pointer.
     *  @param inc  Slots to advance (normally 1).
     *  @returns The new write-pointer slot index.
     */
    int64_t next(int64_t inc = 1) { return m_vid.next(inc); }

    /** Return the current write-pointer slot index offset by @p offset.
     *  @param offset  Signed offset from the write pointer (default 0).
     *  @returns Wrapped slot index.
     */
    int64_t getPtr(int64_t offset = 0) { return m_vid.getPtr(offset); }

    /// Close the ring buffer and release all resources.
    void close() { m_vid.close(); }

    /// Return a reference to the underlying memvid object.
    memvid &raw() { return m_vid; }

private:
    memvid m_vid;
};


/** Convenience wrapper: attaches to an existing video ring buffer and tracks
 *  the read position with overrun detection.
 *
 *  Calls open_existing(); the segment is mapped read-only.
 */
class memvid_reader
{
public:

    /** Attach to an existing video ring buffer.
     *
     *  Calls open_existing() and then resync() so the reader starts at the
     *  current write position and does not report stale frames as new ones.
     *
     *  @param name  Share name (POSIX: must start with '/').
     *  @returns true on success; false if the share does not exist or its header
     *           is invalid.
     */
    bool open(const std::string &name)
    {
        if (!m_vid.open_existing(name))
            return false;
        resync();
        return true;
    }

    /** Poll until a new frame is available or @p wait_ms elapses.
     *  @param wait_ms  Maximum milliseconds to block.
     *  @returns true if a new frame arrived; false on timeout.
     */
    bool wait(uint64_t wait_ms) { return m_vid.waitForFrame(wait_ms, m_lastSeq); }

    /** Return the next available frame view and advance the internal read position.
     *
     *  On overrun @p pOverrun is set to true and the returned view must not be
     *  used — it points at the slot the writer is actively filling.  Call wait()
     *  and readNext() again to receive the next complete frame.
     *
     *  @param pOverrun  If non-null, set to true when the writer lapped this reader.
     *  @returns A vidview over the current frame's pixel data.
     */
    memvid::vidview readNext(bool *pOverrun = nullptr)
    {
        if (pOverrun) *pOverrun = false;

        int64_t seq = m_vid.getSeq();
        if (seq - m_lastSeq >= m_vid.getBufs())
        {
            resync();
            if (pOverrun) *pOverrun = true;
            set_last_error(errc::overrun);
            return m_vid.getBuf(m_pos);
        }

        memvid::vidview view = m_vid.getBuf(m_pos);
        m_lastSeq = m_vid.getFrameSeq(m_pos);
        m_lastVpts = m_vid.getVpts(m_pos);
        m_lastApts = m_vid.getApts(m_pos);
        m_pos = (m_pos + 1) % m_vid.getBufs();
        set_last_error(errc::ok);
        return view;
    }

    /** Return the video presentation timestamp of the last frame returned by readNext().
     *  @returns vpts value, or 0 if readNext() has not been called yet.
     */
    int64_t lastVpts() const { return m_lastVpts; }

    /** Return the audio presentation timestamp of the last frame returned by readNext().
     *  @returns apts value, or 0 if readNext() has not been called yet.
     */
    int64_t lastApts() const { return m_lastApts; }

    /** Resync the read position to the current write position.
     *
     *  Call this after an overrun, or after re-opening to avoid processing stale
     *  frames.  Sets m_lastSeq to getSeq() and m_pos to getPtr(0).
     */
    void resync()
    {
        m_lastSeq = m_vid.getSeq();
        m_pos = m_vid.getPtr(0);
    }

    /// Close the ring buffer and release all resources.
    void close() { m_vid.close(); }

    /// Return a reference to the underlying memvid object.
    memvid &raw() { return m_vid; }

private:
    memvid m_vid;

    /// Sequence number of the last frame successfully delivered to the caller.
    int64_t m_lastSeq = 0;

    /// Next slot index to read.
    int64_t m_pos = 0;

    /// Video presentation timestamp of the last delivered frame.
    int64_t m_lastVpts = 0;

    /// Audio presentation timestamp of the last delivered frame.
    int64_t m_lastApts = 0;
};


/** Convenience wrapper: creates an audio ring buffer and publishes PCM buffers.
 *
 *  Calls open() with bCreate=true; any existing share with the same name is
 *  removed and recreated.
 */
class memaud_writer
{
public:

    /** Create an audio ring buffer.
     *
     *  Buffer slot size is ceil(sampleRate / fps) * ch * bytesPerSample for
     *  known PCM formats, or @p payloadSize for audio_format::userType.
     *
     *  @param name        Share name (POSIX: must start with '/').
     *  @param ch          Number of interleaved channels.
     *  @param fmt         Sample format.
     *  @param sampleRate  Sample rate in Hz.
     *  @param fps         Buffers per second.
     *  @param bufs        Number of buffer slots in the ring.
     *  @param payloadSize Per-slot payload bytes. Required for audio_format::userType.
     *  @returns true on success.
     */
    bool open(const std::string &name, int64_t ch, audio_format fmt,
              int64_t sampleRate, int64_t fps, int64_t bufs,
              int64_t payloadSize = 0)
    { return m_aud.open(name, true, ch, fmt, sampleRate, fps, bufs,
                        0, 0, 0, nullptr, nullptr, 0, payloadSize); }

    /** Fill a buffer slot with a constant byte value (e.g. 0 for silence).
     *  @param idx  Slot index; wrapped modulo getBufs().
     *  @param col  Byte fill value.
     *  @returns true on success.
     */
    bool fill(int64_t idx, int col) { return m_aud.fill(idx, col); }

    /** Write the presentation timestamp for the current write slot.
     *  @param pts  Timestamp value; call before next().
     *  @returns true on success.
     */
    bool setPts(int64_t pts) { return m_aud.setPts(m_aud.getPtr(0), pts); }

    /** Stamp the current slot's sequence number and advance the write pointer.
     *  @param inc  Slots to advance (normally 1).
     *  @returns The new write-pointer slot index.
     */
    int64_t next(int64_t inc = 1) { return m_aud.next(inc); }

    /** Return the current write-pointer slot index offset by @p offset.
     *  @param offset  Signed offset from the write pointer (default 0).
     *  @returns Wrapped slot index.
     */
    int64_t getPtr(int64_t offset = 0) { return m_aud.getPtr(offset); }

    /// Close the ring buffer and release all resources.
    void close() { m_aud.close(); }

    /// Return a reference to the underlying memaud object.
    memaud &raw() { return m_aud; }

private:
    memaud m_aud;
};


/** Convenience wrapper: attaches to an existing audio ring buffer and tracks
 *  the read position with overrun detection.
 *
 *  Calls open_existing(); the segment is mapped read-only.
 */
class memaud_reader
{
public:

    /** Attach to an existing audio ring buffer.
     *
     *  Calls open_existing() and then resync() so the reader starts at the
     *  current write position and does not report stale buffers as new ones.
     *
     *  @param name  Share name (POSIX: must start with '/').
     *  @returns true on success; false if the share does not exist or its header
     *           is invalid.
     */
    bool open(const std::string &name)
    {
        if (!m_aud.open_existing(name))
            return false;
        resync();
        return true;
    }

    /** Poll until a new buffer is available or @p wait_ms elapses.
     *  @param wait_ms  Maximum milliseconds to block.
     *  @returns true if a new buffer arrived; false on timeout.
     */
    bool wait(uint64_t wait_ms) { return m_aud.waitForFrame(wait_ms, m_lastSeq); }

    /** Return the next available buffer view and advance the internal read position.
     *
     *  On overrun @p pOverrun is set to true and the returned view must not be
     *  used — it points at the slot the writer is actively filling.  Call wait()
     *  and readNext() again to receive the next complete buffer.
     *
     *  @param pOverrun  If non-null, set to true when the writer lapped this reader.
     *  @returns An audview over the current buffer's PCM data.
     */
    memaud::audview readNext(bool *pOverrun = nullptr)
    {
        if (pOverrun) *pOverrun = false;

        int64_t seq = m_aud.getSeq();
        if (seq - m_lastSeq >= m_aud.getBufs())
        {
            resync();
            if (pOverrun) *pOverrun = true;
            set_last_error(errc::overrun);
            return m_aud.getBuf(m_pos);
        }

        memaud::audview view = m_aud.getBuf(m_pos);
        m_lastSeq = m_aud.getFrameSeq(m_pos);
        m_lastPts = m_aud.getPts(m_pos);
        m_pos = (m_pos + 1) % m_aud.getBufs();
        set_last_error(errc::ok);
        return view;
    }

    /** Return the presentation timestamp of the last buffer returned by readNext().
     *  @returns pts value, or 0 if readNext() has not been called yet.
     */
    int64_t lastPts() const { return m_lastPts; }

    /** Resync the read position to the current write position.
     *
     *  Call this after an overrun, or after re-opening to avoid processing stale
     *  buffers.  Sets m_lastSeq to getSeq() and m_pos to getPtr(0).
     */
    void resync()
    {
        m_lastSeq = m_aud.getSeq();
        m_pos = m_aud.getPtr(0);
    }

    /// Close the ring buffer and release all resources.
    void close() { m_aud.close(); }

    /// Return a reference to the underlying memaud object.
    memaud &raw() { return m_aud; }

private:
    memaud m_aud;

    /// Sequence number of the last buffer successfully delivered to the caller.
    int64_t m_lastSeq = 0;

    /// Next slot index to read.
    int64_t m_pos = 0;

    /// Presentation timestamp of the last delivered buffer.
    int64_t m_lastPts = 0;
};


/** Convenience wrapper: creates a variable-length record ring and publishes records.
 *
 *  Calls open() with bCreate=true; any existing share with the same name is
 *  removed and recreated.
 */
class mempkt_writer
{
public:

    /** Create a record ring.
     *  @param name     Share name (POSIX: must start with '/').
     *  @param bufs     Number of descriptor slots.
     *  @param arenasz  Payload arena size in bytes (size with headroom; see MB-MEMPKT.md §6.1).
     *  @param maxrec   Largest single record (payload + metadata) accepted.
     *  @param align    Record alignment (power of two, >= 8); 0 = default 64.
     *  @param fourcc   Fourcc identity (0 = none).
     *  @param guid     Optional 16-byte GUID identity.
     *  @param meta     Optional main user buffer bytes.
     *  @param metasz   Size of @p meta in bytes.
     *  @returns true on success.
     */
    bool open(const std::string &name, int64_t bufs, int64_t arenasz, int64_t maxrec,
              int64_t align = 0, uint32_t fourcc = 0, const uint8_t *guid = nullptr,
              const void *meta = nullptr, int64_t metasz = 0)
    {
        mempkt::remove(name);
        return m_pkt.open(name, true, bufs, arenasz, maxrec, align, fourcc, guid, meta, metasz);
    }

    /** Publish a record.  @returns the new descriptor write-pointer slot, or -1. */
    int64_t write(const void *payload, int64_t len,
                  pkt_kind kind = pkt_kind::data, int64_t track = 0, int64_t pts = 0,
                  const void *meta = nullptr, int64_t metalen = 0)
    { return m_pkt.write(payload, len, kind, track, pts, meta, metalen); }

    /** Publish a std::string payload. */
    int64_t write(const std::string &payload,
                  pkt_kind kind = pkt_kind::data, int64_t track = 0, int64_t pts = 0)
    { return m_pkt.write(payload, kind, track, pts); }

    /// Close the ring and release all resources.
    void close() { m_pkt.close(); }

    /** Return the session ID written when the ring was created. */
    int64_t getSessionId() { return m_pkt.getSessionId(); }

    /// Return a reference to the underlying mempkt object.
    mempkt &raw() { return m_pkt; }

private:
    mempkt m_pkt;
};


/** Convenience wrapper: attaches to an existing record ring and tracks the read
 *  position with overrun detection.
 *
 *  Calls open_existing(); the segment is mapped read-only.
 */
class mempkt_reader
{
public:

    /** Attach to an existing record ring and resync to the write position. */
    bool open(const std::string &name)
    {
        if (!m_pkt.open_existing(name))
            return false;
        resync();
        return true;
    }

    /** Poll until a new record is available or @p wait_ms elapses. */
    bool wait(uint64_t wait_ms) { return m_pkt.waitForFrame(wait_ms, m_lastSeq); }

    /** Copy the next available record out and advance the read position.
     *
     *  On overrun (the writer lapped this reader, or the record was overwritten
     *  mid-copy) @p pOverrun is set true, the read position is resynced, and
     *  false is returned — @p payload / @p meta must not be used.
     *
     *  @param payload   Receives the payload bytes on success.
     *  @param meta      Receives the per-record metadata bytes on success.
     *  @param info      Receives the record metadata (seq, pts, kind, …).
     *  @param pOverrun  If non-null, set true on overrun.
     *  @returns true on a clean read; false on overrun/timeout/no-data.
     */
    bool readNext(std::string &payload, std::string &meta, mempkt::recinfo &info,
                  bool *pOverrun = nullptr)
    {
        if (pOverrun) *pOverrun = false;

        int64_t bufs = m_pkt.getBufs();
        if (bufs <= 0)
            return false;

        int64_t seq = m_pkt.getSeq();
        if (seq - m_lastSeq >= bufs)
        {
            resync();
            if (pOverrun) *pOverrun = true;
            set_last_error(errc::overrun);
            return false;
        }

        if (!m_pkt.getRecord(m_pos, payload, meta, info))
        {
            // Descriptor in flight or bytes lapped mid-copy — treat as overrun.
            resync();
            if (pOverrun) *pOverrun = true;
            set_last_error(errc::overrun);
            return false;
        }

        m_lastSeq = info.seq;
        m_lastPts = info.pts;
        m_pos = (m_pos + 1) % bufs;
        set_last_error(errc::ok);
        return true;
    }

    /** Presentation timestamp of the last record returned by readNext(). */
    int64_t lastPts() const { return m_lastPts; }

    /** Resync the read position to the current write position. */
    void resync()
    {
        m_lastSeq = m_pkt.getSeq();
        m_pos = m_pkt.getPtr(0);
    }

    /// Close the ring and release all resources.
    void close() { m_pkt.close(); }

    /// Return a reference to the underlying mempkt object.
    mempkt &raw() { return m_pkt; }

private:
    mempkt m_pkt;

    /// Sequence number of the last record successfully delivered to the caller.
    int64_t m_lastSeq = 0;

    /// Next slot index to read.
    int64_t m_pos = 0;

    /// Presentation timestamp of the last delivered record.
    int64_t m_lastPts = 0;
};

}; // end namespace
