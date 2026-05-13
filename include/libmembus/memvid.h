#pragma once

namespace LIBMEMBUS_NS
{

enum class video_format : int64_t
{
    gray8 = 1,
    rgb24,
    bgr24,
    rgba32,
    bgra32,
    yuyv422,
    uyvy422
};

const char *video_format_name(video_format fmt);
int64_t video_format_bytes_per_pixel(video_format fmt);

/** Memory mapped video buffer

*/
class memvid
{
public:

    /** Fit value to unit size
        @param [in] val     - Value to fit
        @param [in] unit    - Unit size into which to fit value
    */
    template < typename T >
        static T fitTo( T val, T unit )
    {	if ( !unit ) return 0;
        T i = (T)( val / unit );
        return ( ( i * unit ) >= val ) ? i : i + 1;
    }

    /** Returns the absolute value
        @param [in] v   - Value for which the absolute value will be returned.
    */
    template < typename T >
        static T abs( T v )
    {   return ( v >= 0 ) ? v : -v; }

    /** View of memory mapped video buffer
    */
    class vidview
    {
    public:

        /** Constructor
            @param [in] p   - Pointer to buffer
            @param [in] sz  - Size of buffer
            @param [in] sw  - Scan width
            @param [in] w   - Image width
            @param [in] h   - Image height
        */
        vidview(char* p, int64_t sz, int64_t sw, int64_t w, int64_t h, video_format fmt)
            : m_ptr(p), m_size(sz), m_sw(sw), m_w(w), m_h(h), m_format(fmt)
        {
        }

        /** Copy constructor
            @param [in] r   - Object to copy
        */
        vidview(const vidview &r)
        {
            (*this) = r;
        }

        /// Destructor
        virtual ~vidview() {}

        /** Copy operator
            @param [in] r   - Object to copy
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

        /// Pointer to data
        char        *m_ptr;

        /// Buffer size
        int64_t     m_size;

        /// Image width
        int64_t     m_w;

        /// Image height
        int64_t     m_h;

        /// Image scan width
        int64_t     m_sw;

        /// Pixel format
        video_format m_format;
    };

    /// Main header
    enum HeaderVal
    {
        hv_size         = 0,
        hv_ptr          = 1 * sizeof(int64_t),
        hv_seq          = 2 * sizeof(int64_t),   // monotonic write-sequence counter
        hv_id           = 3 * sizeof(int64_t),   // random session ID, changes on every open(bCreate=true)
        hv_width        = 4 * sizeof(int64_t),
        hv_height       = 5 * sizeof(int64_t),
        hv_scanwidth    = 6 * sizeof(int64_t),
        hv_format       = 7 * sizeof(int64_t),
        hv_fps          = 8 * sizeof(int64_t),
        hv_bufs         = 9 * sizeof(int64_t),
        hv_blocksz      = 10 * sizeof(int64_t),
        hv_last         = 11 * sizeof(int64_t)
    };

    /// Frame header: [size][vpts][apts][seq][pixel data...]
    enum FrameHeaderVal
    {
        fv_size         = 0,
        fv_vpts         = 1 * sizeof(int64_t),
        fv_apts         = 2 * sizeof(int64_t),
        fv_seq          = 3 * sizeof(int64_t),   // sequence number stamped by next()
        fv_last         = 4 * sizeof(int64_t)
    };

public:

    /// Constructor
    memvid() {};

    /// Close the buffer
    ~memvid() { close(); }

    /// Creates / attaches to an image ring buffer in memory
    bool open(const std::string &sName, bool bCreate,
              int64_t w, int64_t h, video_format fmt,
              int64_t fps, int64_t bufs);

    /// Open an existing image share
    bool open_existing(const std::string &sName);

    /// Remove a stale video share from the OS namespace
    static bool remove(const std::string &sName) { return memmap::remove(sName); }

    /// Close image share
    void close();

    /// Returns true if a share is open
    bool isOpen() { return m_mem.isOpen(); }

    /// Returns true if memory already existed
    bool existing() { return m_mem.existing(); }

    /// Get image at specified index
    vidview getBuf(int64_t idx) noexcept(false);

    /// Fills the frame with the specified value
    bool fill(int64_t idx, int col);

    /// Returns the number of image buffers
    int64_t getBufs();

    /// Get image index, offset places from the ptr
    int64_t getPtr(int64_t offset);

    /// Sets hthe image index
    int64_t setPtr(int64_t ptr);

    /// Calculates the offset error from ptr
    int64_t getPtrErr(int64_t pos, int64_t bias);

    /// Increment pointer by inc
    int64_t next(int64_t inc);

    /// Wait until getSeq() advances beyond lastSeq, or timeout expires.
    bool waitForFrame(uint64_t wait_ms, int64_t lastSeq);

    /// Image width
    int64_t getWidth();

    /// Image width
    int64_t getHeight();

    /// Pixel format
    video_format getFormat();

    /// Pixel format name
    const char *getFormatName();

    /// Image fps
    int64_t getFps();

    /** Returns the session ID written when the share was created.
        Readers should save this on open and compare periodically.  A change means
        the writer restarted and the reader must close() and reopen.
    */
    int64_t getSessionId();

    /** Returns the global write-sequence counter (incremented by every next() call).

        Readers use this together with the sequence number of the last frame they
        processed (rLastSeq) to measure lag and detect overrun:

          int64_t lag    = vid.getSeq() - rLastSeq;
          bool    lapped = lag >= vid.getBufs();

        A lag of zero means the reader is up to date.  A lag >= getBufs() means the
        writer has advanced a full ring past the reader's last position: the slot the
        reader would read next has been (or is about to be) overwritten.
    */
    int64_t getSeq();

    /** Returns the sequence number most recently stamped into slot @p idx by next().
        A value of 0 means the slot has never been written.

        Use to verify a slot holds the expected frame before reading it:
          bool ready   = vid.getFrameSeq(rPos) > rLastSeq;      // new data available
          bool in_sync = vid.getFrameSeq(rPos) == rLastSeq + 1; // no frames skipped
    */
    int64_t getFrameSeq(int64_t idx);

private:

    /// The memory map
    memmap      m_mem;

};

}; // end namespace
