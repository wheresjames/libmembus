#pragma once

namespace LIBMEMBUS_NS
{

/** Memory mapped buffer
*/
class memmap
{
public:

    /// Constructor
    memmap();

    /// Destructor
    virtual ~memmap();

    /// Close the shared memory space
    void close();

    /// Open a shared memory space
    bool open(const std::string &sName, int64_t nSize, bool bCreate = false, bool bNew = false);

    /// Write string to buffer
    int64_t write(const std::string &sStr);

    /// Read string from buffer
    std::string read(int64_t sz = -1);

    /// Size of the buffer
    int64_t size() { return m_size; }

    /// Get raw data pointer
    char* data();

    /// Returns true if the share already existed
    bool existing() { return m_bExisting; }

    /// Returns true if a share is open
    bool isOpen();

    /// Returns the share name
    std::string name() { return m_sName; }

private:

    /// Size of the shared memory
    int64_t                                 m_size;

    /// true if share already existed
    bool                                    m_bExisting;

    /// Share name
    std::string                             m_sName;

    /// Private data
    struct memmap_data                      *_d;

};

}; // end namespace
