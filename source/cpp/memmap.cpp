
#include "libmembus-internal.h"

namespace LIBMEMBUS_NS
{

struct memmap_data
{
    /// Shared pointer to memory object
    std::shared_ptr<shared_memory_object>   m_mem;

    /// Shared pointer to mapped memory region
    std::shared_ptr<mapped_region>          m_map;
};

memmap::memmap()
{
    m_size = 0;
    m_bExisting = false;
    _d = new memmap_data();
    set_last_error(errc::ok);
}

memmap::~memmap()
{
    close();
    if (_d)
        delete _d, _d = 0;

}

char* memmap::data()
{
    return (char*)(_d && _d->m_map.get() ? _d->m_map->get_address() : 0);
}

bool memmap::isOpen()
{
    return _d && _d->m_map.get() ? true : false;
}

void memmap::close()
{
    if (!_d)
        return;

    // Remove share if needed
    if (_d->m_mem.get() && !m_bExisting && 0 < m_sName.length())
        shared_memory_object::remove(m_sName.c_str());

    // Lose the old object
    _d->m_map.reset();
    _d->m_mem.reset();

    m_size = 0;
    m_bExisting = false;
    m_sName.clear();
}

bool memmap::open(const std::string &sName, int64_t nSize, bool bCreate, bool bNew, bool bReadOnly)
{
    set_last_error(errc::ok);

    // Out with the old
    close();

    // Read-only opens cannot create or recreate a share
    if (bReadOnly)
        bCreate = bNew = false;

    // Delete any existing share if we're creating
    if (bCreate && bNew)
    {
        try { shared_memory_object::remove(sName.c_str()); }
        catch(...) {}
    }

    const boost::interprocess::mode_t accessMode = bReadOnly ? read_only : read_write;

    // Try to open existing share
    bool bFail = false;
    try
    {   m_bExisting = true;
        _d->m_mem.reset(new shared_memory_object(open_only, sName.c_str(), accessMode));
    } catch(...) { bFail = true; set_last_error(errc::open_failed); }

    // Did we get anything?
    if (bFail && bCreate)
    {
        bFail = false;
        try
        {   m_bExisting = false;
            _d->m_mem.reset(new shared_memory_object(create_only, sName.c_str(), read_write));
        } catch(...) { bFail = true; set_last_error(errc::create_failed); }
    }

    // If we failed
    if (bFail)
    {   close();
        return false;
    }

    try
    {
        // Size newly-created shares only.  Attaching must never resize a live share.
        if (!m_bExisting && 0 < nSize)
            _d->m_mem->truncate(nSize);

        // Map the region
        _d->m_map.reset(new mapped_region(*_d->m_mem, accessMode));
    }
    catch(...)
    {
        close();
        set_last_error(errc::map_failed);
        return false;
    }

    // Use the actual mapped size, not the caller-supplied nSize, so that
    // reads/writes are bounded by reality when attaching to an existing share.
    m_size = (int64_t)_d->m_map->get_size();
    m_sName = sName;

    set_last_error(errc::ok);
    return true;
}

bool memmap::remove(const std::string &sName)
{
    set_last_error(errc::ok);
    try
    {
        return shared_memory_object().remove(sName.c_str());
    }
    catch(...)
    {
        set_last_error(errc::open_failed);
        return false;
    }
}

int64_t memmap::write(const std::string &sStr)
{
    if (0 >= m_size)
    {
        set_last_error(errc::not_open);
        return 0;
    }

    char *p = data();
    if (!p)
    {
        set_last_error(errc::not_open);
        return 0;
    }

    int64_t len = sStr.length();
    if (len > m_size)
        len = m_size;

    if (0 < len)
        memcpy(p, sStr.c_str(), len);

    set_last_error(errc::ok);
    return len;
}

std::string memmap::read(int64_t sz)
{
    if (0 >= m_size)
    {
        set_last_error(errc::not_open);
        return std::string();
    }

    char *p = data();
    if (!p)
    {
        set_last_error(errc::not_open);
        return std::string();
    }

    int64_t len = size();
    if (0 < sz && len > sz)
        len = sz;

    set_last_error(errc::ok);
    return std::string(p, len);
}

}; // end namespace
