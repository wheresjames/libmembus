#pragma once

namespace LIBMEMBUS_NS
{

/** PCM sample formats stored in the audio ring buffer header.
 *
 *  The format value is written as an int64_t at byte offset hv_format in the
 *  shared-memory header so that readers can discover the format without knowing
 *  it in advance.  All multi-byte formats are little-endian.
 */
enum class audio_format : int64_t
{
    u8    = 1, ///< Unsigned 8-bit; 1 byte per sample.
    s16le,     ///< Signed 16-bit little-endian; 2 bytes per sample.
    s24le,     ///< Signed 24-bit little-endian; 3 bytes per sample.
    s32le,     ///< Signed 32-bit little-endian; 4 bytes per sample.
    f32le,     ///< 32-bit IEEE-754 float little-endian; 4 bytes per sample.
    f64le      ///< 64-bit IEEE-754 double little-endian; 8 bytes per sample.
};

/** Return the name string for an audio_format enum value (e.g. "S16LE").
 *  @param fmt  Format to name.
 *  @returns Pointer to a static null-terminated string; "unknown" for unrecognised values.
 */
const char *audio_format_name(audio_format fmt);

/** Return the number of bytes per sample for one channel of an audio_format.
 *  @param fmt  Format to query.
 *  @returns Bytes per sample, or 0 for unrecognised values.
 */
int64_t audio_format_bytes_per_sample(audio_format fmt);

/** Lock-free multi-buffer PCM audio ring buffer in shared memory.
 *
 *  Follows the same design as memvid: the writer creates the share and
 *  calls next() after filling each buffer slot; readers attach with
 *  open_existing() and consume slots independently.
 *
 *  Buffer size: each slot holds ceil(sampleRate / fps) samples per channel.
 *  Ceiling division ensures the buffer is never short even when the rates do
 *  not divide evenly, preventing long-running clock drift.
 *
 *  Overrun detection: compare getSeq() - rLastSeq against getBufs().  When
 *  the difference is >= getBufs() the writer has lapped the reader.
 */
class memaud
{
public:

    /** Round @p val up to the nearest multiple of @p unit.
     *  @param val   Value to round.
     *  @param unit  Multiple to round up to; returns 0 if unit is 0.
     *  @returns Smallest multiple of unit that is >= val.
     */
    template < typename T >
        static T fitTo( T val, T unit )
    {	if ( !unit ) return 0;
        T i = (T)( val / unit );
        return ( ( i * unit ) >= val ) ? i : i + 1;
    }

    /** Return the absolute value of @p v.
     *  @param v  Input value.
     *  @returns Non-negative value with the same magnitude as v.
     */
    template < typename T >
        static T abs( T v )
    {   return ( v >= 0 ) ? v : -v; }

    /** Read-only view of a single buffer slot in the audio ring.
     *
     *  Returned by getBuf().  The pointer is valid only while the underlying
     *  memaud handle is open.  On a read-only mapping (open_existing()) writing
     *  through m_ptr is undefined behaviour.
     */
    class audview
    {
    public:

        /** Construct a view over a raw PCM buffer.
         *  @param p    Pointer to the first sample byte.
         *  @param sz   Total buffer size in bytes.
         *  @param ch   Number of channels.
         *  @param fmt  Sample format.
         */
        audview(char* p, int64_t sz, int64_t ch, audio_format fmt)
            : m_ptr(p), m_size(sz), m_ch(ch), m_format(fmt)
        {
        }

        /** Copy constructor.
         *  @param r  View to copy.
         */
        audview(const audview &r)
        {
            (*this) = r;
        }

        /// Destructor.
        virtual ~audview() {}

        /** Copy assignment.
         *  @param r  View to copy.
         *  @returns Reference to this.
         */
        audview& operator = (const audview &r)
        {
            m_ptr = r.m_ptr;
            m_size = r.m_size;
            m_ch = r.m_ch;
            m_format = r.m_format;

            return *this;
        }

    public:

        /// Pointer to the first sample byte of the buffer.
        char        *m_ptr;

        /// Total PCM data size in bytes.
        int64_t     m_size;

        /// Number of interleaved channels.
        int64_t     m_ch;

        /// Sample format of this buffer.
        audio_format m_format;
    };

    /** Byte offsets of fields in the shared-memory main header.
     *
     *  These constants are public so that callers can inspect or corrupt-test
     *  header fields via a raw memmap handle.  Normal usage goes through the
     *  typed accessors (getChannels(), getSeq(), etc.).
     */
    enum HeaderVal
    {
        hv_size         = 0,                     ///< int64_t: total allocated size including header and all buffer slots.
        hv_ptr          = 1 * sizeof(int64_t),   ///< int64_t: current write-pointer slot index (atomic).
        hv_seq          = 2 * sizeof(int64_t),   ///< int64_t: monotonic write-sequence counter (atomic).
        hv_id           = 3 * sizeof(int64_t),   ///< int64_t: random session ID; changes on every open(bCreate=true).
        hv_ch           = 4 * sizeof(int64_t),   ///< int64_t: number of channels.
        hv_format       = 5 * sizeof(int64_t),   ///< int64_t: audio_format cast to int64_t.
        hv_samplerate   = 6 * sizeof(int64_t),   ///< int64_t: sample rate in Hz.
        hv_fps          = 7 * sizeof(int64_t),   ///< int64_t: nominal buffers per second.
        hv_bufs         = 8 * sizeof(int64_t),   ///< int64_t: number of buffer slots in the ring.
        hv_blocksz      = 9 * sizeof(int64_t),   ///< int64_t: size of one buffer slot in bytes (fv_last + PCM data).
        hv_last         = 10 * sizeof(int64_t)   ///< Total header size; buffer slot data begins at this offset.
    };

    /** Byte offsets of fields within a single buffer slot, relative to the slot start.
     *
     *  Layout: [fv_size : int64_t][fv_pts : int64_t][fv_seq : int64_t][PCM sample data...]
     *
     *  @note The addition of @c fv_pts changed @c fv_last from 16 to 24 bytes.
     *        Shares created by versions of the library that lacked this field
     *        will fail layout validation and cannot be opened by this version.
     */
    enum FrameHeaderVal
    {
        fv_size         = 0,                   ///< int64_t: (reserved; matches slot position in ring).
        fv_pts          = 1 * sizeof(int64_t), ///< int64_t: presentation timestamp (application-defined); set before next().
        fv_seq          = 2 * sizeof(int64_t), ///< int64_t: sequence number stamped by next() when this slot was published.
        fv_last         = 3 * sizeof(int64_t)  ///< Total per-slot header size; PCM data begins at this offset.
    };

public:

    /// Construct an un-opened handle.
    memaud() {};

    /// Destructor; calls close().
    ~memaud() { close(); }

    /** Create or attach to an audio ring buffer.
     *
     *  When bCreate=true, any existing share with the same name is removed and
     *  recreated.  When attaching to an existing share all parameters must match
     *  exactly or the call fails.
     *
     *  Buffer slot size is computed as ceil(sampleRate / fps) * ch * bytesPerSample.
     *  Ceiling division ensures each slot holds at least one full frame of audio
     *  even when rates do not divide evenly.
     *
     *  @param sName       Share name (POSIX: must start with '/').
     *  @param bCreate     Create the share.  Pass false to attach to an existing one.
     *  @param ch          Number of interleaved channels (must be > 0).
     *  @param fmt         Sample format.
     *  @param sampleRate  Sample rate in Hz (must be > 0).
     *  @param fps         Buffers per second — number of buffer slots written per second
     *                     (must be > 0 and <= sampleRate).
     *  @param bufs        Number of buffer slots in the ring (must be > 0).
     *  @returns true on success; false on failure (see last_error()).
     */
    bool open(const std::string &sName, bool bCreate, int64_t ch, audio_format fmt,
              int64_t sampleRate, int64_t fps, int64_t bufs);

    /** Attach to an existing audio ring buffer without knowing its parameters.
     *
     *  Validates the header before mapping.  Maps the segment read-only
     *  (least-privilege); writing through audview::m_ptr is undefined behaviour.
     *
     *  @param sName  Share name (POSIX: must start with '/').
     *  @returns true on success; false if the share does not exist or has an
     *           invalid/inconsistent header (errc::invalid_layout).
     */
    bool open_existing(const std::string &sName);

    /** Remove a stale audio share from the OS namespace.
     *  @param sName  Share name to remove.
     *  @returns true if the object was removed.
     */
    static bool remove(const std::string &sName) { return memmap::remove(sName); }

    /// Close the ring buffer and release all resources.
    void close();

    /// Returns true if this handle attached to an already-existing share.
    bool existing() { return m_mem.existing(); }

    /// Returns true if an audio ring buffer is currently open.
    bool isOpen() { return m_mem.isOpen(); }

    /** Return a view of the buffer slot at ring index @p idx.
     *
     *  The index is wrapped modulo getBufs() so any integer offset is valid.
     *  Header fields are snapshotted at call time and the resulting offset is
     *  bounds-checked against the mapped size before the pointer is returned.
     *
     *  @param idx  Slot index; wrapped with full modulo arithmetic.
     *  @returns audview pointing into the mapped PCM data for slot idx.
     *  @throws std::exception if the handle is not open or the computed slot
     *          address falls outside the mapped region (TOCTOU guard).
     */
    audview getBuf(int64_t idx) noexcept(false);

    /** Return the number of buffer slots in the ring.
     *  @returns Slot count, or 0 if not open.
     */
    int64_t getBufs();

    /** Fill a buffer slot with a constant byte value (e.g. 0 for silence).
     *  @param idx  Slot index; wrapped modulo getBufs().
     *  @param col  Byte value to write across the entire PCM buffer.
     *  @returns true on success; false if not open or slot address is out of bounds.
     */
    bool fill(int64_t idx, int col);

    /** Return the current write-pointer slot index offset by @p offset.
     *  @param offset  Signed offset from the current write pointer.
     *  @returns Wrapped slot index, or -1 if not open.
     */
    int64_t getPtr(int64_t offset);

    /** Set the write-pointer slot index to @p ptr (wrapped modulo getBufs()).
     *  @param ptr  New write-pointer value.
     *  @returns The resulting wrapped slot index, or -1 if not open.
     */
    int64_t setPtr(int64_t ptr);

    /** Return the signed circular distance from (ptr + bias) to @p pos.
     *  @param pos   Target slot index.
     *  @param bias  Offset added to the current write pointer before computing the distance.
     *  @returns Signed distance in the range [-(bufs/2), bufs/2].
     */
    int64_t getPtrErr(int64_t pos, int64_t bias);

    /** Stamp the current slot's sequence number, then advance the write pointer by @p inc.
     *  @param inc  Number of slots to advance (normally 1).
     *  @returns The new write-pointer slot index, or -1 if not open.
     */
    int64_t next(int64_t inc);

    /** Poll until getSeq() advances beyond @p lastSeq or @p wait_ms elapses.
     *  @param wait_ms  Maximum milliseconds to wait.  Pass 0 for a single non-blocking check.
     *  @param lastSeq  Sequence value to wait beyond.
     *  @returns true if getSeq() > lastSeq within the timeout; false on timeout (errc::timeout).
     */
    bool waitForFrame(uint64_t wait_ms, int64_t lastSeq);

    /** Return the number of channels.
     *  @returns Channel count, or 0 if not open.
     */
    int64_t getChannels();

    /** Return the sample format stored in the header.
     *  @returns Sample format enum value, or (audio_format)0 if not open.
     */
    audio_format getFormat();

    /** Return the sample format name string (e.g. "S16LE").
     *  @returns Pointer to a static null-terminated string.
     */
    const char *getFormatName();

    /** Return the number of bytes per sample for one channel.
     *  @returns Bytes per sample based on the stored format, or 0 if not open.
     */
    int64_t getBytesPerSample();

    /** Return the sample rate in Hz stored in the header.
     *  @returns Sample rate, or 0 if not open.
     */
    int64_t getSampleRate();

    /** Return the nominal buffer rate (buffers per second) stored in the header.
     *  @returns Buffers per second, or 0 if not open.
     */
    int64_t getFps();

    /** Return the PCM data size of a single buffer slot in bytes.
     *
     *  Equals ceil(sampleRate / fps) * channels * bytesPerSample.
     *  @returns Buffer size in bytes, or 0 if not open.
     */
    int64_t getBufSize();

    /** Return the random session ID written when the share was created.
     *
     *  Readers should save this on open and compare periodically.  A change
     *  means the writer restarted and the reader must close() and reopen.
     *  @returns Session ID, or 0 if not open.
     */
    int64_t getSessionId();

    /** Write the presentation timestamp for slot @p idx.
     *
     *  Call before next() so the timestamp is visible to readers after they
     *  observe the new sequence number.
     *
     *  @param idx  Slot index; wrapped modulo getBufs().
     *  @param pts  Timestamp value (microseconds, nanoseconds, or any epoch
     *              the application uses — the library stores it verbatim).
     *  @returns true on success; false if not open or the slot is out of bounds.
     */
    bool setPts(int64_t idx, int64_t pts);

    /** Return the presentation timestamp stored in slot @p idx.
     *  @param idx  Slot index; wrapped modulo getBufs().
     *  @returns The stored pts value, or 0 if not open or out of bounds.
     */
    int64_t getPts(int64_t idx);

    /** Return the global monotonic write-sequence counter.
     *
     *  Incremented by every next() call.  Use with getFrameSeq() to detect overrun:
     *  @code
     *      int64_t lag    = aud.getSeq() - rLastSeq;
     *      bool    lapped = lag >= aud.getBufs();
     *  @endcode
     *  @returns Current sequence counter, or -1 if not open.
     */
    int64_t getSeq();

    /** Return the sequence number most recently stamped into slot @p idx by next().
     *
     *  A value of 0 means the slot has never been written.
     *  @param idx  Slot index; wrapped modulo getBufs().
     *  @returns Per-slot sequence number, or -1 if not open or the header is invalid.
     */
    int64_t getFrameSeq(int64_t idx);

private:

    /// The underlying memory map.
    memmap      m_mem;

};

}; // end namespace
