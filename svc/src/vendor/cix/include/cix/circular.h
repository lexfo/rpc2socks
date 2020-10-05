// CIX C++ library
// Copyright (c) Jean-Charles Lefebvre
// SPDX-License-Identifier: MIT

#pragma once

namespace cix {

namespace detail
{
    template <typename Container, typename = void>
    struct is_growable : std::false_type { };

    template <typename Container>
    struct is_growable<
        Container,
        std::void_t<
            decltype(std::declval<Container>().reserve(std::declval<std::size_t>())),
            decltype(std::declval<Container>().resize(std::declval<std::size_t>())),
            decltype(std::declval<Container>().shrink_to_fit())>>
        : std::true_type { };

    template <typename Container>
    inline constexpr bool is_growable_v =
        typename is_growable<Container>::value;
}


// Implementation of a circular buffer based on either std::array or std::vector
// container typically.
//
// circular::push() methods never fail. They loop once internal container is
// filled and internal write cursor reached the end of the container.
//
// The *pos* argument passed to either method circular::at() or
// circular::operator[] is the virtual index relative to the current internal
// cursor. So that position zero always points to the oldest pushed element and
// position (size() - 1) to the latest pushed element.
//
// circular::set_capacity() method is enabled only for underlying containers
// that support the standard reserve(), resize() and shrink_to_fit() semantics.
// There is a loss of data only if *new_capacity* is smaller than current
// size(). In which case, only the latest pushed elements are kept. Otherwise,
// elements may be relocated physically but internal state remains unchanged.
//
// See also circular_array and circular_vector shorthand types defined below for
// a better understanding of the template arguments.
template <
    typename ElementT,
    std::size_t InitialCapacity,
    typename Container,
    typename std::enable_if_t<
        std::is_class_v<Container> &&
        std::is_same_v<typename Container::value_type, ElementT>, int> = 0>
class circular
{
public:
    typedef Container container_type;

    typedef typename container_type::size_type size_type;

    typedef typename container_type::value_type value_type;
    typedef typename container_type::reference reference;
    typedef typename container_type::const_reference const_reference;
    typedef typename container_type::pointer pointer;
    typedef typename container_type::const_pointer const_pointer;

    static constexpr std::size_t initial_capacity = InitialCapacity;
    static constexpr bool growable = detail::is_growable_v<Container>;


public:
    ~circular() = default;

    circular()
        : m_size{0}
        , m_wpos{0}
    {
        if constexpr (growable)
            this->set_capacity(initial_capacity);
    }


    bool empty() const noexcept
    {
        return !m_size;
    }


    size_type depth() const noexcept
    {
        return m_size;
    }


    size_type capacity() const noexcept
    {
        return m_container.size();
    }


    reference at(size_type pos)
    {
        const auto real_idx = this->translate_pos(pos);
        return m_container[real_idx];
    }

    const_reference at(size_type pos) const
    {
        const auto real_idx = this->translate_pos(pos);
        return m_container[real_idx];
    }


    reference operator[](size_type pos)
    {
        const auto real_idx = this->translate_pos(pos);
        return m_container[real_idx];
    }

    const_reference operator[](size_type pos) const
    {
        const auto real_idx = this->translate_pos(pos);
        return m_container[real_idx];
    }


    reference current()
    {
        const auto real_idx = this->translate_previous(0);
        return m_container[real_idx];
    }

    const_reference current() const
    {
        const auto real_idx = this->translate_previous(0);
        return m_container[real_idx];
    }


    // Get a reference to the element at ``current - depth``.
    // Where *depth* being in ``[0 .. (m_size - 1)]``
    reference previous(size_type depth=1)
    {
        const auto real_idx = this->translate_previous(depth);
        return m_container[real_idx];
    }

    // Get a const reference to the element at ``current - depth``.
    // Where *depth* being in ``[0 .. (m_size - 1)]``
    const_reference previous(size_type depth=1) const
    {
        const auto real_idx = this->translate_previous(depth);
        return m_container[real_idx];
    }


    void clear() noexcept
    {
        m_size = 0;
        m_wpos = 0;
    }


    // Advance current position and return a reference to the then-current
    // element in container so that it can be modified.
    reference push() noexcept
    {
        const auto csize = m_container.size();

        assert(m_size <= csize);
        assert(m_wpos <= m_size);

        if (m_size < csize && m_wpos == m_size)
            ++m_size;

        auto& elem_ref = m_container[m_wpos++];

        if (m_wpos == csize)
        {
            assert(m_size == csize);
            m_wpos = 0;
        }

        return elem_ref;
    }


    // Advance current position and move *new_elem* to the then-current element
    // in container. A reference to it is returned so that it can be modified.
    reference push(value_type&& new_elem) noexcept
    {
        auto& elem_ref = this->push();
        elem_ref = std::move(new_elem);
        return elem_ref;
    }


    // Resize the underlying container to decrease or increase history depth.
    // `set_capacity` is defined only for resizable containers.
    // No data loss unless ``new_capacity < depth()``.
    template <typename Dummy = Container>
    typename std::enable_if_t<detail::is_growable_v<Dummy>, void>
    set_capacity(size_type new_capacity)
    {
        if (!new_capacity)
        {
            assert(0);
            return;
        }

        const auto csize = m_container.size();

        if (new_capacity == csize)
            return;

        if (m_size == 0 || csize == 0)
        {
            m_container.resize(new_capacity);
            m_container.shrink_to_fit();
            m_size = 0;
            m_wpos = 0;
        }
        else
        {
            container_type new_container(new_capacity);
            size_type wpos =
                (m_size <= new_capacity) ? 0 :
                (m_size - new_capacity);

            for (; wpos < m_size; ++wpos)
            {
                const auto src_idx = this->translate_pos(wpos);
                new_container[wpos] = std::move(m_container[src_idx]);
            }

            m_container.swap(new_container);
            m_container.shrink_to_fit();
            m_size = wpos;
            m_wpos = wpos;
        }
    }


    // Give access to the underyling container object. Do not resize it!
    // Use at your own risk.
    container_type& container()
    {
        return m_container;
    }


private:
    // Convert a history depth value into a real element index in the container.
    // *depth* expected to be in [0 .. m_size].
    size_type translate_previous(size_type depth) const
    {
        if (!m_size)
            CIX_THROW_OUTRANGE("empty circular buffer");

        if (depth >= m_size)
        {
            CIX_THROW_OUTRANGE(
                "circular buffer history smaller than expected ({} < {})",
                depth, m_size);
        }

        ++depth;  // m_wpos is always at the offset of the next element

        auto idx = m_wpos;

        do
        {
            if (idx == 0)
            {
                idx = m_size - 1;
                --depth;
            }
            else
            {
                const auto step = std::min(depth, idx);
                idx -= step;
                depth -= step;
            }
        }
        while (depth > 0);

        return idx;
    }

    // Convert circular position index to the actual container index
    size_type translate_pos(size_type pos) const
    {
        const auto csize = m_container.size();

        assert(m_size <= csize);
        assert(m_wpos <= m_size);

        if (pos >= 0 && pos < m_size)
        {
            if (m_size < csize)
                return pos;
            else
                return (m_wpos + pos) % m_size;
        }

        CIX_THROW_OUTRANGE("circular::offset");
    }


private:
    container_type m_container;
    size_type m_size;
    size_type m_wpos;
};


template <typename T, std::size_t N>
using circular_array = typename circular<T, N, std::array<T, N>>;

template <
    typename T,
    std::size_t InitialCapacity,
    typename Allocator = std::allocator<T>>
using circular_vector =
    typename circular<T, InitialCapacity, std::vector<T, Allocator>>;


}  // namespace cix
