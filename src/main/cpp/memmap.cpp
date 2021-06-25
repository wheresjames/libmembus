
#include "libmembus/libmembus_internal.h"

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
    m_bExisting = false;
    _d = new memmap_data();
}

memmap::~memmap()
{
    close();
    if (_d)
        delete _d, _d = 0;

}

char* memmap::data()
{
    return (char*)(_d->m_map.get() ? _d->m_map->get_address() : 0);
}

bool memmap::isOpen()
{
    return _d->m_map.get() ? true : false;
}

void memmap::close()
{
    // Remove share if needed
    if (_d->m_mem.get() && !m_bExisting && 0 < m_sName.length())
        _d->m_mem->remove(m_sName.c_str());

    // Lose the old object
    _d->m_map.reset();
    _d->m_mem.reset();

    m_size = 0;
    m_bExisting = false;
    m_sName.clear();
}

bool memmap::open(const std::string &sName, int64_t nSize, bool bCreate, bool bNew)
{
    // Out with the old
    close();

    // Delete any existing share if we're creating
    if (bCreate && bNew)
        shared_memory_object().remove(sName.c_str());

    // Try to open existing share
    bool bFail = false;
    try
    {   m_bExisting = true;
        _d->m_mem.reset(new shared_memory_object(open_only, sName.c_str(), read_write));
    } catch(...) { bFail = true; }

    // Did we get anything?
    if (bFail && bCreate)
    {
        bFail = false;
        try
        {   m_bExisting = false;
            _d->m_mem.reset(new shared_memory_object(create_only, sName.c_str(), read_write));
        } catch(...) { bFail = true; }
    }

    // If we failed
    if (bFail)
    {   close();
        return false;
    }

    // Set size?
    if (bCreate && 0 < nSize)
        _d->m_mem->truncate(nSize);

    // Map the region
    _d->m_map.reset(new mapped_region(*_d->m_mem, read_write));

    m_size = nSize;
    m_sName = sName;

    return true;
}

int64_t memmap::write(const std::string &sStr)
{
    if (0 >= m_size)
        return 0;

    char *p = data();
    if (!p)
        return 0;

    int64_t len = sStr.length();
    if (len > m_size)
        len = m_size;

    if (0 < len)
        memcpy(p, sStr.c_str(), len);

    return len;
}

std::string memmap::read(int64_t sz)
{
    if (0 >= m_size)
        return 0;

    char *p = data();
    if (!p)
        return std::string();

    int64_t len = size();
    if (0 < sz && len > sz)
        len = sz;

    return std::string(p, len);
}

}; // end namespace
