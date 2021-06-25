#pragma once

namespace LIBMEMBUS_NS
{

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
        vidview(char* p, int64_t sz, int64_t sw, int64_t w, int64_t h)
            : m_ptr(p), m_size(sz), m_sw(sw), m_w(w), m_h(h)
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
    };

    /// Main header
    enum HeaderVal
    {
        hv_size         = 0,
        hv_ptr          = 1 * sizeof(int64_t),
        hv_width        = 2 * sizeof(int64_t),
        hv_height       = 3 * sizeof(int64_t),
        hv_scanwidth    = 4 * sizeof(int64_t),
        hv_bpp          = 5 * sizeof(int64_t),
        hv_fps          = 6 * sizeof(int64_t),
        hv_bufs         = 7 * sizeof(int64_t),
        hv_blocksz      = 8 * sizeof(int64_t),
        hv_last         = 9 * sizeof(int64_t)
    };

    /// Frame header
    enum FrameHeaderVal
    {
        fv_size         = 0,
        fv_vpts         = 1 * sizeof(int64_t),
        fv_apts         = 2 * sizeof(int64_t),
        fv_last         = 3 * sizeof(int64_t)
    };

public:

    /// Constructor
    memvid() {};

    /// Close the buffer
    ~memvid() { close(); }

    /// Creates / attaches to an image ring buffer in memory
    bool open(const std::string &sName, bool bCreate,
              int64_t w, int64_t h, int64_t bpp,
              int64_t fps, int64_t bufs);

    /// Open an existing image share
    bool open_existing(const std::string &sName);

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

    /// Image width
    int64_t getWidth();

    /// Image width
    int64_t getHeight();

    /// Image bpp
    int64_t getBpp();

    /// Image fps
    int64_t getFps();

private:

    /// The memory map
    memmap      m_mem;

};

}; // end namespace
