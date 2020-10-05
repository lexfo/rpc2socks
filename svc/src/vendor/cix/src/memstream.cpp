// CIX C++ library
// Copyright (c) Jean-Charles Lefebvre
// SPDX-License-Identifier: MIT

#include <cix/cix>
#include <cix/detail/intro.h>

namespace cix {

memstream::memstream(size_type grow_size)
    : m_buffer{nullptr}, m_grow_size{grow_size}
    , m_size{0}, m_rpos{0}, m_wpos{0}
    , m_view_size{0}
{
}


memstream::memstream(const_pointer data, size_type size)
    : memstream()
{
    this->open_read(data, size);
}


memstream& memstream::open_read(const_pointer data, size_type size)
{
    assert(size > 0);

    this->clear(true);

    if (size > 0)
    {
        m_buffer = const_cast<pointer>(data);
        m_size = size;
        m_view_size = size;
    }

    return *this;
}


bool memstream::read_only() const
{
    return m_view_size != 0;
}


memstream& memstream::clear(bool free_memory)
{
    m_buffer = nullptr;
    m_size = 0;
    m_rpos = 0;
    m_wpos = 0;
    m_view_size = 0;

    if (free_memory)
    {
        container tmp;
        m_container.swap(tmp);
    }

    return *this;
}


bool memstream::empty() const
{
    return m_size == 0;
}


memstream::size_type memstream::size() const
{
    return m_size;
}


memstream::const_pointer memstream::data() const
{
    assert(m_size > 0);
    return &m_buffer[0];
}


memstream::const_pointer memstream::begin() const
{
    assert(m_size > 0);
    return &m_buffer[0];
}


memstream::const_pointer memstream::end() const
{
    assert(m_size > 0);
    return &m_buffer[m_size];
}


memstream::const_reference memstream::operator[](pos_type pos) const
{
    assert(pos < m_size);
    return m_buffer[pos];
}


memstream::pos_type memstream::tellr() const
{
    return m_rpos;
}


memstream::pos_type memstream::tellw() const
{
    assert(!this->read_only());
    return m_wpos;
}


memstream& memstream::seekr(pos_type position)
{
    return this->seek_impl(m_rpos, position);
}


memstream& memstream::seekr(off_type offset, seekdir dir)
{
    return this->seek_impl(m_rpos, offset, dir);
}


memstream& memstream::seekw(pos_type position)
{
    assert(!this->read_only());
    return this->seek_impl(m_wpos, position);
}


memstream& memstream::seekw(off_type offset, seekdir dir)
{
    assert(!this->read_only());
    return this->seek_impl(m_wpos, offset, dir);
}


memstream& memstream::write(const void* data, size_type size)
{
    assert(!this->read_only());

    if (!this->read_only())
    {
        assert(size < npos);

        if (size > 0 && size < npos)
        {
            this->grow(size);
            std::memcpy(&m_buffer[m_wpos], data, size);
            m_wpos += size;
            if (m_wpos > m_size)
                m_size = m_wpos;
        }
    }
    return *this;
}


memstream& memstream::read(void* dest, size_type size)
{
    if (!this->ensure(size))
        CIX_THROW_BADARG("reading beyond eof");

    std::memcpy(dest, &m_buffer[m_rpos], size);
    m_rpos += size;

    return *this;
}


bool memstream::peek_cmp(
    const void* expected_data, size_type expected_size,
    bool advance_rpos_on_match)
{
    if (!expected_data || !expected_size)
    {
        assert(0);
        return false;
    }

    if (!this->ensure(expected_size))
        return false;

    if (0 != std::memcmp(&m_buffer[m_rpos], expected_data, expected_size))
        return false;

    if (advance_rpos_on_match)
        m_rpos += expected_size;

    return true;
}


memstream& memstream::write(const std::uint8_t value)
{
    return this->write(&value, sizeof(value));
}

memstream& memstream::read(std::uint8_t& value)
{
    return this->read(&value, sizeof(value));
}


memstream::pointer memstream::prepare_write(size_type estimated_extra_size)
{
    if (this->read_only())
    {
        assert(0);
        return nullptr;
    }
    else
    {
        this->grow(estimated_extra_size);
        return &m_buffer[m_wpos];
    }
}


memstream& memstream::finalize_write(size_type written)
{
    assert(!this->read_only());
    if (!this->read_only())
    {
        if ((m_container.size() - m_wpos) < written)
            CIX_THROW_BADARG("wrote out of boundaries");

        m_wpos += written;
        if (m_wpos > m_size)
            m_size = m_wpos;
    }

    return *this;
}


void memstream::grow(size_type required_extra_size)
{
    assert(!this->read_only());

    if (!this->read_only())
    {
        assert(m_size <= m_container.size());
        assert(m_rpos <= m_container.size());
        assert(m_wpos <= m_container.size());

        if ((m_container.size() - m_wpos) < required_extra_size)
        {
            m_container.resize(
                m_wpos + std::max<size_type>(required_extra_size, m_grow_size));
            m_buffer = &m_container[0];
        }

        assert(m_buffer);
    }
}


bool memstream::ensure(size_type read_size)
{
#ifdef _DEBUG
    assert(m_buffer);

    if (this->read_only())
    {
        assert(m_size <= m_view_size);
        assert(m_rpos <= m_view_size);
    }
    else
    {
        assert(m_size <= m_container.size());
        assert(m_rpos <= m_container.size());
        assert(m_wpos <= m_container.size());
    }
#endif

    return (m_rpos < m_size) && read_size <= (m_size - m_rpos);
}


memstream& memstream::seek_impl(pos_type& cursor, pos_type position)
{
    if (position < 0 || position > m_size)
        CIX_THROW_BADARG("seeking out of boundaries");

    cursor = position;

    return *this;
}


memstream& memstream::seek_impl(pos_type& cursor, off_type offset, seekdir dir)
{
    if (dir == seek_beg)
    {
        if (offset < 0)
            { assert(0); goto __badarg; }
        if (static_cast<pos_type>(offset) > m_size)
            { assert(0); goto __badarg; }

        cursor = static_cast<pos_type>(offset);
    }
    else if (dir == seek_end)
    {
        if (offset > 0)
            { assert(0); goto __badarg; }
        if (static_cast<pos_type>(std::abs(offset)) > m_size)
            { assert(0); goto __badarg; }

        cursor = m_size - static_cast<pos_type>(offset);
    }
    else if (offset == 0)
    {
        assert(dir == seek_cur);
    }
    else if (offset > 0)
    {
        assert(dir == seek_cur);

        if (static_cast<pos_type>(offset) > (m_size - cursor))
            { assert(0); goto __badarg; }

        cursor += static_cast<pos_type>(offset);
    }
    else if (offset < 0)
    {
        assert(dir == seek_cur);

        const auto abs_off = static_cast<pos_type>(std::abs(offset));
        if (abs_off > cursor)
            { assert(0); goto __badarg; }

        cursor -= static_cast<pos_type>(abs_off);
    }

    return *this;

__badarg:
    CIX_THROW_BADARG("offset out of boundaries");
}

}  // namespace cix
