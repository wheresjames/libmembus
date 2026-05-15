#pragma once

namespace LIBMEMBUS_NS
{

/** Raw named shared-memory buffer.
 *
 *  Wraps a POSIX (or Windows) named shared-memory object and its mapped region.
 *  One process creates the region; any number of processes may attach to it.
 *  The creator owns the OS namespace entry and removes it from the filesystem
 *  on close().  Attaching processes do not remove it on close().
 *
 *  @code
 *      mmb::memmap writer, reader;
 *      writer.open("/my_share", 1024, true, true);
 *      reader.open("/my_share", 1024, false, false);
 *      writer.write("hello");
 *      std::string s = reader.read(5);   // "hello"
 *  @endcode
 */
class memmap
{
public:

    /// Construct an un-opened handle.
    memmap();

    /// Destructor; calls close().
    virtual ~memmap();

    /** Close the mapped region and release all resources.
     *
     *  If this handle created the share (existing() == false) the share is
     *  also removed from the OS namespace.  Safe to call on an already-closed
     *  handle.
     */
    void close();

    /** Open or create a named shared-memory region.
     *
     *  Tries to open an existing share first.  If that fails and bCreate is
     *  true, a new share is created.  The attaching path never resizes an
     *  existing share; size() will reflect the actual mapped size.
     *
     *  @param sName     Share name.  On POSIX, must start with '/'.
     *  @param nSize     Requested size in bytes.  Used only when creating a new
     *                   share; ignored when attaching to an existing one.
     *  @param bCreate   Create the share if it does not already exist.
     *  @param bNew      Remove any pre-existing share before creating (implies bCreate).
     *  @param bReadOnly Map the region read-only.  Forces bCreate=false and bNew=false.
     *  @returns true on success; false on failure (see last_error()).
     */
    bool open(const std::string &sName, int64_t nSize,
              bool bCreate = false, bool bNew = false, bool bReadOnly = false);

    /** Remove a named shared-memory object from the OS namespace.
     *  @param sName  Share name to remove.
     *  @returns true if the object was found and successfully removed.
     */
    static bool remove(const std::string &sName);

    /** Write the contents of a string into the mapped buffer.
     *
     *  Writes from offset 0; truncates to size() if the string is longer.
     *  No null-terminator is written.
     *
     *  @param sStr  Data to write.
     *  @returns Number of bytes actually written; 0 if the map is not open.
     */
    int64_t write(const std::string &sStr);

    /** Read bytes from the mapped buffer as a string.
     *
     *  Always reads from offset 0.  No null-terminator is expected in the buffer.
     *
     *  @param sz  Maximum number of bytes to read.  Pass -1 (the default) to
     *             read the entire buffer.
     *  @returns String containing the requested bytes; empty if not open.
     */
    std::string read(int64_t sz = -1);

    /// Size of the mapped region in bytes; 0 if not open.
    int64_t size() { return m_size; }

    /// Raw pointer to the start of the mapped region; nullptr if not open.
    char* data();

    /** Returns true if this handle attached to an already-existing share.
     *
     *  A handle that created the share returns false; one that attached to an
     *  existing share returns true.  Only the creator removes the share on close().
     */
    bool existing() { return m_bExisting; }

    /// Returns true if a shared-memory region is currently mapped.
    bool isOpen();

    /// Returns the name this handle was opened with; empty if not open.
    std::string name() { return m_sName; }

private:

    /// Size of the mapped region in bytes.
    int64_t                                 m_size;

    /// True if the share already existed when this handle opened it.
    bool                                    m_bExisting;

    /// Share name used to open this handle.
    std::string                             m_sName;

    /// Private implementation data (Boost.Interprocess objects).
    struct memmap_data                      *_d;

};

}; // end namespace
