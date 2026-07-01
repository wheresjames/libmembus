#pragma once

namespace LIBMEMBUS_NS
{

/** Per-record kind tag stored in a mempkt descriptor (dv_kind). */
enum class pkt_kind : int64_t
{
    data  = 0, ///< Opaque / application data.
    video = 1, ///< Compressed video access unit.
    audio = 2  ///< Compressed audio frame.
};

/** Lock-free variable-length record ring in shared memory (single-writer, multi-reader).
 *
 *  Unlike memvid/memaud, records are variable-length and opaque — intended for
 *  compressed / packetized streams (MJPEG, H.264, RTSP payloads, muxed A/V).
 *  A fixed descriptor ring provides O(1) addressing and overrun detection; the
 *  variable payloads live in a separate packed byte arena.  See MB-MEMPKT.md §6.
 *
 *  Torn-read safety: because the arena is overwritten in place, readers must
 *  copy a record out and then re-check the monotonic arena write-cursor to
 *  confirm the bytes were not lapped mid-copy (getRecord() does this).
 */
class mempkt
{
public:

    /// Shared class discriminator written to hv_type.  See MB-MEMPKT.md §10.1.
    static const int64_t k_type    = 3;      ///< mempkt
    /// Shared-memory magic constant written to hv_magic ('MBUS').
    static const int64_t k_magic   = 0x5355424dLL;
    /// Current header layout version written to hv_version.
    static const int64_t k_version = 2;
    /// Default record alignment when the caller passes 0.
    static const int64_t k_defAlign = 64;

    /** Byte offsets of fields in the shared-memory main header (§10.1 prefix + class fields). */
    enum HeaderVal
    {
        // ---- shared prefix (identical offsets in all classes) ----
        hv_magic        =  0 * sizeof(int64_t),  ///< int64_t: k_magic.
        hv_type         =  1 * sizeof(int64_t),  ///< int64_t: class discriminator (k_type).
        hv_version      =  2 * sizeof(int64_t),  ///< int64_t: header layout version.
        hv_size         =  3 * sizeof(int64_t),  ///< int64_t: total allocated size.
        hv_ptr          =  4 * sizeof(int64_t),  ///< int64_t: descriptor write-pointer slot index (atomic).
        hv_seq          =  5 * sizeof(int64_t),  ///< int64_t: monotonic write-sequence counter (atomic).
        hv_id           =  6 * sizeof(int64_t),  ///< int64_t: random session ID.
        hv_bufs         =  7 * sizeof(int64_t),  ///< int64_t: number of descriptor slots in the ring.
        hv_fourcc       =  8 * sizeof(int64_t),  ///< int64_t: fourcc (low 32 bits); 0 = none.
        hv_guid_lo      =  9 * sizeof(int64_t),  ///< int64_t: GUID bytes 0..7.
        hv_guid_hi      = 10 * sizeof(int64_t),  ///< int64_t: GUID bytes 8..15.
        hv_align        = 11 * sizeof(int64_t),  ///< int64_t: record alignment (power of two, >= 8).
        hv_metasize     = 12 * sizeof(int64_t),  ///< int64_t: main user buffer size in bytes (0 = none).
        hv_reserved0    = 13 * sizeof(int64_t),  ///< int64_t: spare.
        hv_reserved7    = 20 * sizeof(int64_t),  ///< int64_t: last spare slot.
        hv_common_end   = 21 * sizeof(int64_t),  ///< End of shared prefix.

        // ---- mempkt class-specific ----
        hv_arenasz      = 21 * sizeof(int64_t),  ///< int64_t: payload arena size in bytes.
        hv_maxrec       = 22 * sizeof(int64_t),  ///< int64_t: largest single record (payload + meta) accepted.
        hv_wcursor      = 23 * sizeof(int64_t),  ///< int64_t: live monotonic arena write-cursor (atomic).
        hv_descstride   = 24 * sizeof(int64_t),  ///< int64_t: descriptor slot stride in bytes.
        hv_descoffset   = 25 * sizeof(int64_t),  ///< int64_t: computed base offset of DESCRIPTOR_RING.
        hv_arenaoffset  = 26 * sizeof(int64_t),  ///< int64_t: computed base offset of PAYLOAD_ARENA.
        hv_last         = 27 * sizeof(int64_t)   ///< Total fixed header size; MAINUSERBUF begins here.
    };

    /** Byte offsets of fields within a single descriptor slot (§10.4). */
    enum DescVal
    {
        dv_offset       = 0,                   ///< int64_t: payload offset in the arena (post pad-to-end).
        dv_len          = 1 * sizeof(int64_t), ///< int64_t: payload length in bytes.
        dv_wcursor      = 2 * sizeof(int64_t), ///< int64_t: arena write-cursor snapshot at record start.
        dv_pts          = 3 * sizeof(int64_t), ///< int64_t: presentation timestamp.
        dv_kind         = 4 * sizeof(int64_t), ///< int64_t: pkt_kind.
        dv_track        = 5 * sizeof(int64_t), ///< int64_t: track/stream id (low 32 bits may carry fourcc).
        dv_userlen      = 6 * sizeof(int64_t), ///< int64_t: per-record metadata length (stored before payload).
        dv_seq          = 7 * sizeof(int64_t), ///< int64_t: publish sequence (stamped last, release-store).
        dv_last         = 8 * sizeof(int64_t)  ///< Descriptor slot size.
    };

    /** Metadata describing a record returned by getRecord(). */
    struct recinfo
    {
        int64_t seq     = 0;   ///< Publish sequence of the record.
        int64_t wcursor = 0;   ///< Arena write-cursor at record start.
        int64_t pts     = 0;   ///< Presentation timestamp.
        int64_t kind    = 0;   ///< pkt_kind.
        int64_t track   = 0;   ///< Track/stream id.
        int64_t len     = 0;   ///< Payload length in bytes.
        int64_t userlen = 0;   ///< Per-record metadata length in bytes.
    };

public:

    /// Construct an un-opened handle.
    mempkt() {};

    /// Destructor; calls close().
    ~mempkt() { close(); }

    /** Create or attach to a variable-length record ring.
     *
     *  @param sName    Share name (POSIX: must start with '/').
     *  @param bCreate  Create the share.  Pass false to attach.
     *  @param bufs     Number of descriptor slots (> 0).
     *  @param arenasz  Payload arena size in bytes.  Must be >= maxrec; size it
     *                  with headroom (several × maxrec) so readers are not
     *                  livelocked — see MB-MEMPKT.md §6.1.
     *  @param maxrec   Largest single record (payload + metadata) accepted.
     *  @param align    Record alignment (power of two, >= 8); 0 = default 64.
     *  @param fourcc   Fourcc identity (0 = none).
     *  @param guid     Optional 16-byte GUID identity (null = none).
     *  @param meta     Optional main user buffer bytes copied in at create.
     *  @param metasz   Size of @p meta in bytes (0 = none).
     *  @returns true on success; false on failure (see last_error()).
     */
    bool open(const std::string &sName, bool bCreate,
              int64_t bufs, int64_t arenasz, int64_t maxrec,
              int64_t align = 0, uint32_t fourcc = 0, const uint8_t *guid = nullptr,
              const void *meta = nullptr, int64_t metasz = 0);

    /** Attach to an existing record ring, read-only, validating magic/type/layout. */
    bool open_existing(const std::string &sName);

    /** Remove a stale share from the OS namespace. */
    static bool remove(const std::string &sName) { return memmap::remove(sName); }

    /// Close the ring and release all resources.
    void close();

    /// Returns true if a ring is currently open.
    bool isOpen() { return m_mem.isOpen(); }

    /// Returns true if this handle attached to an already-existing share.
    bool existing() { return m_mem.existing(); }

    /** Publish a record.
     *
     *  Copies @p meta (if any) followed by @p payload into the arena, fills the
     *  current descriptor slot, publishes the arena write-cursor, then stamps the
     *  slot sequence and advances the descriptor write pointer.
     *
     *  @param payload  Payload bytes.
     *  @param len      Payload length.
     *  @param kind     pkt_kind tag.
     *  @param track    Track/stream id.
     *  @param pts      Presentation timestamp.
     *  @param meta     Optional per-record metadata bytes.
     *  @param metalen  Per-record metadata length.
     *  @returns The new descriptor write-pointer slot index, or -1 on failure
     *           (not open, read-only, or payload+meta exceeds maxrec).
     */
    int64_t write(const void *payload, int64_t len,
                  pkt_kind kind = pkt_kind::data, int64_t track = 0, int64_t pts = 0,
                  const void *meta = nullptr, int64_t metalen = 0);

    /** Convenience overload writing a std::string payload. */
    int64_t write(const std::string &payload,
                  pkt_kind kind = pkt_kind::data, int64_t track = 0, int64_t pts = 0)
    { return write(payload.data(), (int64_t)payload.size(), kind, track, pts); }

    /** Copy the record in slot @p idx out with torn-read validation.
     *
     *  Performs a seqlock read of the descriptor, copies the payload (and any
     *  per-record metadata) out of the arena, then re-checks the arena write
     *  cursor.  @returns false if the descriptor was being rewritten, the record
     *  bytes were lapped mid-copy, or the slot is out of bounds — in which case
     *  @p payload / @p meta must not be used.
     *
     *  @param idx      Descriptor slot index; wrapped modulo getBufs().
     *  @param payload  Receives the payload bytes on success.
     *  @param meta     Receives the per-record metadata bytes on success (may be empty).
     *  @param info     Receives the record metadata (seq, pts, kind, …).
     *  @returns true on a clean read; false on torn/lapped/invalid.
     */
    bool getRecord(int64_t idx, std::string &payload, std::string &meta, recinfo &info);

    /** Return the number of descriptor slots.  0 if not open. */
    int64_t getBufs();

    /** Return the current descriptor write-pointer slot index offset by @p offset. */
    int64_t getPtr(int64_t offset);

    /** Return the global monotonic write-sequence counter.  -1 if not open. */
    int64_t getSeq();

    /** Return the sequence stamped into descriptor slot @p idx (dv_seq). */
    int64_t getFrameSeq(int64_t idx);

    /** Poll until getSeq() advances beyond @p lastSeq or @p wait_ms elapses. */
    bool waitForFrame(uint64_t wait_ms, int64_t lastSeq);

    /** Return the arena size in bytes.  0 if not open. */
    int64_t getArenaSize();

    /** Return the maximum record size in bytes.  0 if not open. */
    int64_t getMaxRec();

    /** Return the live arena write-cursor (total bytes ever written). */
    int64_t getWcursor();

    /** Return the random session ID.  0 if not open. */
    int64_t getSessionId();

    /** Return the header layout version.  -1 if not open. */
    int64_t getVersion();

    /** Return the fourcc identity (low 32 bits).  0 if none. */
    uint32_t getFourcc();

    /** Copy the 16-byte GUID identity into @p out.  @returns true if non-zero. */
    bool getGuid(uint8_t out[16]);

    /** Return a pointer to the main user metadata buffer, or null if none. */
    const char *getMeta();

    /** Return the size of the main user metadata buffer in bytes.  0 if none. */
    int64_t getMetaSize();

private:

    /// Advance the descriptor write pointer to (ptr) wrapped modulo bufs.
    int64_t setPtr(int64_t ptr);

    /// The underlying memory map.
    memmap      m_mem;

};

}; // end namespace
