// CIX C++ library
// Copyright (c) Jean-Charles Lefebvre
// SPDX-License-Identifier: MIT

#pragma once

#include "detail/ensure_cix.h"

namespace cix {

class memstream
{
public:
    typedef std::vector<std::uint8_t> container;

    typedef container::value_type value_type;
    typedef container::pointer pointer;
    typedef container::const_pointer const_pointer;
    typedef container::const_reference const_reference;

    typedef std::make_unsigned<container::size_type>::type size_type;
    typedef std::make_unsigned<container::size_type>::type pos_type;
    typedef std::make_signed<size_type>::type off_type;

    static constexpr size_type npos = std::numeric_limits<size_type>::max();

    enum seekdir { seek_beg, seek_end, seek_cur };

protected:
    enum : size_type { default_grow_size = 1024 };

public:
    explicit memstream(size_type grow_size=default_grow_size);
    memstream(const_pointer data, size_type size);

    memstream& open_read(const_pointer data, size_type size);  // enable read-only mode
    bool read_only() const;

    memstream& clear(bool free_memory=false);  // also resets read-ony mode

    bool empty() const;
    size_type size() const;
    const_pointer data() const;

    const_pointer begin() const;
    const_pointer end() const;

    const_reference operator[](pos_type pos) const;

    // current read position
    pos_type tellr() const;

    // current write position
    pos_type tellw() const;

    // seek read cursor
    memstream& seekr(pos_type position);
    memstream& seekr(off_type offset, seekdir dir);

    // seek write cursor
    memstream& seekw(pos_type position);
    memstream& seekw(off_type offset, seekdir dir);

    // generic i/o
    memstream& write(const void* data, size_type size);
    memstream& read(void* dest, size_type size);
    bool peek_cmp(
        const void* expected_data,
        size_type expected_size,
        bool advance_rpos_on_match);

    // single byte i/o
    memstream& write(const std::uint8_t value);
    memstream& read(std::uint8_t& value);

    // integral type write
    template <
        typename T,
        typename std::enable_if_t<
            std::is_integral<T>::value &&
            sizeof(T) >= 2, int> = 0>
    memstream& write(const T value);

    // integral type read
    template <
        typename T,
        typename std::enable_if_t<
            std::is_integral<T>::value &&
            sizeof(T) >= 2, int> = 0>
    memstream& read(T& value);

    // third-party write
    pointer prepare_write(size_type estimated_extra_size);
    memstream& finalize_write(size_type written);

protected:
    void grow(size_type required_extra_size);
    bool ensure(size_type read_size);
    memstream& seek_impl(pos_type& cursor, pos_type position);
    memstream& seek_impl(pos_type& cursor, off_type offset, seekdir dir);

protected:
    pointer m_buffer;
    size_type m_grow_size;
    size_type m_size;
    pos_type m_rpos;
    pos_type m_wpos;

    size_type m_view_size;
    container m_container;
};

}  // namespace cix

#include "memstream.inl.h"
