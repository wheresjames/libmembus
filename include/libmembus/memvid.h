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
    uyvy422,     ///< YUV 4:2:2 packed, UYVY byte order; 2 bytes per pixel; requires even width.

    userType = 0x1000 ///< Opaque / custom fixed-size format; identity carried in hv_fourcc / hv_guid,
                      ///< geometry supplied by the caller (scanwidth). See MB-MEMPKT.md §3.
};

/** Return the name string for a video_format enum value (e.g. "RGB24").
 *  @param fmt  Format to name.
 *  @returns Pointer to a static null-terminated string; "unknown" for unrecognised values.
 */
const char *video_format_name(video_format fmt);

/** Return the number of bytes per pixel for a video_format.
 *  @param fmt  Format to query.
 *  @returns Bytes per pixel, or 0 for unrecognised or opaque (userType) values.
 */
int64_t video_format_bytes_per_pixel(video_format fmt);

/** Lock-free multi-buffer video ring buffer in shared memory.
 *
 *  The writer creates the share with open(bCreate=true) and calls next() after
 *  filling each frame slot.  Readers attach with open_existing() and observe
 *  the write pointer and per-frame sequence numbers independently with no
 *  synchronisation between readers.
 *
 *  Single-writer, multiple-reader (SPMC).  Overrun detection: compare
 *  getSeq() - rLastSeq against getBufs().  When the difference is >= getBufs()
 *  the writer has lapped the reader.
 */
class memvid
{
public:

    /// Shared class discriminator written to hv_type.  See MB-MEMPKT.md §10.1.
    static const int64_t k_type    = 1;      ///< memvid
    /// Shared-memory magic constant written to hv_magic ('MBUS').
    static const int64_t k_magic   = 0x5355424dLL;
    /// Current header layout version written to hv_version.
    static const int64_t k_version = 2;
    /// Default payload alignment when the caller passes 0.
    static const int64_t k_defAlign = 64;

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
     *  The first block (hv_magic .. hv_reserved7) is the shared header prefix,
     *  laid out identically in memvid, memaud and mempkt so a generic reader can
     *  identify any share from magic/type/version without knowing the class
     *  (MB-MEMPKT.md §10.1).  The class-specific fields follow.  hv_useroffset
     *  and hv_dataoffset are values *computed at open()* and stored, not
     *  structural constants — every slot accessor reads them.
     */
    enum HeaderVal
    {
        // ---- shared prefix (identical offsets in all classes) ----
        hv_magic        =  0 * sizeof(int64_t),  ///< int64_t: k_magic; rejects non-libmembus segments.
        hv_type         =  1 * sizeof(int64_t),  ///< int64_t: class discriminator (k_type).
        hv_version      =  2 * sizeof(int64_t),  ///< int64_t: header layout version (k_version).
        hv_size         =  3 * sizeof(int64_t),  ///< int64_t: total allocated size including header and all frame slots.
        hv_ptr          =  4 * sizeof(int64_t),  ///< int64_t: current write-pointer slot index (atomic).
        hv_seq          =  5 * sizeof(int64_t),  ///< int64_t: monotonic write-sequence counter (atomic).
        hv_id           =  6 * sizeof(int64_t),  ///< int64_t: random session ID; changes on every open(bCreate=true).
        hv_bufs         =  7 * sizeof(int64_t),  ///< int64_t: number of frame slots in the ring.
        hv_fourcc       =  8 * sizeof(int64_t),  ///< int64_t: fourcc (low 32 bits); 0 = none.
        hv_guid_lo      =  9 * sizeof(int64_t),  ///< int64_t: GUID bytes 0..7.
        hv_guid_hi      = 10 * sizeof(int64_t),  ///< int64_t: GUID bytes 8..15.
        hv_align        = 11 * sizeof(int64_t),  ///< int64_t: payload alignment (power of two, >= 8).
        hv_metasize     = 12 * sizeof(int64_t),  ///< int64_t: main user buffer size in bytes (0 = none).
        hv_reserved0    = 13 * sizeof(int64_t),  ///< int64_t: spare.
        hv_reserved7    = 20 * sizeof(int64_t),  ///< int64_t: last spare slot.
        hv_common_end   = 21 * sizeof(int64_t),  ///< End of shared prefix.

        // ---- memvid class-specific ----
        hv_width        = 21 * sizeof(int64_t),  ///< int64_t: frame width in pixels.
        hv_height       = 22 * sizeof(int64_t),  ///< int64_t: frame height in pixels.
        hv_scanwidth    = 23 * sizeof(int64_t),  ///< int64_t: scan width (bytes per row).
        hv_format       = 24 * sizeof(int64_t),  ///< int64_t: video_format cast to int64_t.
        hv_fps          = 25 * sizeof(int64_t),  ///< int64_t: nominal frames per second.
        hv_blocksz      = 26 * sizeof(int64_t),  ///< int64_t: size of one frame slot in bytes (aligned).
        hv_frameextra   = 27 * sizeof(int64_t),  ///< int64_t: per-frame user buffer stride in bytes (0 = none).
        hv_useroffset   = 28 * sizeof(int64_t),  ///< int64_t: computed base offset of PERFRAMEUSERBUF region.
        hv_dataoffset   = 29 * sizeof(int64_t),  ///< int64_t: computed base offset of PERFRAMEDATABUFFER region.
        hv_last         = 30 * sizeof(int64_t)   ///< Total fixed header size; MAINUSERBUF begins here.
    };

    /** Byte offsets of fields within a single frame slot, relative to the slot start.
     *
     *  Layout: [fv_size][fv_vpts][fv_apts][fv_userlen][fv_seq] then padding up to
     *  hv_align, then pixel data.
     */
    enum FrameHeaderVal
    {
        fv_size         = 0,                   ///< int64_t: (reserved; matches slot position in ring).
        fv_vpts         = 1 * sizeof(int64_t), ///< int64_t: video presentation timestamp (application-defined).
        fv_apts         = 2 * sizeof(int64_t), ///< int64_t: audio presentation timestamp (application-defined).
        fv_userlen      = 3 * sizeof(int64_t), ///< int64_t: used bytes in this slot's PERFRAMEUSERBUF entry.
        fv_seq          = 4 * sizeof(int64_t), ///< int64_t: sequence number stamped by next() when this slot was published.
        fv_last         = 5 * sizeof(int64_t)  ///< Per-frame header size; padded up to hv_align before pixel data.
    };

public:

    /// Construct an un-opened handle.
    memvid() {};

    /// Destructor; calls close().
    ~memvid() { close(); }

    /** Create or attach to a video ring buffer.
     *
     *  The trailing parameters are additive and default to today's behaviour, so
     *  existing callers are source-compatible (MB-MEMPKT.md §3.2).
     *
     *  @param sName      Share name (POSIX: must start with '/').
     *  @param bCreate    Create the share.  Pass false to attach to an existing one.
     *  @param w          Frame width in pixels.
     *  @param h          Frame height in pixels.
     *  @param fmt        Pixel format.
     *  @param fps        Nominal frames per second (must be > 0).
     *  @param bufs       Number of frame slots in the ring (must be > 0).
     *  @param scanwidth  Bytes per row.  Required (> 0) for video_format::userType;
     *                    pass 0 to derive from fmt for known formats.
     *  @param align      Payload alignment (power of two, >= 8); 0 = default 64.
     *  @param frameextra Per-frame user buffer size in bytes (0 = none).
     *  @param fourcc     Fourcc identity (0 = none).
     *  @param guid       Optional 16-byte GUID identity (null = none).
     *  @param meta       Optional main user buffer bytes copied in at create.
     *  @param metasz     Size of @p meta in bytes (0 = none).
     *  @returns true on success; false on failure (see last_error()).
     */
    bool open(const std::string &sName, bool bCreate,
              int64_t w, int64_t h, video_format fmt,
              int64_t fps, int64_t bufs,
              int64_t scanwidth = 0, int64_t align = 0, int64_t frameextra = 0,
              uint32_t fourcc = 0, const uint8_t *guid = nullptr,
              const void *meta = nullptr, int64_t metasz = 0);

    /** Attach to an existing video ring buffer without knowing its parameters.
     *
     *  Validates magic/type and the header before mapping.  Maps the segment
     *  read-only (least-privilege); writing through vidview::m_ptr is undefined.
     *
     *  @param sName  Share name (POSIX: must start with '/').
     *  @returns true on success; false if the share does not exist, is the wrong
     *           type, or has an invalid/inconsistent header.
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
     *  @param idx  Slot index; wrapped with full modulo arithmetic.
     *  @returns vidview pointing into the mapped pixel data for slot idx.
     *  @throws std::exception if not open or the computed slot address is out of bounds.
     */
    vidview getBuf(int64_t idx) noexcept(false);

    /** Fill a frame slot with a constant byte value.
     *  @param idx  Slot index; wrapped modulo getBufs().
     *  @param col  Byte value to write across the entire pixel buffer.
     *  @returns true on success; false on failure.
     */
    bool fill(int64_t idx, int col);

    /** Return the number of frame slots in the ring.  0 if not open. */
    int64_t getBufs();

    /** Return the current write-pointer slot index offset by @p offset.  -1 if not open. */
    int64_t getPtr(int64_t offset);

    /** Set the write-pointer slot index to @p ptr (wrapped modulo getBufs()). */
    int64_t setPtr(int64_t ptr);

    /** Return the signed circular distance from (ptr + bias) to @p pos. */
    int64_t getPtrErr(int64_t pos, int64_t bias);

    /** Stamp the current slot's sequence number, then advance the write pointer by @p inc. */
    int64_t next(int64_t inc);

    /** Poll until getSeq() advances beyond @p lastSeq or @p wait_ms elapses. */
    bool waitForFrame(uint64_t wait_ms, int64_t lastSeq);

    /** Return the frame width in pixels.  -1 if not open. */
    int64_t getWidth();

    /** Return the frame height in pixels.  -1 if not open. */
    int64_t getHeight();

    /** Return the pixel format stored in the header. */
    video_format getFormat();

    /** Return the pixel format name string (e.g. "RGB24"). */
    const char *getFormatName();

    /** Return the nominal frame rate stored in the header.  -1 if not open. */
    int64_t getFps();

    /** Return the random session ID written when the share was created.  0 if not open. */
    int64_t getSessionId();

    /** Return the header layout version (hv_version).  -1 if not open. */
    int64_t getVersion();

    /** Return the payload alignment (hv_align).  0 if not open. */
    int64_t getAlign();

    /** Return the fourcc identity (low 32 bits).  0 if not open or none. */
    uint32_t getFourcc();

    /** Copy the 16-byte GUID identity into @p out.
     *  @returns true if a non-zero GUID is present; false otherwise. */
    bool getGuid(uint8_t out[16]);

    /** Return a pointer to the main user metadata buffer, or null if none. */
    const char *getMeta();

    /** Return the size of the main user metadata buffer in bytes.  0 if none. */
    int64_t getMetaSize();

    /** Return the per-frame user buffer stride (hv_frameextra).  0 if none. */
    int64_t getFrameExtra();

    /** Copy up to getFrameExtra() bytes into slot @p idx's user buffer and set its length.
     *  Call before next().  @returns true on success. */
    bool setUserData(int64_t idx, const void *data, int64_t len);

    /** Return a pointer to slot @p idx's per-frame user buffer, or null if none/out of bounds. */
    const char *getUserData(int64_t idx);

    /** Return the used length of slot @p idx's per-frame user buffer (fv_userlen). */
    int64_t getUserLen(int64_t idx);

    /** Write the video presentation timestamp for slot @p idx.  Call before next(). */
    bool setVpts(int64_t idx, int64_t pts);

    /** Write the companion audio presentation timestamp for slot @p idx.  Call before next(). */
    bool setApts(int64_t idx, int64_t pts);

    /** Return the video presentation timestamp stored in slot @p idx. */
    int64_t getVpts(int64_t idx);

    /** Return the audio presentation timestamp stored in slot @p idx. */
    int64_t getApts(int64_t idx);

    /** Return the global monotonic write-sequence counter.  -1 if not open. */
    int64_t getSeq();

    /** Return the sequence number most recently stamped into slot @p idx by next(). */
    int64_t getFrameSeq(int64_t idx);

private:

    /// The underlying memory map.
    memmap      m_mem;

};

}; // end namespace
