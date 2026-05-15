#pragma once

namespace LIBMEMBUS_NS
{

/** Packed pixel formats stored in the video ring buffer header.
 *
 *  The format value is written as an int64_t at byte offset hv_format in the
 *  shared-memory header so that readers can discover the format without knowing
 *  it in advance.
 */
enum class video_format : int64_t
{
    gray8   = 1, ///< 8-bit greyscale; 1 byte per pixel.
    rgb24,       ///< 24-bit RGB packed (R, G, B); 3 bytes per pixel.
    bgr24,       ///< 24-bit BGR packed (B, G, R); 3 bytes per pixel.
    rgba32,      ///< 32-bit RGBA packed; 4 bytes per pixel.
    bgra32,      ///< 32-bit BGRA packed; 4 bytes per pixel.
    yuyv422,     ///< YUV 4:2:2 packed, YUYV byte order; 2 bytes per pixel; requires even width.
    uyvy422      ///< YUV 4:2:2 packed, UYVY byte order; 2 bytes per pixel; requires even width.
};

/** Return the name string for a video_format enum value (e.g. "RGB24").
 *  @param fmt  Format to name.
 *  @returns Pointer to a static null-terminated string; "unknown" for unrecognised values.
 */
const char *video_format_name(video_format fmt);

/** Return the number of bytes per pixel for a video_format.
 *  @param fmt  Format to query.
 *  @returns Bytes per pixel, or 0 for unrecognised values.
 */
int64_t video_format_bytes_per_pixel(video_format fmt);

/** Lock-free multi-buffer video ring buffer in shared memory.
 *
 *  The writer creates the share with open(bCreate=true) and calls next() after
 *  filling each frame slot.  Readers attach with open_existing() and observe
 *  the write pointer and per-frame sequence numbers independently with no
 *  synchronisation between readers.
 *
 *  Overrun detection: compare getSeq() - rLastSeq against getBufs().  When the
 *  difference is >= getBufs() the writer has lapped the reader.
 */
class memvid
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

    /** Read-only view of a single frame slot in the ring buffer.
     *
     *  Returned by getBuf().  The pointer is valid only while the underlying
     *  memvid handle is open.  On a read-only mapping (open_existing()) writing
     *  through m_ptr is undefined behaviour.
     */
    class vidview
    {
    public:

        /** Construct a view over a raw pixel buffer.
         *  @param p    Pointer to the first pixel byte.
         *  @param sz   Total buffer size in bytes (m_sw * m_h).
         *  @param sw   Scan width — bytes per row.
         *  @param w    Frame width in pixels.
         *  @param h    Frame height in pixels.
         *  @param fmt  Pixel format.
         */
        vidview(char* p, int64_t sz, int64_t sw, int64_t w, int64_t h, video_format fmt)
            : m_ptr(p), m_size(sz), m_sw(sw), m_w(w), m_h(h), m_format(fmt)
        {
        }

        /** Copy constructor.
         *  @param r  View to copy.
         */
        vidview(const vidview &r)
        {
            (*this) = r;
        }

        /// Destructor.
        virtual ~vidview() {}

        /** Copy assignment.
         *  @param r  View to copy.
         *  @returns Reference to this.
         */
        vidview& operator = (const vidview &r)
        {
            m_ptr = r.m_ptr;
            m_size = r.m_size;
            m_w = r.m_w;
            m_h = r.m_h;
            m_sw = r.m_sw;
            m_format = r.m_format;

            return *this;
        }

    public:

        /// Pointer to the first pixel byte of the frame.
        char        *m_ptr;

        /// Total pixel data size in bytes (m_sw * m_h).
        int64_t     m_size;

        /// Frame width in pixels.
        int64_t     m_w;

        /// Frame height in pixels.
        int64_t     m_h;

        /// Scan width — bytes per row (m_w * bytes_per_pixel for packed formats).
        int64_t     m_sw;

        /// Pixel format of this frame.
        video_format m_format;
    };

    /** Byte offsets of fields in the shared-memory main header.
     *
     *  These constants are public so that callers can inspect or corrupt-test
     *  header fields via a raw memmap handle.  Normal usage goes through the
     *  typed accessors (getWidth(), getSeq(), etc.).
     */
    enum HeaderVal
    {
        hv_size         = 0,                      ///< int64_t: total allocated size including header and all frame slots.
        hv_ptr          = 1 * sizeof(int64_t),    ///< int64_t: current write-pointer slot index (atomic).
        hv_seq          = 2 * sizeof(int64_t),    ///< int64_t: monotonic write-sequence counter (atomic).
        hv_id           = 3 * sizeof(int64_t),    ///< int64_t: random session ID; changes on every open(bCreate=true).
        hv_width        = 4 * sizeof(int64_t),    ///< int64_t: frame width in pixels.
        hv_height       = 5 * sizeof(int64_t),    ///< int64_t: frame height in pixels.
        hv_scanwidth    = 6 * sizeof(int64_t),    ///< int64_t: scan width (bytes per row).
        hv_format       = 7 * sizeof(int64_t),    ///< int64_t: video_format cast to int64_t.
        hv_fps          = 8 * sizeof(int64_t),    ///< int64_t: nominal frames per second.
        hv_bufs         = 9 * sizeof(int64_t),    ///< int64_t: number of frame slots in the ring.
        hv_blocksz      = 10 * sizeof(int64_t),   ///< int64_t: size of one frame slot in bytes (fv_last + pixel data).
        hv_last         = 11 * sizeof(int64_t)    ///< Total header size; frame data begins at this offset.
    };

    /** Byte offsets of fields within a single frame slot, relative to the slot start.
     *
     *  Layout: [fv_size : int64_t][fv_vpts : int64_t][fv_apts : int64_t][fv_seq : int64_t][pixel data...]
     */
    enum FrameHeaderVal
    {
        fv_size         = 0,                   ///< int64_t: (reserved; matches slot position in ring).
        fv_vpts         = 1 * sizeof(int64_t), ///< int64_t: video presentation timestamp (application-defined).
        fv_apts         = 2 * sizeof(int64_t), ///< int64_t: audio presentation timestamp (application-defined).
        fv_seq          = 3 * sizeof(int64_t), ///< int64_t: sequence number stamped by next() when this slot was published.
        fv_last         = 4 * sizeof(int64_t)  ///< Total per-frame header size; pixel data begins at this offset.
    };

public:

    /// Construct an un-opened handle.
    memvid() {};

    /// Destructor; calls close().
    ~memvid() { close(); }

    /** Create or attach to a video ring buffer.
     *
     *  When bCreate=true, any existing share with the same name is removed and
     *  recreated.  When attaching to an existing share all layout parameters
     *  must match exactly or the call fails.
     *
     *  @param sName    Share name (POSIX: must start with '/').
     *  @param bCreate  Create the share.  Pass false to attach to an existing one.
     *  @param w        Frame width in pixels.
     *  @param h        Frame height in pixels.
     *  @param fmt      Pixel format.
     *  @param fps      Nominal frames per second (must be > 0).
     *  @param bufs     Number of frame slots in the ring (must be > 0).
     *  @returns true on success; false on failure (see last_error()).
     */
    bool open(const std::string &sName, bool bCreate,
              int64_t w, int64_t h, video_format fmt,
              int64_t fps, int64_t bufs);

    /** Attach to an existing video ring buffer without knowing its parameters.
     *
     *  Validates the header before mapping.  Maps the segment read-only
     *  (least-privilege); writing through vidview::m_ptr is undefined behaviour.
     *
     *  @param sName  Share name (POSIX: must start with '/').
     *  @returns true on success; false if the share does not exist or has an
     *           invalid/inconsistent header (errc::invalid_layout).
     */
    bool open_existing(const std::string &sName);

    /** Remove a stale video share from the OS namespace.
     *  @param sName  Share name to remove.
     *  @returns true if the object was removed.
     */
    static bool remove(const std::string &sName) { return memmap::remove(sName); }

    /// Close the ring buffer and release all resources.
    void close();

    /// Returns true if a video ring buffer is currently open.
    bool isOpen() { return m_mem.isOpen(); }

    /// Returns true if this handle attached to an already-existing share.
    bool existing() { return m_mem.existing(); }

    /** Return a view of the frame slot at ring index @p idx.
     *
     *  The index is wrapped modulo getBufs() so any integer offset is valid.
     *  Header fields are snapshotted at call time and the resulting offset is
     *  bounds-checked against the mapped size before the pointer is returned.
     *
     *  @param idx  Slot index; wrapped with full modulo arithmetic.
     *  @returns vidview pointing into the mapped pixel data for slot idx.
     *  @throws std::exception if the handle is not open or the computed slot
     *          address falls outside the mapped region (TOCTOU guard).
     */
    vidview getBuf(int64_t idx) noexcept(false);

    /** Fill a frame slot with a constant byte value.
     *
     *  @param idx  Slot index; wrapped modulo getBufs().
     *  @param col  Byte value to write across the entire pixel buffer.
     *  @returns true on success; false if the handle is not open or the
     *           computed slot address is out of bounds.
     */
    bool fill(int64_t idx, int col);

    /** Return the number of frame slots in the ring.
     *  @returns Slot count, or 0 if not open.
     */
    int64_t getBufs();

    /** Return the current write-pointer slot index offset by @p offset.
     *
     *  Equivalent to (ptr + offset) % bufs with full wrap-around for large
     *  or negative offsets.
     *
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
     *
     *  Useful for phase-lock feedback: a positive result means pos is ahead of
     *  the biased pointer; negative means it is behind.
     *
     *  @param pos   Target slot index.
     *  @param bias  Offset added to the current write pointer before computing
     *               the distance.
     *  @returns Signed distance in the range [-(bufs/2), bufs/2].
     */
    int64_t getPtrErr(int64_t pos, int64_t bias);

    /** Stamp the current slot's sequence number, then advance the write pointer by @p inc.
     *
     *  The sequence number is incremented atomically before the pointer moves so
     *  that readers can detect overrun by comparing getSeq() - rLastSeq against
     *  getBufs().
     *
     *  @param inc  Number of slots to advance the write pointer (normally 1).
     *  @returns The new write-pointer slot index after advancing, or -1 if not open.
     */
    int64_t next(int64_t inc);

    /** Poll until getSeq() advances beyond @p lastSeq or @p wait_ms elapses.
     *
     *  Sleeps 1 ms between polls to avoid busy-waiting.
     *
     *  @param wait_ms  Maximum milliseconds to wait.  Pass 0 for a single
     *                  non-blocking check.
     *  @param lastSeq  Sequence value to wait beyond.
     *  @returns true if getSeq() > lastSeq within the timeout; false on timeout
     *           (errc::timeout).
     */
    bool waitForFrame(uint64_t wait_ms, int64_t lastSeq);

    /** Return the frame width in pixels.
     *  @returns Width, or -1 if not open.
     */
    int64_t getWidth();

    /** Return the frame height in pixels.
     *  @returns Height, or -1 if not open.
     */
    int64_t getHeight();

    /** Return the pixel format stored in the header.
     *  @returns Pixel format enum value, or (video_format)0 if not open.
     */
    video_format getFormat();

    /** Return the pixel format name string (e.g. "RGB24").
     *  @returns Pointer to a static null-terminated string.
     */
    const char *getFormatName();

    /** Return the nominal frame rate stored in the header.
     *  @returns Frames per second, or -1 if not open.
     */
    int64_t getFps();

    /** Return the random session ID written when the share was created.
     *
     *  Readers should save this on open and compare periodically.  A change
     *  means the writer restarted and the reader must close() and reopen.
     *  @returns Session ID, or 0 if not open.
     */
    int64_t getSessionId();

    /** Write the video presentation timestamp for slot @p idx.
     *
     *  Call before next() so the timestamp is visible to readers after they
     *  observe the new sequence number.  The value is application-defined;
     *  the library does not interpret it.
     *
     *  @param idx  Slot index; wrapped modulo getBufs().
     *  @param pts  Timestamp value (microseconds, nanoseconds, or any epoch
     *              the application uses — the library stores it verbatim).
     *  @returns true on success; false if not open or the slot is out of bounds.
     */
    bool setVpts(int64_t idx, int64_t pts);

    /** Write the companion audio presentation timestamp for slot @p idx.
     *
     *  Useful when a video frame and its audio block share the same ring slot
     *  and the application wants to carry both timestamps in one place.
     *  Call before next().
     *
     *  @param idx  Slot index; wrapped modulo getBufs().
     *  @param pts  Audio presentation timestamp.
     *  @returns true on success; false if not open or the slot is out of bounds.
     */
    bool setApts(int64_t idx, int64_t pts);

    /** Return the video presentation timestamp stored in slot @p idx.
     *  @param idx  Slot index; wrapped modulo getBufs().
     *  @returns The stored vpts value, or 0 if not open or out of bounds.
     */
    int64_t getVpts(int64_t idx);

    /** Return the audio presentation timestamp stored in slot @p idx.
     *  @param idx  Slot index; wrapped modulo getBufs().
     *  @returns The stored apts value, or 0 if not open or out of bounds.
     */
    int64_t getApts(int64_t idx);

    /** Return the global monotonic write-sequence counter.
     *
     *  Incremented by every next() call.  Readers use this together with the
     *  sequence number of the last frame they processed (rLastSeq) to measure
     *  lag and detect overrun:
     *  @code
     *      int64_t lag    = vid.getSeq() - rLastSeq;
     *      bool    lapped = lag >= vid.getBufs();
     *  @endcode
     *  @returns Current sequence counter, or -1 if not open.
     */
    int64_t getSeq();

    /** Return the sequence number most recently stamped into slot @p idx by next().
     *
     *  A value of 0 means the slot has never been written.  Use this to verify
     *  that a slot contains the expected frame before reading it:
     *  @code
     *      bool ready   = vid.getFrameSeq(rPos) > rLastSeq;      // new data available
     *      bool in_sync = vid.getFrameSeq(rPos) == rLastSeq + 1; // no frames skipped
     *  @endcode
     *  @param idx  Slot index; wrapped modulo getBufs().
     *  @returns Per-slot sequence number, or -1 if not open or the header is invalid.
     */
    int64_t getFrameSeq(int64_t idx);

private:

    /// The underlying memory map.
    memmap      m_mem;

};

}; // end namespace
