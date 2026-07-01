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

/** Return the name string for an audio_format enum value (e.g. "S16LE"). */
const char *audio_format_name(audio_format fmt);

/** Return the number of bytes per sample for one channel of an audio_format. */
int64_t audio_format_bytes_per_sample(audio_format fmt);

/** Lock-free multi-buffer PCM audio ring buffer in shared memory.
 *
 *  Follows the same design as memvid (single-writer, multiple-reader): the
 *  writer creates the share and calls next() after filling each buffer slot;
 *  readers attach with open_existing() and consume slots independently.
 *
 *  Buffer size: each slot holds ceil(sampleRate / fps) samples per channel.
 */
class memaud
{
public:

    /// Shared class discriminator written to hv_type.  See MB-MEMPKT.md §10.1.
    static const int64_t k_type    = 2;      ///< memaud
    /// Shared-memory magic constant written to hv_magic ('MBUS').
    static const int64_t k_magic   = 0x5355424dLL;
    /// Current header layout version written to hv_version.
    static const int64_t k_version = 2;
    /// Default payload alignment when the caller passes 0.
    static const int64_t k_defAlign = 64;

    /** Round @p val up to the nearest multiple of @p unit. */
    template < typename T >
        static T fitTo( T val, T unit )
    {	if ( !unit ) return 0;
        T i = (T)( val / unit );
        return ( ( i * unit ) >= val ) ? i : i + 1;
    }

    /** Return the absolute value of @p v. */
    template < typename T >
        static T abs( T v )
    {   return ( v >= 0 ) ? v : -v; }

    /** Read-only view of a single buffer slot in the audio ring. */
    class audview
    {
    public:

        /** Construct a view over a raw PCM buffer. */
        audview(char* p, int64_t sz, int64_t ch, audio_format fmt)
            : m_ptr(p), m_size(sz), m_ch(ch), m_format(fmt)
        {
        }

        /** Copy constructor. */
        audview(const audview &r)
        {
            (*this) = r;
        }

        /// Destructor.
        virtual ~audview() {}

        /** Copy assignment. */
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
     *  The first block is the shared header prefix, identical to memvid/mempkt
     *  (MB-MEMPKT.md §10.1).  hv_useroffset / hv_dataoffset are computed at
     *  open() and stored.
     */
    enum HeaderVal
    {
        // ---- shared prefix (identical offsets in all classes) ----
        hv_magic        =  0 * sizeof(int64_t),  ///< int64_t: k_magic.
        hv_type         =  1 * sizeof(int64_t),  ///< int64_t: class discriminator (k_type).
        hv_version      =  2 * sizeof(int64_t),  ///< int64_t: header layout version.
        hv_size         =  3 * sizeof(int64_t),  ///< int64_t: total allocated size.
        hv_ptr          =  4 * sizeof(int64_t),  ///< int64_t: current write-pointer slot index (atomic).
        hv_seq          =  5 * sizeof(int64_t),  ///< int64_t: monotonic write-sequence counter (atomic).
        hv_id           =  6 * sizeof(int64_t),  ///< int64_t: random session ID.
        hv_bufs         =  7 * sizeof(int64_t),  ///< int64_t: number of buffer slots in the ring.
        hv_fourcc       =  8 * sizeof(int64_t),  ///< int64_t: fourcc (low 32 bits); 0 = none.
        hv_guid_lo      =  9 * sizeof(int64_t),  ///< int64_t: GUID bytes 0..7.
        hv_guid_hi      = 10 * sizeof(int64_t),  ///< int64_t: GUID bytes 8..15.
        hv_align        = 11 * sizeof(int64_t),  ///< int64_t: payload alignment (power of two, >= 8).
        hv_metasize     = 12 * sizeof(int64_t),  ///< int64_t: main user buffer size in bytes (0 = none).
        hv_reserved0    = 13 * sizeof(int64_t),  ///< int64_t: spare.
        hv_reserved7    = 20 * sizeof(int64_t),  ///< int64_t: last spare slot.
        hv_common_end   = 21 * sizeof(int64_t),  ///< End of shared prefix.

        // ---- memaud class-specific ----
        hv_ch           = 21 * sizeof(int64_t),  ///< int64_t: number of channels.
        hv_samplerate   = 22 * sizeof(int64_t),  ///< int64_t: sample rate in Hz.
        hv_format       = 23 * sizeof(int64_t),  ///< int64_t: audio_format cast to int64_t.
        hv_fps          = 24 * sizeof(int64_t),  ///< int64_t: nominal buffers per second.
        hv_blocksz      = 25 * sizeof(int64_t),  ///< int64_t: size of one buffer slot in bytes (aligned).
        hv_frameextra   = 26 * sizeof(int64_t),  ///< int64_t: per-frame user buffer stride in bytes (0 = none).
        hv_useroffset   = 27 * sizeof(int64_t),  ///< int64_t: computed base offset of per-frame user region.
        hv_dataoffset   = 28 * sizeof(int64_t),  ///< int64_t: computed base offset of PCM data region.
        hv_last         = 29 * sizeof(int64_t)   ///< Total fixed header size; MAINUSERBUF begins here.
    };

    /** Byte offsets of fields within a single buffer slot, relative to the slot start.
     *
     *  Layout: [fv_size][fv_pts][fv_userlen][fv_seq] then padding up to hv_align,
     *  then PCM sample data.
     */
    enum FrameHeaderVal
    {
        fv_size         = 0,                   ///< int64_t: (reserved; matches slot position in ring).
        fv_pts          = 1 * sizeof(int64_t), ///< int64_t: presentation timestamp (application-defined).
        fv_userlen      = 2 * sizeof(int64_t), ///< int64_t: used bytes in this slot's per-frame user buffer.
        fv_seq          = 3 * sizeof(int64_t), ///< int64_t: sequence number stamped by next().
        fv_last         = 4 * sizeof(int64_t)  ///< Per-slot header size; padded up to hv_align before PCM data.
    };

public:

    /// Construct an un-opened handle.
    memaud() {};

    /// Destructor; calls close().
    ~memaud() { close(); }

    /** Create or attach to an audio ring buffer.
     *
     *  The trailing parameters are additive and default to today's behaviour.
     *
     *  @param sName       Share name (POSIX: must start with '/').
     *  @param bCreate     Create the share.  Pass false to attach.
     *  @param ch          Number of interleaved channels (must be > 0).
     *  @param fmt         Sample format.
     *  @param sampleRate  Sample rate in Hz (must be > 0).
     *  @param fps         Buffers per second (must be > 0 and <= sampleRate).
     *  @param bufs        Number of buffer slots in the ring (must be > 0).
     *  @param align       Payload alignment (power of two, >= 8); 0 = default 64.
     *  @param frameextra  Per-frame user buffer size in bytes (0 = none).
     *  @param fourcc      Fourcc identity (0 = none).
     *  @param guid        Optional 16-byte GUID identity (null = none).
     *  @param meta        Optional main user buffer bytes copied in at create.
     *  @param metasz      Size of @p meta in bytes (0 = none).
     *  @returns true on success; false on failure (see last_error()).
     */
    bool open(const std::string &sName, bool bCreate, int64_t ch, audio_format fmt,
              int64_t sampleRate, int64_t fps, int64_t bufs,
              int64_t align = 0, int64_t frameextra = 0,
              uint32_t fourcc = 0, const uint8_t *guid = nullptr,
              const void *meta = nullptr, int64_t metasz = 0);

    /** Attach to an existing audio ring buffer without knowing its parameters. */
    bool open_existing(const std::string &sName);

    /** Remove a stale audio share from the OS namespace. */
    static bool remove(const std::string &sName) { return memmap::remove(sName); }

    /// Close the ring buffer and release all resources.
    void close();

    /// Returns true if this handle attached to an already-existing share.
    bool existing() { return m_mem.existing(); }

    /// Returns true if an audio ring buffer is currently open.
    bool isOpen() { return m_mem.isOpen(); }

    /** Return a view of the buffer slot at ring index @p idx. */
    audview getBuf(int64_t idx) noexcept(false);

    /** Return the number of buffer slots in the ring.  0 if not open. */
    int64_t getBufs();

    /** Fill a buffer slot with a constant byte value (e.g. 0 for silence). */
    bool fill(int64_t idx, int col);

    /** Return the current write-pointer slot index offset by @p offset. */
    int64_t getPtr(int64_t offset);

    /** Set the write-pointer slot index to @p ptr (wrapped modulo getBufs()). */
    int64_t setPtr(int64_t ptr);

    /** Return the signed circular distance from (ptr + bias) to @p pos. */
    int64_t getPtrErr(int64_t pos, int64_t bias);

    /** Stamp the current slot's sequence number, then advance the write pointer by @p inc. */
    int64_t next(int64_t inc);

    /** Poll until getSeq() advances beyond @p lastSeq or @p wait_ms elapses. */
    bool waitForFrame(uint64_t wait_ms, int64_t lastSeq);

    /** Return the number of channels.  0 if not open. */
    int64_t getChannels();

    /** Return the sample format stored in the header. */
    audio_format getFormat();

    /** Return the sample format name string (e.g. "S16LE"). */
    const char *getFormatName();

    /** Return the number of bytes per sample for one channel. */
    int64_t getBytesPerSample();

    /** Return the sample rate in Hz stored in the header.  0 if not open. */
    int64_t getSampleRate();

    /** Return the nominal buffer rate (buffers per second) stored in the header. */
    int64_t getFps();

    /** Return the PCM data size of a single buffer slot in bytes. */
    int64_t getBufSize();

    /** Return the random session ID written when the share was created.  0 if not open. */
    int64_t getSessionId();

    /** Return the header layout version (hv_version).  -1 if not open. */
    int64_t getVersion();

    /** Return the payload alignment (hv_align).  0 if not open. */
    int64_t getAlign();

    /** Return the fourcc identity (low 32 bits).  0 if none. */
    uint32_t getFourcc();

    /** Copy the 16-byte GUID identity into @p out.  @returns true if non-zero. */
    bool getGuid(uint8_t out[16]);

    /** Return a pointer to the main user metadata buffer, or null if none. */
    const char *getMeta();

    /** Return the size of the main user metadata buffer in bytes.  0 if none. */
    int64_t getMetaSize();

    /** Return the per-frame user buffer stride (hv_frameextra).  0 if none. */
    int64_t getFrameExtra();

    /** Copy up to getFrameExtra() bytes into slot @p idx's user buffer and set its length. */
    bool setUserData(int64_t idx, const void *data, int64_t len);

    /** Return a pointer to slot @p idx's per-frame user buffer, or null if none. */
    const char *getUserData(int64_t idx);

    /** Return the used length of slot @p idx's per-frame user buffer (fv_userlen). */
    int64_t getUserLen(int64_t idx);

    /** Write the presentation timestamp for slot @p idx.  Call before next(). */
    bool setPts(int64_t idx, int64_t pts);

    /** Return the presentation timestamp stored in slot @p idx. */
    int64_t getPts(int64_t idx);

    /** Return the global monotonic write-sequence counter.  -1 if not open. */
    int64_t getSeq();

    /** Return the sequence number most recently stamped into slot @p idx by next(). */
    int64_t getFrameSeq(int64_t idx);

private:

    /// The underlying memory map.
    memmap      m_mem;

};

}; // end namespace
