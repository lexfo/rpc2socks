// CIX C++ library
// Copyright (c) Jean-Charles Lefebvre
// SPDX-License-Identifier: MIT

#pragma once

#include "detail/ensure_cix.h"

namespace cix {

// A memory stream buffer class compliant with `std::basic_streambuf`.
//
// Usage example::
//
//   std::vector<char> data;
//   // ... fill-in *data* here ...
//   cix::basic_memstreambuf<data.value_type> sbuf(&data[0], data.size());
//   std::istream in(&sbuf);
//   // ... read data from *in* ...
//
// Useful references:
// * Deriving from std::streambuf
//   https://artofcode.wordpress.com/2010/12/12/deriving-from-stdstreambuf/
// * A beginner's guide to writing a custom stream buffer (std::streambuf)
//   http://www.voidcn.com/article/p-vjnlygmc-gy.html
// * membuf.cpp
//   https://gist.github.com/mlfarrell/28ea0e7b10756042956b579781ac0dd8
// * Memory streambuf and stream
//   https://codereview.stackexchange.com/questions/138479/memory-streambuf-and-stream
template <
    class CharT,
    class Traits = std::char_traits<CharT>>
class basic_memstreambuf :
    public std::basic_streambuf<CharT, Traits>
{
public:
    typedef std::basic_streambuf<CharT, Traits> BaseT;

    // TODO: check this
    using BaseT::int_type;
    using BaseT::traits_type;


public:
    basic_memstreambuf()
        : BaseT()
        { }

    explicit basic_memstreambuf(const basic_memstreambuf& rhs)
        : BaseT(static_cast<BaseT&>(rhs))
        { }

    // non-standard
    explicit basic_memstreambuf(const std::basic_string<char_type>& s)
        : BaseT()
    {
        assert(!s.empty());
        setg(
            const_cast<char_type*>(s.begin()),
            const_cast<char_type*>(s.begin()),
            const_cast<char_type*>(s.end()));
    }

    // non-standard
    basic_memstreambuf(const char_type* s, std::streamsize n)
        : BaseT()
    {
        assert(s);
        assert(n > 0);
        setg(
            const_cast<char_type*>(s),
            const_cast<char_type*>(s),
            const_cast<char_type*>(s + n));
    }

    // non-standard
    basic_memstreambuf(const char_type* begin, const char_type* end)
        : BaseT()
    {
        assert(begin);
        assert(end);
        assert(begin < end);

        // check size
        const std::uintmax_t count = end - begin;
        if (count > static_cast<decltype(count)>(
            std::numeric_limits<std::streamsize>::max()))
        {
            throw std::invalid_argument("basic_memstreambuf too big");
        }

        setg(
            const_cast<char_type*>(begin),
            const_cast<char_type*>(begin),
            const_cast<char_type*>(end));
    }

    basic_memstreambuf& operator=(const basic_memstreambuf&) = delete;


protected:
    virtual std::streamsize showmanyc() override
    {
        const auto* ptr = gptr();
        const auto* end = egptr();

        assert(ptr <= end);

        return (ptr <= end) ? (end - ptr) : 0;
    }

    virtual int_type underflow() override
    {
        const auto* ptr = gptr();

        if (ptr >= egptr())
            return traits_type::eof();

        return traits_type::to_int_type(*ptr);
    }

    virtual std::streamsize xsgetn(char_type* s, std::streamsize count) override
    {
        if (count == 0)
            return 0;

        const auto* ptr = gptr();
        const auto to_read = std::min(
            count,
            static_cast<std::streamsize>(egptr() - ptr));

        if (to_read == 0)
        {
            return traits_type::eof();
        }
        else
        {
            std::memcpy(s, ptr, static_cast<int>(to_read));
            gbump(static_cast<int>(to_read));
            return to_read;
        }
    }

    virtual pos_type seekoff(
        off_type off,
        std::ios_base::seekdir dir,
        std::ios_base::openmode which = std::ios_base::in) override
    {
        if (which != std::ios_base::in)
        {
            assert(0);
            throw std::invalid_argument("basic_memstreambuf::seekoff[which]");
        }

        if (dir == std::ios_base::beg)
        {
            if (off >= 0 && off < egptr() - eback())
            {
                setg(eback(), eback() + off, egptr());
            }
            else
            {
                assert(0);
                throw std::out_of_range("basic_memstreambuf::seekoff[beg]");
            }
        }
        else if (dir == std::ios_base::cur)
        {
            if ((off >= 0 && off < egptr() - gptr()) ||
                (off < 0 && std::abs(off) < gptr() - eback()))
            {
                gbump(static_cast<int>(off));
            }
            else
            {
                assert(0);
                throw std::out_of_range("basic_memstreambuf::seekoff[cur]");
            }
        }
        else if (dir == std::ios_base::end)
        {
            if (off <= 0 && std::abs(off) < egptr() - eback())
            {
                setg(eback(), egptr() + static_cast<int>(off), egptr());
            }
            else
            {
                assert(0);
                throw std::out_of_range("basic_memstreambuf::seekoff[end]");
            }
        }
        else
        {
            assert(0);
            throw std::invalid_argument("basic_memstreambuf::seekoff[dir]");
        }

        return gptr() - eback();
    }

    virtual pos_type seekpos(
        pos_type pos,
        std::ios_base::openmode which = std::ios_base::in) override
    {
        if (which != std::ios_base::in)
        {
            assert(0);
            throw std::invalid_argument("basic_memstreambuf::seekpos[which]");
        }

        if (pos < egptr() - eback())
        {
            setg(eback(), eback() + static_cast<int>(pos), egptr());
        }
        else
        {
            assert(0);
            throw std::out_of_range("memstreambuf::seekpos");
        }

        return pos;
    }

#if 0
    virtual int_type pbackfail(int_type c = traits_type::eof()) override
    {
        const auto* begin = eback();
        const auto* ptr = gptr();
        const auto gc = *(ptr - 1);

        if (ptr == begin || (c != traits_type::eof() && c != gc))
            return traits_type::eof();

        gbump(-1);

        return traits_type::to_int_type(gc);
    }
#endif
};


typedef basic_memstreambuf<char> memstreambuf;
typedef basic_memstreambuf<wchar_t> wmemstreambuf;

}  // namespace cix
