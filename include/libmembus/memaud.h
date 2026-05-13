#pragma once

namespace LIBMEMBUS_NS
{

/** Memory mapped audio buffer
*/
class memaud
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

    /** Audio buffer view
    */
    class audview
    {
    public:

        /** Constructor
            @param [in] p   - Pointer to buffer
            @param [in] sz  - Size of buffer
            @param [in] ch  - Number of channels
            @param [in] bps - Bit rate, (bits per sample)
        */
        audview(char* p, int64_t sz, int64_t ch, int64_t bps)
            : m_ptr(p), m_size(sz), m_ch(ch), m_bps(bps)
        {
        }

        /** Copy constructor
            @param [in] r   - Object to copy
        */
        audview(const audview &r)
        {
            (*this) = r;
        }

        /// Destructor
        virtual ~audview() {}

        /** Copy operator
            @param [in] r   - Object to copy
        */
        audview& operator = (const audview &r)
        {
            m_ptr = r.m_ptr;
            m_size = r.m_size;
            m_ch = r.m_ch;
            m_bps = r.m_bps;

            return *this;
        }

    public:

        /// Pointer to data
        char        *m_ptr;

        /// Buffer size
        int64_t     m_size;

        /// Channels
        int64_t     m_ch;

        /// Bits per sample
        int64_t     m_bps;
    };

    /// Main header
    enum HeaderVal
    {
        hv_size         = 0,
        hv_ptr          = 1 * sizeof(int64_t),
        hv_seq          = 2 * sizeof(int64_t),   // monotonic write-sequence counter
        hv_id           = 3 * sizeof(int64_t),   // random session ID, changes on every open(bCreate=true)
        hv_ch           = 4 * sizeof(int64_t),
        hv_bps          = 5 * sizeof(int64_t),
        hv_bitrate      = 6 * sizeof(int64_t),
        hv_fps          = 7 * sizeof(int64_t),
        hv_bufs         = 8 * sizeof(int64_t),
        hv_blocksz      = 9 * sizeof(int64_t),
        hv_last         = 10 * sizeof(int64_t)
    };

    /// Frame header: [size][seq][sample data...]
    enum FrameHeaderVal
    {
        fv_size         = 0,
        fv_seq          = 1 * sizeof(int64_t),   // sequence number stamped by next()
        fv_last         = 2 * sizeof(int64_t)
    };

public:

    /// Constructor
    memaud() {};

    /// Close the buffer
    ~memaud() { close(); }

    /// Creates / attaches to an image ring buffer in memory
    bool open(const std::string &sName, bool bCreate, int64_t ch, int64_t bps, int64_t bitrate, int64_t fps, int64_t bufs);

    /// Open an existing image share
    bool open_existing(const std::string &sName);

    /// Close image share
    void close();

    /// Returns true if memory already existed
    bool existing() { return m_mem.existing(); }

    /// Returns true if a share is open
    bool isOpen() { return m_mem.isOpen(); }

    /// Get image at specified index
    audview getBuf(int64_t idx) noexcept(false);

    /// Returns the number of image buffers
    int64_t getBufs();

    /// Fills the frame with the specified value
    bool fill(int64_t idx, int col);

    /// Get image index, offset places from the ptr
    int64_t getPtr(int64_t offset);

    /// Sets hthe image index
    int64_t setPtr(int64_t ptr);

    /// Calculates the offset error from ptr
    int64_t getPtrErr(int64_t pos, int64_t bias);

    /// Increment pointer by inc
    int64_t next(int64_t inc);

    /// Channels
    int64_t getChannels();

    /// Bits per second
    int64_t getBps();

    /// Bit rate
    int64_t getBitRate();

    /// Frames per second
    int64_t getFps();

    /// Size of a single buffer
    int64_t getBufSize();

    /** Returns the session ID written when the share was created.
        Readers should save this on open and compare periodically.  A change means
        the writer restarted and the reader must close() and reopen.
    */
    int64_t getSessionId();

    /** Returns the global write-sequence counter (incremented by every next() call).

        Readers use this together with the sequence number of the last buffer they
        processed (rLastSeq) to measure lag and detect overrun:

          int64_t lag    = aud.getSeq() - rLastSeq;
          bool    lapped = lag >= aud.getBufs();

        A lag of zero means the reader is up to date.  A lag >= getBufs() means the
        writer has advanced a full ring past the reader's last position.
    */
    int64_t getSeq();

    /** Returns the sequence number most recently stamped into slot @p idx by next().
        A value of 0 means the slot has never been written.

        Use to verify a slot holds the expected buffer before reading it:
          bool ready   = aud.getFrameSeq(rPos) > rLastSeq;      // new data available
          bool in_sync = aud.getFrameSeq(rPos) == rLastSeq + 1; // no buffers skipped
    */
    int64_t getFrameSeq(int64_t idx);

private:

    /// The memory map
    memmap      m_mem;

};

}; // end namespace
