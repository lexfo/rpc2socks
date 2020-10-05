// CIX C++ library
// Copyright (c) Jean-Charles Lefebvre
// SPDX-License-Identifier: MIT

#include <cix/cix>
#include <cix/detail/intro.h>

#ifdef CIX_ENABLE_WSTR

namespace cix {

//------------------------------------------------------------------------------
static _locale_t _wstr_numlocale = nullptr;


//------------------------------------------------------------------------------
void wstr::static_init()
{
    assert(!_wstr_numlocale);
    if (!_wstr_numlocale)
        _wstr_numlocale = _create_locale(LC_NUMERIC, "C");
}

bool wstr::static_isinit()
{
    return _wstr_numlocale != nullptr;
}

void wstr::static_uninit()
{
    assert(_wstr_numlocale);
    if (_wstr_numlocale)
    {
        _free_locale(_wstr_numlocale);
        _wstr_numlocale = nullptr;
    }
}




//------------------------------------------------------------------------------
wstr::pointer wstr::acquire_buffer(size_type len)
{
    this->grow(len);
    this->assign(len, value_type('\0'));
    return &(*this)[0];
}

wstr& wstr::release_buffer(size_type len)
{
    assert(len == npos || len < this->size());
    if (len == npos)
    {
        for (len = 0; len < this->size() && (*this)[len]; ++len) { ; }
        assert((*this)[len] == value_type('\0'));
    }

    if (len < this->size())
        this->erase(len);
    assert(this->length() == wcsnlen_s(this->c_str(), this->length() + 1));

    return *this;
}




//------------------------------------------------------------------------------
wstr::size_type wstr::findi(const wstr& s, size_type pos) const
{
    // FIXME: this is the lazy version, optimize it!
    wstr a(this->copy_lower());
    wstr b(s.copy_lower());
    return a.find(b, pos);
}

wstr::size_type wstr::rfindi(const wstr& s, size_type pos) const
{
    // FIXME: this is the lazy version, optimize it!
    wstr a(this->copy_lower());
    wstr b(s.copy_lower());
    return a.rfind(b, pos);
}

bool wstr::begins_with(const wstr& s, bool case_sensitive) const
{
    return
        this->length() >= s.length() &&
        0 == wstr::cmp(this->c_str(), s.c_str(), case_sensitive, s.length());
}

bool wstr::ends_with(const wstr& s, bool case_sensitive) const
{
    return
        this->length() >= s.length() &&
        0 == wstr::cmp(
            this->c_str() + this->length() - s.length(),
            s.c_str(), case_sensitive, s.length());
}

wstr::size_type wstr::begins_with_any_of(const std::vector<wstr>& vec, bool case_sensitive) const
{
    size_type idx = 0;
    for (const auto& s : vec)
    {
        if (this->begins_with(s, case_sensitive))
            return idx;
        ++idx;
    }
    return npos;
}

wstr::size_type wstr::ends_with_any_of(const std::vector<wstr>& vec, bool case_sensitive) const
{
    size_type idx = 0;
    for (const auto& s : vec)
    {
        if (this->ends_with(s, case_sensitive))
            return idx;
        ++idx;
    }
    return npos;
}

wstr::size_type wstr::contains_any_of(const std::vector<wstr>& vec, bool case_sensitive, size_type pos) const
{
    size_type idx = 0;
    for (const auto& s : vec)
    {
        if (this->contains(s, case_sensitive, pos))
            return idx;
        ++idx;
    }
    return npos;
}




//------------------------------------------------------------------------------
wstr::size_type wstr::equals_any_of(const std::vector<wstr>& vec, bool case_sensitive) const
{
    size_type idx = 0;
    for (const auto& s : vec)
    {
        if (this->equals(s, case_sensitive))
            return idx;
        ++idx;
    }
    return npos;
}

wstr::size_type wstr::matches_any_of(const std::vector<wstr>& vec, bool case_sensitive) const
{
    size_type idx = 0;
    for (const auto& s : vec)
    {
        if (this->matches(s, case_sensitive))
            return idx;
        ++idx;
    }
    return npos;
}




//------------------------------------------------------------------------------
wstr& wstr::erase_tail(size_type tail_len)
{
    if (tail_len < this->length())
        container::erase(this->length() - tail_len);
    else
        container::clear();

    return *this;
}

wstr& wstr::replace_all(const wstr& from, const wstr& to, size_type pos)
{
    while ((pos = this->find(from, pos)) != npos)
    {
        this->replace(pos, from.length(), to);
        pos += to.length();  // in case 'to' contains 'from'
    }
    return *this;
}

wstr& wstr::ltrim()
{
    if (!this->empty())
    {
        this->erase(this->begin(),
            std::find_if(
                this->begin(), this->end(),
                std::not_fn(std::iswspace)));
    }

    return *this;
}

wstr& wstr::ltrim(value_type c)
{
    if (!this->empty())
    {
        const auto pos = this->find_first_not_of(c);
        if (pos == npos)
            container::clear();
        else if (pos != 0)
            this->erase(0, pos);
    }

    return *this;
}

wstr& wstr::rtrim()
{
    if (!this->empty())
    {
        this->erase(std::find_if(
            this->rbegin(), this->rend(),
                std::not_fn(std::iswspace)).base(),
            this->end());
    }

    return *this;
}

wstr& wstr::rtrim(value_type c)
{
    if (!this->empty())
    {
        const auto pos = this->find_last_not_of(c);
        if (pos == npos)
            container::clear();
        else
            this->erase(pos + 1, npos);
    }

    return *this;
}

std::vector<wstr> wstr::split(const wstr& separator, bool case_sensitive, bool keep_empty_fields, size_type max_fields) const
{
    assert(!separator.empty());
    if (separator.empty() || this->empty())
        return std::vector<wstr>();

    std::vector<wstr> dest;
    size_type begin = 0;
    size_type pos;

    do
    {
        if (case_sensitive)
            pos = this->find(separator, begin);
        else
            pos = this->findi(separator, begin);

        if (pos == npos || (max_fields > 0 && dest.size() == (max_fields - 1)))
        {
            // copy the remaining part of the string
            dest.push_back(this->substr(begin));
            return dest;
        }
        else if (pos == begin)
        {
            if (keep_empty_fields)
                dest.push_back(wstr());

            // go ahead and skip separator
            begin = pos + separator.length();
        }
        else
        {
            // copy up to delimiter
            dest.push_back(this->substr(begin, pos - begin));
            begin = pos + separator.length();
        }
    }
    while (begin < this->length());

    return dest;
}

std::vector<wstr> wstr::split_one_of(const wstr& separators, bool keep_empty_fields, size_type max_fields) const
{
    assert(!separators.empty());
    if (separators.empty() || this->empty())
        return std::vector<wstr>();

    std::vector<wstr> dest;
    size_type begin = 0;
    size_type pos;

    do
    {
        pos = this->find_first_of(separators, begin);
        if (pos == npos || (max_fields > 0 && dest.size() == (max_fields - 1)))
        {
            // copy the remaining part of the string
            dest.push_back(this->substr(begin));
            return dest;
        }
        else if (pos == begin)
        {
            if (keep_empty_fields)
                dest.push_back(wstr());

            // go ahead and skip separator
            begin = pos + 1;
        }
        else
        {
            // copy up to delimiter
            dest.push_back(this->substr(begin, pos - begin));
            begin = pos + 1;
        }

        // seek to the next wanted part of the haystack
        if (!keep_empty_fields)
        {
            begin = this->find_first_not_of(separators, begin);
            if (begin == npos)
                return dest;
        }
    }
    while (begin < this->length());

    return dest;
}

wstr& wstr::join(const std::vector<wstr>& vec, const wstr& glue, bool keep_empty_fields, size_type start_index, size_type max_fields)
{
    this->clear();
    if (start_index < 0 || start_index >= vec.size())
        return *this;

    for (size_type i = start_index, count = 0;
        i < vec.size() && (!max_fields || count < max_fields);
        ++i)
    {
        if (i > start_index)
            this->append(glue);
        if (!vec[i].empty() || keep_empty_fields)
        {
            this->append(vec[i]);
            ++count;
        }
    }

    return *this;
}




//------------------------------------------------------------------------------
std::int64_t wstr::to_int(size_type pos, int from_base) const
{
    assert(wstr::static_isinit());
    assert(pos < this->length());
    assert(from_base == 0 || (from_base >= 2 && from_base <= 36));
    _set_errno(0);
    return _wcstoi64_l(
        &(this->c_str()[pos]), nullptr, from_base, _wstr_numlocale);
}

std::uint64_t wstr::to_uint(size_type pos, int from_base) const
{
    assert(wstr::static_isinit());
    assert(pos < this->length());
    assert(from_base == 0 || (from_base >= 2 && from_base <= 36));
    _set_errno(0);
    return _wcstoui64_l(
        &(this->c_str()[pos]), nullptr, from_base, _wstr_numlocale);
}

double wstr::to_float(size_type pos) const
{
    assert(wstr::static_isinit());
    assert(pos < this->length());
    _set_errno(0);
    const double res = _wcstod_l(&(this->c_str()[pos]), nullptr, _wstr_numlocale);
    assert(res != -HUGE_VAL && res != +HUGE_VAL);
    return res;
}

wstr wstr::to_normalized() const
{
    const int maxIterations = 10;
    const NORM_FORM normForm = NormalizationKD;

    if (this->empty())
        return wstr();

    int estimatedLen = NormalizeString(
        normForm, this->c_str(), static_cast<int>(this->length() + 1),
        NULL, 0);
    DWORD error = GetLastError();
    if (estimatedLen > 0)
    {
        wstr dest;
        for (int i = 0; i < maxIterations; ++i)
        {
            estimatedLen = NormalizeString(
                normForm, this->c_str(), static_cast<int>(this->length() + 1),
                dest.acquire_buffer(static_cast<size_type>(estimatedLen)),
                estimatedLen);
            error = GetLastError();
            dest.release_buffer();

            if (estimatedLen > 0)
                return dest;
            if (error != ERROR_INSUFFICIENT_BUFFER)
            {
                assert(0);
                return wstr(*this);
            }
            estimatedLen = -estimatedLen;
        }
    }

    assert(0);
    return wstr(*this);
}




//------------------------------------------------------------------------------
bool wstr::is_int(bool accept_blank) const
{
    // format of an integer string:
    //   [whitespaces] [sign] [digits] [whitespaces|nl|\0]

    if (this->empty())
        return false;

    const value_type* p = &(*this)[0];

    // skip whitespace first
    if (accept_blank)
        while (wstr::is_blank(*p)) ++p;

    // handle first character which is a special case
    // because of the eventual +/- sign
    if (*p == value_type('-') || *p == value_type('+'))
        ++p;

    // at least one or several digits
    if (!wstr::is_digit(*p)) return false;
    while (wstr::is_digit(*p)) ++p;
    if (!*p) return true;

    // [whitespace|\0]
    if (accept_blank)
    {
        while (wstr::is_blank(*p)) ++p;
        if (!*p) return true;
    }

    return false;
}

bool wstr::is_digit(bool accept_blank) const
{
    if (this->empty())
        return false;

    const value_type* p = &(*this)[0];

    // skip whitespace first
    if (accept_blank)
        while (wstr::is_blank(*p)) ++p;

    // at least one or several digits
    if (!wstr::is_digit(*p)) return false;
    while (wstr::is_digit(*p)) ++p;
    if (!*p) return true;

    // [whitespace|\0]
    if (accept_blank)
    {
        while (wstr::is_blank(*p)) ++p;
        if (!*p) return true;
    }

    return false;
}

bool wstr::is_hex(bool accept_blank) const
{
    if (this->empty())
        return false;

    const value_type* p = &(*this)[0];

    // skip whitespace first
    if (accept_blank)
        while (wstr::is_blank(*p)) ++p;

    // '0x' prefix?
    if (*p == value_type('0') && (*++p == value_type('x') || *p == value_type('X')))
        ++p;

    // at least one or several hex characters
    if (!wstr::is_xdigit(*p)) return false;
    while (wstr::is_xdigit(*p)) ++p;
    if (!*p) return true;

    // [whitespace|\0]
    if (accept_blank)
    {
        while (wstr::is_blank(*p)) ++p;
        if (!*p) return true;
    }

    return false;
}

bool wstr::is_float(bool accept_blank) const
{
    // format of a floating point string:
    //   [whitespaces] [sign] [digits] [.digits] [ {d | D | e | E }[sign]digits] {whitespaces|nl|\0}
    //   "if no digits appear before the decimal point, at least one must appear
    //   after the decimal point."

    if (this->empty())
        return false;

    const value_type* p = &(*this)[0];
    bool mantissa_needed = false;

    // skip whitespace first
    if (accept_blank)
        while (wstr::is_blank(*p)) ++p;

    // [sign]
    if (*p == value_type('-') || *p == value_type('+'))
        ++p;

    // [digits]
    if (*p == value_type('.'))
    {
        ++p;
        mantissa_needed = true;
        goto __mantissa;
    }
    if (!wstr::is_digit(*p)) return false;
    while (wstr::is_digit(*p)) ++p;
    if (!*p) return true;
    if (accept_blank && wstr::is_blank(*p))
    {
        ++p;
        goto __trailing_whitespaces;
    }

    // [.digits] - mantissa (required only if there was no main part)
    __mantissa:
    if (mantissa_needed)
    {
        if (!wstr::is_digit(*p))
            return false;
        ++p;
        while (wstr::is_digit(*p)) ++p;
        if (!*p) return true;
        if (accept_blank && wstr::is_blank(*p))
        {
            ++p;
            goto __trailing_whitespaces;
        }
    }
    else if (*p == value_type('.'))
    {
        ++p;
        // digits are not required after a decimal point here
        while (wstr::is_digit(*p)) ++p;
        if (!*p) return true;
        if (accept_blank && wstr::is_blank(*p))
        {
            ++p;
            goto __trailing_whitespaces;
        }
    }

    // [exponent]
    if (*p != value_type('d') && *p != value_type('D') &&
        *p != value_type('e') && *p != value_type('E'))
    {
        return false;
    }
    ++p;
    if (*p == value_type('-') || *p == value_type('+'))
        ++p;
    if (!wstr::is_digit(*p))
        return false;
    ++p;
    while (wstr::is_digit(*p))
        ++p;

    // [whitespace|\0]
    if (!*p)
        return true;
    if (accept_blank && wstr::is_blank(*p))
    {
        ++p;
        goto __trailing_whitespaces;
    }

    return false;

__trailing_whitespaces:
    while (wstr::is_blank(*p)) ++p;
    return !*p ? true : false;
}




//------------------------------------------------------------------------------
namespace detail
{
    // An fmt::arg_formatter that:
    // * supports wstr
    // * forcefully casts signed integers to unsigned when the 'x' or 'X' type
    //   specifier is used.
    template <typename Char>
    class fmt_custom_arg_formatter :
        public fmt::arg_formatter<fmt::buffer_range<Char>>
    {
    private:
        using base = fmt::arg_formatter<fmt::buffer_range<Char>>;
        using iterator = typename base::iterator;
        using context_type = fmt::basic_format_context<iterator, Char>;
        using format_specs = typename base::format_specs;

    public:
        explicit fmt_custom_arg_formatter(
                context_type& ctx,
                fmt::basic_format_parse_context<Char>* parse_ctx=nullptr,
                format_specs* spec=nullptr)
            : base(ctx, parse_ctx, spec)
            { }

        using base::operator();

        inline iterator operator()(std::int8_t value) { return this->_handle_signed_int(value); }
        inline iterator operator()(std::int16_t value) { return this->_handle_signed_int(value); }
        inline iterator operator()(std::int32_t value) { return this->_handle_signed_int(value); }
        inline iterator operator()(std::int64_t value) { return this->_handle_signed_int(value); }

    private:
        template <typename T>
        iterator _handle_signed_int(T value)
        {
            if (this->specs() && value < T(0))
            {
                const auto type_lc = this->specs()->type | 0x20;  // lower case
                if (type_lc == 'x')  // || type_lc == 'b')
                {
                    typedef std::make_unsigned<T>::type unsigned_type;
                    return base::operator()<unsigned_type>(value);
                }
            }

            return base::operator()(value);
        }
    };
}

wstr& wstr::vfmt(
    const fmt::basic_string_view<value_type>& format,
    fmt::basic_format_args<fmt::buffer_context<value_type>> args)
{
    this->clear();
    return this->vfmt_append(format, args);
}

wstr& wstr::vfmt_append(
    const fmt::basic_string_view<value_type>& format,
    fmt::basic_format_args<fmt::buffer_context<value_type>> args)
{
    // TODO / FIXME: append directly to self, instead of using a buffer

    assert(this->size() == wcsnlen_s(this->c_str(), this->size() + 1));

    fmt::basic_memory_buffer<value_type> buffer;

    fmt::vformat_to<detail::fmt_custom_arg_formatter<value_type>>(
        buffer, format, args);

    if (buffer.size() > 0)
    {
        // safety check
        auto size = buffer.size();
        const auto clen = wcsnlen_s(buffer.data(), size + 1);
        if (clen != size)
        {
            // (clen < size) already happened while printing float values with
            // older versions of fmtlib
            assert(0);
            size = std::min(size, clen);
        }

        this->append(buffer.begin(), std::next(buffer.begin(), size));
    }

    return *this;
}

wstr& wstr::vfmt_prepend(
    const fmt::basic_string_view<value_type>& format,
    fmt::basic_format_args<fmt::buffer_context<value_type>> args)
{
    assert(this->size() == wcsnlen_s(this->c_str(), this->size() + 1));

    fmt::basic_memory_buffer<value_type> buffer;

    fmt::vformat_to<detail::fmt_custom_arg_formatter<value_type>>(
        buffer, format, args);

    this->insert(0, buffer.data(), buffer.size());

    return *this;
}




//------------------------------------------------------------------------------
bool wstr::path_is_absolute() const
{
    const size_type len = this->length();
    if (len > 0)
    {
        if (wstr::is_pathsep((*this)[0]))
            return true;

        if (len >= 2 &&
            (*this)[1] == value_type(':') &&
            wstr::is_asciiletter((*this)[0]))
        {
            return true;
        }
    }
    return false;
}

wstr::value_type wstr::path_drive_letter() const
{
    // note: "c:file.txt" seems to be a valid form
    // ref:
    //   https://msdn.microsoft.com/en-us/library/aa365247.aspx
    //   https://en.wikipedia.org/wiki/Path_(computing)

    // C: form
    if (this->length() >= 2 &&
        (*this)[1] == value_type(':') &&
        wstr::is_asciiletter((*this)[0]))
    {
        return (*this)[0] & ~0x20;  // force upper case
    }
    // \\?\C: or \\.\C: form
    else if (this->length() >= 6 &&
        wstr::is_pathsep((*this)[0]) &&
        wstr::is_pathsep((*this)[1]) &&
        ((*this)[2] == value_type('?') || (*this)[2] == value_type('.')) &&
        wstr::is_pathsep((*this)[3]) &&
        wstr::is_asciiletter((*this)[4]) &&
        (*this)[5] == value_type(':'))
    {
        return (*this)[4] & ~0x20;  // force upper case
    }

    return 0;
}

wstr wstr::path_root(bool normalize, size_type* out_tail_pos) const
{
    // supported forms:
    //   [drive_letter]:
    //   [drive_letter]:\
    //   \ (relative to current working directory root)
    //   \\[server]\[sharename]\
    //   \\?\[drive_spec]:\
    //   \\.\[physical_device]\
    //   \\?\[server]\[sharename]\
    //   \\?\UNC\[server]\[sharename]\
    //
    // ref:
    //   https://msdn.microsoft.com/en-us/library/windows/desktop/aa365247(v=vs.85).aspx
    //     <- especially the "Namespaces" section
    //   https://msdn.microsoft.com/en-us/library/aa365247.aspx
    //   https://en.wikipedia.org/wiki/Path_(computing)

    if (out_tail_pos)
        *out_tail_pos = npos;

    if (this->length() >= 2 &&
        (*this)[1] == value_type(':') &&
        wstr::is_asciiletter((*this)[0]))
    {
        const size_type tail_pos =
            this->find_first_not_of(wstr::path_separators(), 2);

        if (out_tail_pos)
            *out_tail_pos = tail_pos;

        // C: or C:\ form
        // note: in this case we always return a C:\ form
        if (normalize)
        {
            wstr dest;
            dest.push_back((*this)[0] & ~0x20); // force upper case
            dest.push_back(value_type(':'));
            dest.push_back(wstr::native_path_separator());
            return dest;
        }
        else if (this->length() >= 3 && wstr::is_pathsep((*this)[2]))
        {
            return wstr(*this, 0, tail_pos);  // preserve input form
        }
        else
        {
            return wstr(*this, 0, 2);
        }
    }
    else if (this->length() >= 1 && wstr::is_pathsep((*this)[0]))
    {
        if (this->length() == 1 || !wstr::is_pathsep((*this)[1]))
        {
            // "/" or "/..." form
            if (out_tail_pos)
                *out_tail_pos = (this->length() == 1) ? npos : 1;
            return
                normalize ? wstr(wstr::native_path_separator()) :
                wstr((*this)[0]);
        }

        size_type pos = this->find_first_not_of(wstr::path_separators(), 2);
        if (pos == npos)
        {
            // same as above but separator is duplicated
            // e.g. "/////"
            return
                normalize ?
                wstr(wstr::native_path_separator()) :
                wstr(*this);  // preserve input form
        }

        wstr norm_dest(wstr::native_path_separator());
        size_type old_pos = pos;
        const bool is_dot_unc = (*this)[pos] == value_type('.');
        const bool is_questionmark_unc = (*this)[pos] == value_type('?');
        const bool is_prefixed_unc = is_dot_unc || is_questionmark_unc;
        bool is_literal_unc = false;

        // forms \\.\..., \\?\... or \\?\UNC\...
        if (is_prefixed_unc)
        {
            // invalid form
            if ((pos + 1) < this->length() && !wstr::is_pathsep((*this)[pos+1]))
                return wstr();

            if (normalize)
            {
                norm_dest.append(wstr::native_path_separator());
                norm_dest.append((*this)[old_pos]);  // '?' or '.'
                norm_dest.append(wstr::native_path_separator());
            }

            // skip separator(s) if any
            pos = this->find_first_not_of(wstr::path_separators(), pos + 1);

            // incomplete root
            if (pos == npos)
                return normalize ? norm_dest : wstr(*this);

            // skip the UNC\ part
            if (this->length() >= (pos + 3) &&
                ((*this)[pos + 0] == value_type('U') ||
                    (*this)[pos + 0] == value_type('u')) &&
                ((*this)[pos + 1] == value_type('N') ||
                    (*this)[pos + 1] == value_type('n')) &&
                ((*this)[pos + 2] == value_type('C') ||
                    (*this)[pos + 2] == value_type('c')) &&
                (this->length() == (pos + 3) ||
                    wstr::is_pathsep((*this)[pos + 3])))
            {
                is_literal_unc = true;

                if (normalize)
                {
                    norm_dest.append(WSTR("UNC"));
                    norm_dest.append(wstr::native_path_separator());
                }

                pos = this->find_first_not_of(wstr::path_separators(), pos + 3);
                if (pos == npos)
                {
                    // Windows' PathSkipRoot() supports this form: \\?\UNC
                    return normalize ? norm_dest : wstr(*this);
                }
            }
        }

        // incomplete root, [physical_device] or [server] missing
        if (pos == npos)
            return normalize ? norm_dest : wstr(*this);

        // form: \\?\[drive_spec]:
        if (this->length() >= pos + 2 &&
            wstr::upper((*this)[pos + 1]) == value_type(':') &&
            wstr::is_asciiletter((*this)[pos]))
        {
            const size_type tail_pos =
                this->find_first_not_of(wstr::path_separators(), pos + 2);

            if (out_tail_pos)
                *out_tail_pos = tail_pos;

            if (normalize)
            {
                norm_dest.clear();
                norm_dest.push_back((*this)[pos] & ~0x20);  // force upper case
                norm_dest.push_back(value_type(':'));
                norm_dest.push_back(wstr::native_path_separator());
                return norm_dest;
            }
            else if (this->length() >= pos + 3 &&
                wstr::is_pathsep((*this)[pos + 2]))
            {
                return wstr(*this, 0, tail_pos);  // preserve input form
            }
            else
            {
                return wstr(*this, 0, pos + 2);
            }
        }

        // [physical_device] or [server]
        const size_type server_pos = pos;
        pos = this->find_first_of(wstr::path_separators(), pos + 1);
        old_pos = pos;  // end of [server]

        // skip separator(s)
        if (pos != npos)
            pos = this->find_first_not_of(wstr::path_separators(), pos + 1);

        // here, we want both [server] and [sharename] unless is_dot_unc is
        // true, in which case we only want the [physical_device]
        // note: (pos != npos) means [sharename] is present
        if (is_dot_unc || pos != npos)
        {
            if (normalize)
            {
                if (!is_dot_unc)
                    norm_dest.assign(2, wstr::native_path_separator());
                else if (norm_dest.length() == 1)
                    norm_dest.append(wstr::native_path_separator());

                norm_dest.append(
                    *this, server_pos,
                    (old_pos == npos) ? npos : (old_pos - server_pos));
                norm_dest.append(wstr::native_path_separator());
            }

            if (is_dot_unc)
            {
                if (out_tail_pos)
                    *out_tail_pos = pos;
                return normalize ? norm_dest : wstr(*this, 0, pos);
            }

            // find the end of [sharename]
            old_pos = pos;  // begin of [sharename]
            pos = this->find_first_of(wstr::path_separators(), pos + 1);
            if (normalize)
            {
                norm_dest.append(
                    *this, old_pos,
                    (pos == npos) ? npos : (pos - old_pos));
                norm_dest.append(wstr::native_path_separator());
            }

            // include separator(s)
            if (pos != npos)
                pos = this->find_first_not_of(wstr::path_separators(), pos + 1);
            if (out_tail_pos)
                *out_tail_pos = pos;

            return normalize ? norm_dest : wstr(*this, 0, pos);
        }
        else
        {
            // incomplete root, [sharename] is missing we only want the part
            // before [server]
            if (out_tail_pos)
                *out_tail_pos = is_prefixed_unc ? npos : server_pos;
            return normalize ? norm_dest : wstr(*this, 0, server_pos);
        }
    }

    // no known root form, path is assumed to be relative
    if (out_tail_pos && !this->empty())
        *out_tail_pos = 0;
    return wstr();
}

wstr wstr::path_dir() const
{
    size_type last;

    last = this->length();
    if (last == 0)
    {
        return wstr(WSTR("."));  // dirname() compliant
    }
    else if (last >= 2 &&
        (*this)[1] == value_type(':') && wstr::is_asciiletter((*this)[0]))
    {
        if (last == 2)
        {
            return wstr(*this);
        }
        else if (wstr::is_pathsep((*this)[2]) &&
            this->find_first_not_of(wstr::path_separators(), 2) == npos)
        {
            return wstr(this->c_str(), 3);
        }
    }
    else if (last >= 6 && // validates \\?\C: or \\.\C:
        wstr::is_pathsep((*this)[0]) &&
        wstr::is_pathsep((*this)[1]) &&
        ((*this)[2] == value_type('?') || (*this)[2] == value_type('.')) &&
        wstr::is_pathsep((*this)[3]) &&
        wstr::is_asciiletter((*this)[4]) &&
        (*this)[5] == value_type(':'))
    {
        if (last == 6)
        {
            return wstr(*this);
        }
        else if (wstr::is_pathsep((*this)[6]) &&
            this->find_first_not_of(wstr::path_separators(), 6) == npos)
        {
            return wstr(this->c_str(), 6);
        }
    }

    last = this->find_last_of(container(wstr::path_separators()));
    if (last == npos)
    {
        // no separators
        return wstr(WSTR("."));
    }
    else if (last + 1 == this->length())
    {
        // has trailing separator(s)
        last = this->find_last_not_of(wstr::path_separators(), last);
        if (last == npos)  // has only separators in the whole string
            return wstr(WSTR("/"));

        last = this->find_last_of(wstr::path_separators(), last);
        if (last == npos)  // has only a name and trailing separator(s)
            return wstr(WSTR("."));
        else if (last == 0)
            ++last;

        return this->substr(0, last);
    }
    else
    {
        // has separators but no trailing separator(s)
        if (last == 0)
            ++last;
        else
            last = this->find_last_not_of(wstr::path_separators(), last) + 1;
        return this->substr(0, last);
    }
}

wstr wstr::path_name() const
{
    size_type first;
    size_type last;

    if (this->empty())
        return wstr();

    last = this->find_last_of(wstr::path_separators());
    if (last == npos)
    {
        // no separators
        return *this;
    }
    else if (last + 1 == this->length())
    {
        // has trailing separator(s)
        last = this->find_last_not_of(wstr::path_separators(), last);
        if (last == npos)  // has only separators
            return *this;

        first = this->find_last_of(wstr::path_separators(), last);
        if (first == npos)
            first = 0;
        else
            ++first;

        return this->substr(first, last - first + 1);
    }
    else
    {
        // has separators but no trailing separator(s)
        return this->substr(last + 1);
    }
}

wstr wstr::path_title(bool remove_all_extensions) const
{
    wstr name(this->path_name());
    size_type pos =
        name[0] == value_type('.') ?
        name.find_first_not_of(value_type('.')) :
        0;

    if (pos == npos)
        return name;  // only dots

    if (remove_all_extensions)
    {
        pos = name.find_first_of(value_type('.'), pos);
        if (pos == npos)
            return name;  // no more dots after the beginning
    }
    else
    {
        size_type last = name.find_last_of(value_type('.'));
        if (last <= pos || last == npos)
            return name;  // no dot
        pos = last;
    }

    return name.substr(0, pos);
}

wstr wstr::path_ext(bool all) const
{
    size_type start = 0;

    if (this->empty() || wstr::is_pathsep(*this->rbegin()))
        return wstr();

    wstr name(this->path_name());

    if (name.length() >= 1 && name[0] == value_type('.'))
    {
        if (name.length() == 1 || (name.length() == 2 && name[1] == value_type('.')))
            return wstr();
        else
            start = name.find_first_not_of(value_type('.'));
    }

    size_type pos = all ?
        name.find_first_of(value_type('.'), start) :
        name.find_last_of(value_type('.'));
    if (pos == npos || pos <= start || pos == name.length() - 1)
        return wstr();

    return name.substr(pos + 1);
}

wstr& wstr::path_replace_ext(bool all, const wstr& new_extension)
{
    if (this->empty() || wstr::is_pathsep(*this->rbegin()) ||
        *this->rbegin() == value_type(':'))
    {
        return *this;
    }
    else if (this->length() >= 1 && (*this)[0] == value_type('.'))
    {
        if (this->length() == 1 || (this->length() == 2 && (*this)[1] == value_type('.')))
            return *this;
    }

    // find the beginning of the name
    size_type start = this->find_last_of(wstr::path_separators());
    if (start == npos)
        start = 0;
    else if (start + 1 == this->length())
        return *this;
    else
        ++start;

    // find the extension(s) part
    size_type pos;
    if (all)
    {
        pos = this->find_first_of(value_type('.'), start);
    }
    else
    {
        pos = this->find_last_of(value_type('.'));
        if (pos < start)
            pos = npos;
    }
    if (pos == start) // when the file name starts with a '.'
        pos = npos;

    if (new_extension.empty())
    {
        if (pos != npos)
            this->erase(pos);
        return *this;
    }

    // replate it/them
    if (pos == npos)
    {
        this->push_back(value_type('.'));
        this->append(new_extension);
    }
    else if (pos == this->length() - 1)
    {
        this->append(new_extension);
    }
    else
    {
        this->erase(pos + 1);
        this->append(new_extension);
    }

    return *this;
}

bool wstr::path_has_ext(const wstr& extension, bool case_sensitive) const
{
    assert(!extension.empty());
    assert(extension[0] != WSTR('.'));
    return
        (this->length() > extension.length() + 1) &&
        this->ends_with(wstr(WSTR(".")) + extension, case_sensitive);
}

wstr wstr::path_last_dir_and_name() const
{
    size_type first = npos;

    if (this->empty())
        return wstr();

    if (wstr::is_pathsep(*this->rbegin()))
    {
        first = this->find_last_not_of(wstr::path_separators());
        if (first == npos)
            return *this;
    }

    first = this->find_last_of(wstr::path_separators(), first);
    if (first == npos)
        return *this;

    first = this->find_last_not_of(wstr::path_separators(), first);
    if (first == npos)
        return *this;

    first = this->find_last_of(wstr::path_separators(), first);
    if (first == npos)
        return *this;

    return this->substr(first + 1);
}

wstr& wstr::path_append(const wstr& tail)
{
    if (!this->empty() && !tail.empty() &&
        !wstr::is_pathsep(this->back()) &&
        !wstr::is_pathsep(tail.front()))
    {
        this->push_back(wstr::native_path_separator());
    }
    return this->append(tail);
}

wstr wstr::path_normalize() const
{
    // PathCanonicalize() is not safe and PathCchCanonicalizeEx() requires Win8+
    // (we target Win7) so we had to reinvent the wheel.
    // Fortunately the path_root() method makes it easy for us and even deals
    // better with malformed paths and mixed separators (use of both '/' and
    // '\\'), as well as sequences of repeated separators.

    if (this->empty())
        return wstr();

    std::list<std::pair<size_type, size_type>> tail;
    size_type tail_pos = 0;
    wstr dest(this->path_root(true, &tail_pos));

    // canonicalize
    if (tail_pos != npos)
    {
        size_type pos = tail_pos;

        for (;;)
        {
            const size_type end_pos = this->find_first_of(
                wstr::path_separators(), pos + 1);
            const size_type part_len =
                (end_pos == npos) ? (this->length() - pos) : (end_pos - pos);

            if (part_len == 0 || part_len == npos)
            {
                assert(0);
                break;
            }

            if ((*this)[pos] == value_type('.'))
            {
                // skip "." element
                if (part_len == 1)
                    goto __continue;

                // apply ".." element
                if (part_len == 2 && (*this)[pos + 1] == value_type('.'))
                {
                    if (!tail.empty())
                        tail.pop_back();
                    goto __continue;
                }
            }

            tail.push_back(std::make_pair(pos, part_len));

        __continue:

            if (end_pos == npos)
                break;

            // skip separator(s)
            pos = this->find_first_not_of(wstr::path_separators(), end_pos + 1);
            if (pos == npos)
                break;
        }

        // reserve enough memory for all the elements of the tail once for all
        // note: we reuse the *pos* variable
        pos = 0;
        for (const auto& element : tail)
            pos += element.second + 1;  // +1 for the separator
        dest.reserve(dest.length() + pos);

        // append the tail elements
        for (const auto& element : tail)
            dest.path_append(wstr(*this, element.first, element.second));
    }

    return dest;
}

wstr wstr::path_normalize_case() const
{
#ifdef _WIN32
    return wstr::path_change_case(*this, false);  // lower case
#else
    // nothing to do
    return *this;
#endif
}

int wstr::path_cmp(const wstr& rhs)
{
    // CAUTION: it is assumed that path_normalize() has been called first for
    // both this and rhs objects

    int res = CompareStringOrdinal(
        this->c_str(), static_cast<int>(this->length()),
        rhs.c_str(), static_cast<int>(rhs.length()),
        TRUE); // ignore case
    if (!res)
    {
        CIX_THROW_WINERR(
            "Failed to compare paths: \"{}\" and \"{}\"",
            wstr::narrow(this->c_str()), wstr::narrow(rhs.c_str()));
    }

    // -2 to be consistent with the C runtime (i.e. <0, ==0 or >0)
    return res - 2;
}




//------------------------------------------------------------------------------
int wstr::cmp(const wchar_t* a, const wchar_t* b, bool case_sensitive, size_type max_length)
{
    int res;

    if (max_length == -1 || max_length == 0)
    {
        res = CompareStringEx(LOCALE_NAME_INVARIANT,
            /*NORM_IGNOREWIDTH |*/ (case_sensitive ? 0 : NORM_IGNORECASE),
            a, -1, b, -1,
            NULL, NULL, 0);
    }
    else
    {
        // FIXME: unsafe cast from size_type to int

        size_type len_a = std::wcslen(a);
        size_type len_b = std::wcslen(b);

        assert(max_length <= static_cast<size_type>(std::numeric_limits<int>::max()));
        assert(len_a <= static_cast<size_type>(std::numeric_limits<int>::max()));
        assert(len_b <= static_cast<size_type>(std::numeric_limits<int>::max()));

        res = CompareStringEx(LOCALE_NAME_INVARIANT,
            /*NORM_IGNOREWIDTH |*/ (case_sensitive ? 0 : NORM_IGNORECASE),
            a, static_cast<int>((len_a > max_length) ? max_length : len_a),
            b, static_cast<int>((len_b > max_length) ? max_length : len_b),
            NULL, NULL, 0);
    }

    if (!res)
    {
        CIX_THROW_WINERR(
            "Failed to compare strings (sensitive: {}; maxlen: {})",
            case_sensitive, max_length);
    }

    // -2 to be consistent with the C runtime (i.e.: <0, ==0 or >0)
    return res - 2;
}

bool wstr::match(const_pointer string, const_pointer pattern, bool case_sensitive)
{
    // code taken from a comment of the article (WildcardMatch_straight):
    // http://www.codeproject.com/Articles/188256/A-Simple-Wildcard-Matching-Function
    // slightly modified to handle case sensitive matching and to be integrated
    // in this class

    const_pointer mp = pattern;
    const_pointer cp = 0;

    while (*string)
    {
        if (*pattern == value_type('*'))
        {
            if (!*++pattern)
                return true;
            mp = pattern;
            cp = string + 1;
        }
        else if (*pattern == value_type('?') || *pattern == *string ||
            (!case_sensitive &&
                wstr::upper(*pattern) == wstr::upper(*string)))
        {
            pattern++;
            string++;
        }
        else if (!cp)
        {
            return false;
        }
        else
        {
            pattern = mp;
            string = cp++;
        }
    }

    while (*pattern == value_type('*'))
        pattern++;

    return !*pattern;
}




//------------------------------------------------------------------------------
wstr& wstr::lower(size_type pos, size_type len)
{
    if (!this->empty() && pos < this->length())
    {
        assert(pos < this->length());
        if (len == -1 || len > (this->length() - pos))
            len = this->length() - pos;
        CharLowerBuffW(
            static_cast<wchar_t*>(&(*this)[pos]),
            static_cast<DWORD>(len));
    }
    return *this;
}

wstr& wstr::upper(size_type pos, size_type len)
{
    if (!this->empty() && pos < this->length())
    {
        assert(pos < this->length());
        if (len == -1 || len > (this->length() - pos))
            len = this->length() - pos;
        CharUpperBuffW(
            static_cast<wchar_t*>(&(*this)[pos]),
            static_cast<DWORD>(len));
    }
    return *this;
}

wstr wstr::path_change_case(const wstr& src, bool upper, size_type pos, size_type len)
{
    if (pos >= src.length() || !len)
        return wstr();

    if (len == npos)
    {
        len = src.length() - pos;
    }
    else
    {
        const auto maxLen = src.length() - pos;
        if (len > maxLen)
            len = maxLen;
    }

    return wstr::path_change_case(src.c_str() + pos, upper, len);
}

wstr wstr::path_change_case(const_pointer src, bool upper, size_type len)
{
    if (!src || !src[0] || !len)
        return wstr();
    if (len == npos)
        len = std::wcslen(src);

    const DWORD flags = upper ? LCMAP_UPPERCASE : LCMAP_LOWERCASE;
    wstr dest;
    int requiredLen;

    // we favor memory usage instead of speed so take the time to know exactly
    // how much space we need to allocate the destination buffer
    requiredLen = LCMapStringEx(
        LOCALE_NAME_INVARIANT, flags,
        src, static_cast<int>(len),
        NULL, 0, // dest
        NULL, NULL, 0);
    if (requiredLen <= 0)
    {
        const auto error = GetLastError();
        assert(0);
        return wstr(src, len);  // unchanged
    }

    // add the terminating null char since we did not count it in *len*
    requiredLen++;

    auto pDest = dest.acquire_buffer(static_cast<size_type>(requiredLen));
    int res = LCMapStringEx(
        LOCALE_NAME_INVARIANT, flags,
        src, static_cast<int>(len),
        pDest, requiredLen,
        NULL, NULL, 0);
    if (res > 0)
    {
        // note: *len* does not take the null-terminating char, so neither does
        // *res*
        dest.release_buffer(static_cast<size_type>(res));
        return dest;
    }
    else
    {
        const auto error = GetLastError();
        assert(0);
        return wstr(src, len);  // unchanged
    }
}




//------------------------------------------------------------------------------
std::string wstr::narrow(const std::wstring& s, size_type src_length, UINT dest_codepage, DWORD flags)
{
    if (src_length == npos)
    {
        src_length = s.length();
    }
    else if (src_length > s.length())
    {
        assert(0);
        src_length = s.length();
    }

    if (!src_length)
        return std::string();

    return wstr::narrow(s.c_str(), src_length, dest_codepage, flags);
}

std::string wstr::narrow(const wchar_t* src, size_type src_length, UINT dest_codepage, DWORD flags)
{
    if (!src || !*src || !src_length)
        return std::string();

    // ensure default value of src_length (i.e.: npos == -1) is compatibale with
    // the WideCharToMultiByte() function
    static_assert(npos == -1, "expected npos value is -1");

    // overflow check
    // MultiByteToWideChar() expects an int so we have to cast src_length
    int i_src_length;
    if (src_length == npos)
    {
        i_src_length = -1;
    }
    else if (src_length <= static_cast<size_type>(std::numeric_limits<int>::max()))
    {
        i_src_length = static_cast<int>(src_length);
    }
    else
    {
        assert(0);
        i_src_length = std::numeric_limits<int>::max();
    }

    // request required size
    int count = WideCharToMultiByte(
        dest_codepage, flags, src, i_src_length, 0, 0, 0, 0);
    if (count <= 0)
    {
        assert(0);
        return std::string();
    }

    // convert
    std::string dest(static_cast<size_type>(count), 0);
    WideCharToMultiByte(
        dest_codepage, flags, src, i_src_length, &dest[0], count, 0, 0);
    if (!dest.empty() && dest[dest.length() - 1] == 0)
    {
        dest.erase(dest.end() - 1);
        --count;
    }
    assert(dest.length() == static_cast<size_type>(count));

    return dest;
}

wstr wstr::widen(const std::string& s, size_type src_length, UINT src_codepage, DWORD flags)
{
    if (src_length == npos)
    {
        src_length = s.length();
    }
    else if (src_length > s.length())
    {
        assert(0);
        src_length = s.length();
    }

    if (!src_length)
        return wstr();

    return wstr::widen(s.c_str(), src_length, src_codepage, flags);
}

wstr wstr::widen(const char* src, size_type src_length, UINT src_codepage, DWORD flags)
{
    if (!src || !*src || !src_length)
        return wstr();

    // ensure default value of src_length (i.e.: npos == -1) is compatibale with
    // the MultiByteToWideChar() function
    static_assert(npos == -1, "expected npos value is -1");

    // overflow check
    // MultiByteToWideChar() expects an int so we have to cast src_length
    int i_src_length;
    if (src_length == npos)
    {
        i_src_length = -1;
    }
    else if (src_length <= static_cast<size_type>(std::numeric_limits<int>::max()))
    {
        i_src_length = static_cast<int>(src_length);
    }
    else
    {
        assert(0);
        i_src_length = std::numeric_limits<int>::max();
    }

    // request required size
    int count = MultiByteToWideChar(src_codepage, flags, src, i_src_length, 0, 0);
    if (count <= 0)
    {
        assert(0);
        return wstr();
    }

    // convert
    wstr dest(static_cast<size_type>(count), 0);
    MultiByteToWideChar(src_codepage, flags, src, i_src_length, &dest[0], count);
    if (!dest.empty() && dest[dest.length() - 1] == 0)
    {
        dest.erase(dest.end() - 1);
        --count;
    }
    assert(dest.length() == static_cast<size_type>(count));

    return dest;
}

}  // namespace cix

#endif  // #ifdef CIX_ENABLE_WSTR
