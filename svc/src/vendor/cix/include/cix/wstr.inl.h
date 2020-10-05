// CIX C++ library
// Copyright (c) Jean-Charles Lefebvre
// SPDX-License-Identifier: MIT

#ifdef CIX_ENABLE_WSTR

namespace cix {

//------------------------------------------------------------------------------
inline wstr::wstr() noexcept
: container() { }

inline wstr::wstr(const allocator_type& alloc) noexcept
: container(alloc) { }

inline wstr::wstr(const wstr& s)
: container(static_cast<const container&>(s)) { }

inline wstr::wstr(const wstr& s, const allocator_type& alloc)
: container(static_cast<const container&>(s), alloc) { }

inline wstr::wstr(const wstr& s, size_type pos, const allocator_type& alloc)
: container(static_cast<const container&>(s), pos, alloc) { }

inline wstr::wstr(const wstr& s, size_type pos, size_type len, const allocator_type& alloc)
: container(static_cast<const container&>(s), pos, len, alloc) { }

inline wstr::wstr(const_pointer s)
: container(s) { }

inline wstr::wstr(const_pointer s, size_type len)
: container(s, len) { }

inline wstr::wstr(const_pointer s, size_type len, const allocator_type& alloc)
: container(s, len, alloc) { }

inline wstr::wstr(size_type repeat, value_type c, const allocator_type& alloc)
: container(repeat, c, alloc) { }

inline wstr::wstr(std::initializer_list<value_type> il, const allocator_type& alloc)
: container(il, alloc) { }

template <class InputIterator>
inline wstr::wstr(InputIterator first, InputIterator last, const allocator_type& alloc)
: container(first, last, alloc) { }

inline wstr::wstr(wstr&& s) noexcept
: container(std::forward<container>(s)) { }

inline wstr::wstr(wstr&& s, const allocator_type& alloc) noexcept
: container(std::forward<container>(s), alloc) { }

template <class T>
inline wstr::wstr(const T& t, const allocator_type& alloc)
: container(t, alloc) { }

template <class T>
inline wstr::wstr(const T& t, size_type pos, size_type len, const allocator_type& alloc)
: container(t, pos, len, alloc) { }


//------------------------------------------------------------------------------
inline wstr::wstr(value_type c)
: container(1, c) { }

inline wstr::wstr(const container& s)
: container(s) { }

inline wstr::wstr(const container& s, const allocator_type& alloc)
: container(s, alloc) { }

inline wstr::wstr(const container& s, size_type pos, const allocator_type& alloc)
: container(s, pos, alloc) { }

inline wstr::wstr(const container& s, size_type pos, size_type len, const allocator_type& alloc)
: container(s, pos, len, alloc) { }

inline wstr::wstr(container&& s) noexcept
: container(std::forward<container>(s)) { }

inline wstr::wstr(container&& s, const allocator_type& alloc) noexcept
: container(std::forward<container>(s), alloc) { }

inline wstr::wstr(const std::string& s, size_type pos, size_type len, UINT src_codepage, DWORD flags)
: container(wstr::widen(s.c_str() + pos, len, src_codepage, flags)) { }

inline wstr::wstr(const char* s, size_type pos, size_type len, UINT src_codepage, DWORD flags)
: container(wstr::widen(s + pos, len, src_codepage, flags)) { }



//------------------------------------------------------------------------------
// inline wstr::operator wstr::container&()
// { return static_cast<wstr::container&>(*this); }

// inline wstr::operator const container&() const
// { return static_cast<const container&>(*this); }

// inline wstr::operator wstr::container*()
// { return static_cast<wstr::container*>(this); }

// inline wstr::operator const wstr::container*() const
// { return static_cast<const wstr::container*>(this); }

inline wstr::view wstr::to_string_view() const
{ return {this->data(), this->size()}; }



//------------------------------------------------------------------------------
inline void wstr::grow(size_type len)
{ this->reserve(len); }



//------------------------------------------------------------------------------
inline wstr& wstr::append(const_pointer s)
{ container::append(s); return *this; }

inline wstr& wstr::append(const_pointer s, size_type len)
{ container::append(s, len); return *this; }

inline wstr& wstr::append(size_type count, value_type c)
{ container::append(count, c); return *this; }

template<class InputIterator>
inline wstr& wstr::append(InputIterator first, InputIterator last)
{ container::append(first, last); return *this; }

inline wstr& wstr::append(const_pointer first, const_pointer last)
{ container::append(first, last); return *this; }

inline wstr& wstr::append(const_iterator first, const_iterator last)
{ container::append(first, last); return *this; }

inline wstr& wstr::assign(const_pointer s)
{ container::assign(s); return *this; }

inline wstr& wstr::assign(const_pointer s, size_type len)
{ container::assign(s, len); return *this; }

inline wstr& wstr::assign(size_type count, value_type c)
{ container::assign(count, c); return *this; }

template<class InputIterator>
inline wstr& wstr::assign(InputIterator first, InputIterator last)
{ container::assign(first, last); return *this; }

inline wstr& wstr::assign(const_pointer first, const_pointer last)
{ container::assign(first, last); return *this; }

inline wstr& wstr::assign(const_iterator first, const_iterator last)
{ container::assign(first, last); return *this; }

inline wstr& wstr::assign(container&& s) noexcept
{ container::assign(std::forward<container>(s)); return *this; }

inline wstr& wstr::assign(std::initializer_list<value_type> il)
{ container::assign(il); return *this; }

inline wstr& wstr::insert(size_type pos, const_pointer s)
{ container::insert(pos, s); return *this; }

inline wstr& wstr::insert(size_type pos, const_pointer s, size_type len)
{ container::insert(pos, s, len); return *this; }

inline wstr& wstr::insert(size_type pos, size_type count, value_type c)
{ container::insert(pos, count, c); return *this; }

inline wstr& wstr::erase(size_type pos, size_type len)
{ container::erase(pos, len); return *this; }

inline wstr::iterator wstr::erase(iterator it)
{ return container::erase(it); }

inline wstr::iterator wstr::erase(iterator first, iterator last)
{ return container::erase(first, last); }

inline wstr& wstr::replace(size_type pos, size_type len, const_pointer s)
{ container::replace(pos, len, s); return *this; }

inline wstr& wstr::replace(size_type pos, size_type len, const_pointer s, size_type src_len)
{ container::replace(pos, len, s, src_len); return *this; }

inline wstr& wstr::replace(size_type pos, size_type len, size_type count, value_type c)
{ container::replace(pos, len, count, c); return *this; }

inline wstr& wstr::replace(iterator first, iterator last, const_pointer s)
{ container::replace(first, last, s); return *this; }

inline wstr& wstr::replace(iterator first, iterator last, const_pointer s, size_type src_len)
{ container::replace(first, last, s, src_len); return *this; }

inline wstr& wstr::replace(iterator first, iterator last, size_type len, value_type c)
{ container::replace(first, last, len, c); return *this; }

template <class InputIterator>
inline wstr& wstr::replace(iterator first, iterator last, InputIterator src_first, InputIterator src_last)
{ container::replace(first, last, src_first, src_last); return *this; }

inline wstr& wstr::replace(iterator first, iterator last, const_pointer src_first, const_pointer src_last)
{ container::replace(first, last, src_first, src_last); return *this; }

inline wstr& wstr::replace(iterator first, iterator last, const_iterator src_first, const_iterator src_last)
{ container::replace(first, last, src_first, src_last); return *this; }



//------------------------------------------------------------------------------
inline wstr& wstr::operator=(const wstr& s)
{ return this->assign(s); }

inline wstr& wstr::operator=(wstr&& s)
{ return this->assign(std::forward<wstr>(s)); }

inline wstr& wstr::operator=(const_pointer s)
{ return this->assign(s); }

inline wstr& wstr::operator=(value_type c)
{ return this->assign(1, c); }

inline wstr& wstr::operator=(std::initializer_list<value_type> il)
{ return this->assign(il); }

inline wstr& wstr::operator+=(const wstr& s)
{ container::append(static_cast<const container&>(s)); return *this; }

inline wstr& wstr::operator+=(const_pointer s)
{ return this->append(s); }

inline wstr& wstr::operator+=(value_type c)
{ container::push_back(c); return *this; }

inline wstr& wstr::operator+=(std::initializer_list<value_type> il)
{ static_cast<container&>(*this) += il; return *this; }

inline wstr& wstr::append(const wstr& s)
{ container::append(static_cast<const container&>(s)); return *this; }

inline wstr& wstr::append(const wstr& s, size_type pos, size_type len)
{ container::append(static_cast<const container&>(s), pos, len); return *this; }

inline wstr& wstr::append(value_type c)
{ container::push_back(c); return *this; }

inline wstr& wstr::append(const std::string& s, size_type pos, size_type len, UINT src_codepage, DWORD flags)
{ container::append(static_cast<const container&>(wstr::widen(s.c_str() + pos, len, src_codepage, flags))); return *this; }

inline wstr& wstr::append(const char* s, size_type pos, size_type len, UINT src_codepage, DWORD flags)
{
    assert(s);
    assert(pos < std::strlen(s));
    return this->append(wstr::widen(s + pos, len, src_codepage, flags));
}

inline wstr& wstr::assign(const wstr& s)
{ container::assign(static_cast<const container&>(s)); return *this; }

inline wstr& wstr::assign(const wstr& s, size_type pos, size_type len)
{ container::assign(static_cast<const container&>(s), pos, len); return *this; }

inline wstr& wstr::assign(value_type c)
{ container::assign(1, c); return *this; }

inline wstr& wstr::assign(const std::string& s, size_type pos, size_type len, UINT src_codepage, DWORD flags)
{
    assert((s.empty() && pos == 0) || (pos < s.length()));
    return this->assign(wstr::widen(s.c_str() + pos, len, src_codepage, flags));
}

inline wstr& wstr::assign(const char* s, size_type pos, size_type len, UINT src_codepage, DWORD flags)
{
    assert(s);
    assert((!*s && pos == 0) || (pos < std::strlen(s)));
    return this->assign(wstr::widen(s + pos, len, src_codepage, flags));
}

inline wstr& wstr::assign(wstr&& s) noexcept
{ container::assign(std::forward<container>(s)); return *this; }

inline wstr& wstr::insert(size_type pos, const wstr& s)
{ container::insert(pos, static_cast<const container&>(s)); return *this; }

inline wstr& wstr::insert(size_type pos, const wstr& s, size_type src_pos, size_type src_len)
{ container::insert(pos, static_cast<const container&>(s), src_pos, src_len); return *this; }

inline wstr& wstr::insert(size_type pos, const std::string& s, size_type src_pos, size_type src_len, UINT src_codepage, DWORD flags)
{ container::insert(pos, static_cast<const container&>(wstr::widen(s.c_str() + src_pos, src_len, src_codepage, flags))); return *this; }

inline wstr& wstr::insert(size_type pos, const char* s, size_type src_pos, size_type src_len, UINT src_codepage, DWORD flags)
{
    assert(s);
    assert((!*s && src_pos == 0) || (src_pos < std::strlen(s)));
    return this->insert(pos, wstr::widen(s + src_pos, src_len, src_codepage, flags));
}

inline wstr& wstr::replace(size_type pos, size_type len, const wstr& s)
{ container::replace(pos, len, s); return *this; }

inline wstr& wstr::replace(size_type pos, size_type len, const wstr& s, size_type src_pos, size_type src_len)
{ container::replace(pos, len, s, src_pos, src_len); return *this; }

inline wstr& wstr::replace(iterator first, iterator last, const wstr& s)
{ container::replace(first, last, s); return *this; }

inline void wstr::swap(wstr& s)
{ container::swap(static_cast<container&>(s)); }



//------------------------------------------------------------------------------
inline wstr& wstr::operator-=(const wstr& s)
{ return this->insert(0, s); }

inline wstr& wstr::operator-=(const_pointer s)
{ return this->insert(0, s); }

inline wstr& wstr::operator-=(value_type c)
{ return this->insert(0, 1, c); }

inline wstr& wstr::operator-=(std::initializer_list<value_type> il)
{ return this->insert(0, wstr(il)); }

inline wstr& wstr::prepend(const wstr& s)
{ return this->insert(0, s); }

inline wstr wstr::copy_replace_all(const wstr& from, const wstr& to) const
{ wstr copy(*this); copy.replace_all(from, to); return copy; }

inline wstr& wstr::trim()
{ return this->ltrim().rtrim(); }

inline wstr& wstr::trim(value_type c)
{ return this->ltrim(c).rtrim(c); }

inline wstr wstr::copy_ltrim() const
{ wstr copy(*this); copy.ltrim(); return copy; }

inline wstr wstr::copy_rtrim() const
{ wstr copy(*this); copy.rtrim(); return copy; }

inline wstr wstr::copy_trim() const
{ wstr copy(*this); copy.trim(); return copy; }



//------------------------------------------------------------------------------
inline wstr wstr::substr(size_type pos, size_type len) const
{ return wstr(container::substr(pos, len)); }




//------------------------------------------------------------------------------
inline wstr::size_type wstr::find(const wstr& s, size_type pos) const noexcept
{ return container::find(static_cast<const container&>(s), pos); }

inline wstr::size_type wstr::rfind(const wstr& s, size_type pos) const noexcept
{ return container::rfind(static_cast<const container&>(s), pos); }

inline wstr::size_type wstr::find_first_of(const wstr& s, size_type pos) const noexcept
{ return container::find_first_of(static_cast<const container&>(s), pos); }

inline wstr::size_type wstr::find_last_of(const wstr& s, size_type pos) const noexcept
{ return container::find_last_of(static_cast<const container&>(s), pos); }

inline wstr::size_type wstr::find_first_not_of(const wstr& s, size_type pos) const noexcept
{ return container::find_first_not_of(static_cast<const container&>(s), pos); }

inline wstr::size_type wstr::find_last_not_of(const wstr& s, size_type pos) const noexcept
{ return container::find_last_not_of(static_cast<const container&>(s), pos); }

inline bool wstr::contains(const wstr& s, bool case_sensitive, size_type pos) const
{ return (case_sensitive ? this->find(s, pos) : this->findi(s, pos)) != npos; }



//------------------------------------------------------------------------------
inline int wstr::cmp(const wstr& s, bool case_sensitive, size_type max_length) const
{ return wstr::cmp(this->c_str(), s.c_str(), case_sensitive, max_length); }

inline bool wstr::equals(const wstr& s, bool case_sensitive) const
{
    return
        this->length() == s.length() &&
        0 == wstr::cmp(this->c_str(), s.c_str(), case_sensitive);
}

inline bool wstr::matches(const wstr& pattern, bool case_sensitive) const
{ return wstr::match(this->c_str(), pattern.c_str(), case_sensitive); }



//------------------------------------------------------------------------------
inline wstr wstr::copy_lower(size_type pos, size_type len) const
{ return wstr(*this, pos, len).lower(); }

inline wstr wstr::copy_upper(size_type pos, size_type len) const
{ return wstr(*this, pos, len).upper(); }



//------------------------------------------------------------------------------
inline std::string wstr::to_narrow(size_type pos, size_type len, UINT dest_codepage) const
{
    assert(this->empty() || pos < this->length());
    return wstr::narrow(this->c_str() + pos, len, dest_codepage);
}

inline std::string wstr::to_utf8(size_type pos, size_type len) const
{
    assert(this->empty() || pos < this->length());
    return wstr::narrow(this->c_str() + pos, len, CP_UTF8);
}



//------------------------------------------------------------------------------
inline bool wstr::is_blank() const
{
    return std::all_of<wstr::const_iterator>(
        this->cbegin(), this->cend(),
        static_cast<bool(*)(wchar_t)>(&wstr::is_blank));
}

inline bool wstr::is_space() const
{
    return std::all_of<wstr::const_iterator>(
        this->cbegin(), this->cend(),
        static_cast<bool(*)(wchar_t)>(&wstr::is_space));
}

inline bool wstr::is_alnum() const
{
    return std::all_of<wstr::const_iterator>(
        this->cbegin(), this->cend(),
        static_cast<bool(*)(wchar_t)>(&wstr::is_alnum));
}



//------------------------------------------------------------------------------
template <typename... Args>
wstr& wstr::fmt(const ::fmt::basic_string_view<value_type>& format, Args&&... args)
{ return this->vfmt(format, ::fmt::make_format_args<::fmt::buffer_context<value_type>>(args...)); }

template <typename... Args>
wstr& wstr::fmt_append(const ::fmt::basic_string_view<value_type>& format, Args&&... args)
{ return this->vfmt_append(format, ::fmt::make_format_args<::fmt::buffer_context<value_type>>(args...)); }

template <typename... Args>
wstr& wstr::fmt_prepend(const ::fmt::basic_string_view<value_type>& format, Args&&... args)
{ return this->vfmt_prepend(format, ::fmt::make_format_args<::fmt::buffer_context<value_type>>(args...)); }



//------------------------------------------------------------------------------
inline wstr& wstr::operator/=(const wstr& s)
{ return this->path_append(s); }

inline wstr& wstr::operator/=(const_pointer s)
{ return this->path_append(wstr(s)); }

inline wstr& wstr::operator/=(value_type c)
{ return this->path_append(wstr(1, c)); }

inline bool wstr::path_has_trailsep() const
{ return !this->empty() && wstr::is_pathsep(this->back()); }

inline std::vector<wstr> wstr::path_split() const
{ return this->split_one_of(wstr::path_separators(), false, 0); }

inline wstr& wstr::path_join(const std::vector<wstr>& vec, size_type start_index, size_type max_fields)
{ return this->join(vec, wstr(1, wstr::native_path_separator()), false, start_index, max_fields); }

inline wstr wstr::path_strip_ext(bool remove_all_extensions) const
{ return wstr(*this).path_replace_ext(remove_all_extensions, wstr()); }

inline wstr& wstr::path_win()
{ return this->replace_all(WSTR("/"), WSTR("\\")); }

inline wstr& wstr::path_unix()
{ return this->replace_all(WSTR("\\"), WSTR("/")); }

inline wstr& wstr::path_native()
{
#ifdef _WIN32
    return this->path_win();
#else
    return this->path_unix();
#endif
}



//------------------------------------------------------------------------------
inline wstr operator+(const wstr& lhs, const wstr& rhs)
{ return wstr(lhs).append(rhs); }

inline wstr operator+(const wstr& lhs, wstr::const_pointer rhs)
{ return wstr(lhs).append(rhs); }

inline wstr operator+(const wstr& lhs, wstr::value_type rhs)
{ return wstr(lhs).append(1, rhs); }

inline wstr operator+(wstr::const_pointer lhs, const wstr& rhs)
{ return wstr(lhs).append(rhs); }

inline wstr operator+(wstr::value_type lhs, const wstr& rhs)
{ return wstr(1, lhs).append(rhs); }

inline wstr operator/(const wstr& lhs, const wstr& rhs)
{ return wstr(lhs).path_append(rhs); }

inline wstr operator/(const wstr& lhs, wstr::const_pointer rhs)
{ return wstr(lhs).path_append(rhs); }

inline wstr operator/(const wstr& lhs, wstr::value_type rhs)
{ return wstr(lhs).path_append(wstr(1, rhs)); }

inline wstr operator/(wstr::const_pointer lhs, const wstr& rhs)
{ return wstr(lhs).path_append(rhs); }

inline wstr operator/(wstr::value_type lhs, const wstr& rhs)
{ return wstr(1, lhs).path_append(rhs); }

inline bool operator==(const wstr& lhs, const wstr& rhs)
{ return static_cast<const wstr::container&>(lhs) == static_cast<const wstr::container&>(rhs); }

inline bool operator==(const wstr& lhs, wstr::const_pointer rhs)
{ return static_cast<const wstr::container&>(lhs) == rhs; }

inline bool operator==(wstr::const_pointer lhs, const wstr& rhs)
{ return lhs == static_cast<const wstr::container&>(rhs); }

inline bool operator==(const wstr& lhs, const wstr::container& rhs)
{ return static_cast<const wstr::container&>(lhs) == rhs; }

inline bool operator==(const wstr::container& lhs, const wstr& rhs)
{ return rhs == static_cast<const wstr::container&>(lhs); }

inline bool operator!=(const wstr& lhs, const wstr& rhs)
{ return static_cast<const wstr::container&>(lhs) != static_cast<const wstr::container&>(rhs); }

inline bool operator!=(const wstr& lhs, wstr::const_pointer rhs)
{ return static_cast<const wstr::container&>(lhs) != rhs; }

inline bool operator!=(wstr::const_pointer lhs, const wstr& rhs)
{ return lhs != static_cast<const wstr::container&>(rhs); }

inline bool operator!=(const wstr& lhs, const wstr::container& rhs)
{ return static_cast<const wstr::container&>(lhs) != rhs; }

inline bool operator!=(const wstr::container& lhs, const wstr& rhs)
{ return rhs != static_cast<const wstr::container&>(lhs); }

inline bool operator<(const wstr& lhs, const wstr& rhs)
{ return static_cast<const wstr::container&>(lhs) < static_cast<const wstr::container&>(rhs); }

inline bool operator<(const wstr& lhs, wstr::const_pointer rhs)
{ return static_cast<const wstr::container&>(lhs) < rhs; }

inline bool operator<(wstr::const_pointer lhs, const wstr& rhs)
{ return lhs < static_cast<const wstr::container&>(rhs); }

inline bool operator<(const wstr& lhs, const wstr::container& rhs)
{ return static_cast<const wstr::container&>(lhs) < rhs; }

inline bool operator<(const wstr::container& lhs, const wstr& rhs)
{ return rhs < static_cast<const wstr::container&>(lhs); }

inline bool operator<=(const wstr& lhs, const wstr& rhs)
{ return static_cast<const wstr::container&>(lhs) <= static_cast<const wstr::container&>(rhs); }

inline bool operator<=(const wstr& lhs, wstr::const_pointer rhs)
{ return static_cast<const wstr::container&>(lhs) <= rhs; }

inline bool operator<=(wstr::const_pointer lhs, const wstr& rhs)
{ return lhs <= static_cast<const wstr::container&>(rhs); }

inline bool operator<=(const wstr& lhs, const wstr::container& rhs)
{ return static_cast<const wstr::container&>(lhs) <= rhs; }

inline bool operator<=(const wstr::container& lhs, const wstr& rhs)
{ return rhs <= static_cast<const wstr::container&>(lhs); }

inline bool operator>(const wstr& lhs, const wstr& rhs)
{ return static_cast<const wstr::container&>(lhs) > static_cast<const wstr::container&>(rhs); }

inline bool operator>(const wstr& lhs, wstr::const_pointer rhs)
{ return static_cast<const wstr::container&>(lhs) > rhs; }

inline bool operator>(wstr::const_pointer lhs, const wstr& rhs)
{ return lhs > static_cast<const wstr::container&>(rhs); }

inline bool operator>(const wstr& lhs, const wstr::container& rhs)
{ return static_cast<const wstr::container&>(lhs) > rhs; }

inline bool operator>(const wstr::container& lhs, const wstr& rhs)
{ return rhs > static_cast<const wstr::container&>(lhs); }

inline bool operator>=(const wstr& lhs, const wstr& rhs)
{ return static_cast<const wstr::container&>(lhs) >= static_cast<const wstr::container&>(rhs); }

inline bool operator>=(const wstr& lhs, wstr::const_pointer rhs)
{ return static_cast<const wstr::container&>(lhs) >= rhs; }

inline bool operator>=(wstr::const_pointer lhs, const wstr& rhs)
{ return lhs >= static_cast<const wstr::container&>(rhs); }

inline bool operator>=(const wstr& lhs, const wstr::container& rhs)
{ return static_cast<const wstr::container&>(lhs) >= rhs; }

inline bool operator>=(const wstr::container& lhs, const wstr& rhs)
{ return rhs >= static_cast<const wstr::container&>(lhs); }



//------------------------------------------------------------------------------
template <class InputIt>
InputIt wstr::findi(InputIt first, InputIt last, const wstr& to_find)
{
    return std::find_if(first, last,
        [&to_find](std::iterator_traits<InputIt>::reference ref) {
            return ref.equals(to_find, false);
        });
}



//------------------------------------------------------------------------------
inline wchar_t wstr::lower(wchar_t c)
{ CharLowerBuff(static_cast<wchar_t*>(&c), 1); return c; }

inline wchar_t wstr::upper(wchar_t c)
{ CharUpperBuff(static_cast<wchar_t*>(&c), 1); return c; }

inline bool wstr::is_lower(wchar_t c)
{ return IsCharLower(c) != 0; }

inline bool wstr::is_upper(wchar_t c)
{ return IsCharUpper(c) != 0; }



//------------------------------------------------------------------------------
#define _WSTR_IS(method, func) \
    inline bool wstr::method(wchar_t c) \
    { return 0 != std::func(static_cast<wint_t>(c)); }

_WSTR_IS(is_blank, iswblank);
_WSTR_IS(is_space, iswspace);
_WSTR_IS(is_alpha, iswalpha);
_WSTR_IS(is_alnum, iswalnum);
_WSTR_IS(is_digit, iswdigit);
_WSTR_IS(is_xdigit, iswxdigit);
_WSTR_IS(is_cntrl, iswcntrl);
_WSTR_IS(is_print, iswprint);

#undef _WSTR_IS

inline bool wstr::is_asciiletter(value_type c)
{
    return
        (c >= value_type('A') && c <= value_type('Z')) ||
        (c >= value_type('a') && c <= value_type('z'));
}

inline bool wstr::is_pathsep(value_type c)
{ return c == value_type('/') || c == value_type('\\'); }

inline bool wstr::is_pathsep_unix(value_type c)
{ return c == value_type('/'); }

inline bool wstr::is_pathsep_win(value_type c)
{ return c == value_type('\\'); }

inline wstr::const_pointer wstr::path_separators()
{ static const_pointer c_sep = WSTR("/\\"); return c_sep; }

inline wstr::value_type wstr::native_path_separator()
{
#ifdef _WIN32
    return value_type('\\');
#else
    return value_type('/');
#endif
}

inline wstr::const_pointer wstr::wildcards()
{ static const_pointer c_wildcards = WSTR("*?"); return c_wildcards; }

inline wstr::value_type wstr::replacement_char()
{ return 0xFFFD; }

}  // namespace cix

#endif  // #ifdef CIX_ENABLE_WSTR
