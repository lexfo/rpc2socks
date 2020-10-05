// CIX C++ library
// Copyright (c) Jean-Charles Lefebvre
// SPDX-License-Identifier: MIT

#pragma once

#include "detail/ensure_cix.h"

#ifdef CIX_ENABLE_WSTR

namespace cix {

//------------------------------------------------------------------------------
#define WSTR(x)    __WSTR(x)
#define __WSTR(x)  L##x


//------------------------------------------------------------------------------
class wstr : public std::wstring
{
public:
    typedef std::wstring container;

    using container::size_type;
    using container::difference_type;

    using container::value_type;       // CharT
    using container::reference;        // value_type&
    using container::const_reference;  // const value_type&
    using container::pointer;          // value_type*
    using container::const_pointer;    // const value_type*

    using container::iterator;
    using container::const_iterator;
    using container::reverse_iterator;
    using container::const_reverse_iterator;

    using container::allocator_type;
    using container::traits_type;

    using container::npos;

    typedef std::basic_string_view<value_type, traits_type> view;

public:
    // a case-insensitive "less" operator
    // it is convenient to create a case-insensitive map
    // example: std::map<wstr, int, wstr::less_ci> my_map;
    struct less_ci
    {
        bool operator()(const wstr& lhs, const wstr& rhs) const
            { return lhs.cmp(rhs, false) < 0; }
    };

    struct less
    {
        bool cs;

        // case-sensitive by default, as opposed to wstr::less_ci
        less() : cs(true) { }
        less(bool caseSensitive) : cs(caseSensitive) { }
        bool operator()(const wstr& lhs, const wstr& rhs) const
            { return lhs.cmp(rhs, cs) < 0; }
    };

public:
    // static_init() and static_uninit() methods must be called respectively at
    // the beginning and the end of each process
    static void static_init();
    static bool static_isinit();
    static void static_uninit();

    wstr() noexcept;
    inline ~wstr() = default;
    explicit wstr(const allocator_type& alloc) noexcept;

    // standard constructors
    wstr(const wstr& s);
    wstr(const wstr& s, const allocator_type& alloc);
    wstr(const wstr& s, size_type pos, const allocator_type& alloc=allocator_type());
    wstr(const wstr& s, size_type pos, size_type len, const allocator_type& alloc=allocator_type());
    wstr(const_pointer s);
    wstr(const_pointer s, size_type len);
    wstr(const_pointer s, size_type len, const allocator_type& alloc);
    wstr(size_type repeat, value_type c, const allocator_type& alloc=allocator_type());
    wstr(std::initializer_list<value_type> il, const allocator_type& alloc=allocator_type());
    template <class InputIterator>
    wstr(InputIterator first, InputIterator last, const allocator_type& alloc=allocator_type());
    wstr(wstr&& s) noexcept;
    wstr(wstr&& s, const allocator_type& alloc) noexcept;
    template <class T>
    wstr(const T& t, const allocator_type& alloc=allocator_type());
    template <class T>
    wstr(const T& t, size_type pos, size_type len, const allocator_type& alloc=allocator_type());

    // extra constructors (converters)
    explicit wstr(value_type c);
             wstr(const container& s);
             wstr(const container& s, const allocator_type& alloc);
             wstr(const container& s, size_type pos, const allocator_type& alloc=allocator_type());
             wstr(const container& s, size_type pos, size_type len, const allocator_type& alloc=allocator_type());
             wstr(container&& s) noexcept;
             wstr(container&& s, const allocator_type& alloc) noexcept;
    explicit wstr(const std::string& s, size_type pos=0, size_type len=npos, UINT src_codepage=CP_UTF8, DWORD flags=0);
    explicit wstr(const char* s, size_type pos=0, size_type len=npos, UINT src_codepage=CP_UTF8, DWORD flags=0);

    // type conversion
    // operator container&();
    // operator const container&() const;
    // explicit operator container*();
    // explicit operator const container*() const;
    view to_string_view() const;

    // direct write access to buffer
    // * acquire_buffer() must always be followed by a call to release_buffer()
    pointer acquire_buffer(size_type len);
    wstr& release_buffer(size_type len=npos);  // does not shrink buffer

    // standard iterators
    using container::begin;
    using container::end;
    using container::cbegin;
    using container::cend;
    using container::rbegin;
    using container::rend;
    using container::crbegin;
    using container::crend;

    // standard capacity methods
    using container::clear;
    using container::empty;
    using container::shrink_to_fit;
    using container::size;  // CharT count in the string, excluding the null trailing sequence
    using container::length;  // synonym of size()
    using container::max_size;
    using container::resize;
    using container::capacity;  // the size() it can hold
    using container::reserve;
    void grow(size_type len);  // reserve()

    // standard element access
    using container::operator[];  // faster than at(), no check performed
    using container::at;          // more secure than operator[]
    using container::back;
    using container::front;

    // standard modifiers
    using container::push_back;
    using container::insert;
    using container::swap;
    using container::pop_back;
    wstr& append(const_pointer s);
    wstr& append(const_pointer s, size_type len);
    wstr& append(size_type count, value_type c);
    template<class InputIterator>
    wstr& append(InputIterator first, InputIterator last);
    wstr& append(const_pointer first, const_pointer last);
    wstr& append(const_iterator first, const_iterator last);
    wstr& assign(const_pointer s);
    wstr& assign(const_pointer s, size_type len);
    wstr& assign(size_type count, value_type c);
    template<class InputIterator>
    wstr& assign(InputIterator first, InputIterator last);
    wstr& assign(const_pointer first, const_pointer last);
    wstr& assign(const_iterator first, const_iterator last);
    wstr& assign(container&& s) noexcept;
    wstr& assign(std::initializer_list<value_type> il);
    wstr& insert(size_type pos, const_pointer s);
    wstr& insert(size_type pos, const_pointer s, size_type len);
    wstr& insert(size_type pos, size_type count, value_type c);
    wstr& erase(size_type pos=0, size_type len=npos);
    iterator erase(iterator it);
    iterator erase(iterator first, iterator last);
    wstr& replace(size_type pos, size_type len, const_pointer s);
    wstr& replace(size_type pos, size_type len, const_pointer s, size_type src_len);
    wstr& replace(size_type pos, size_type len, size_type count, value_type c);
    wstr& replace(iterator first, iterator last, const_pointer s);
    wstr& replace(iterator first, iterator last, const_pointer s, size_type src_len);
    wstr& replace(iterator first, iterator last, size_type len, value_type c);
    template <class InputIterator>
    wstr& replace(iterator first, iterator last, InputIterator src_first, InputIterator src_last);
    wstr& replace(iterator first, iterator last, const_pointer src_first, const_pointer src_last);
    wstr& replace(iterator first, iterator last, const_iterator src_first, const_iterator src_last);

    // overloaded standard modifiers
    wstr& operator=(const wstr& s);
    wstr& operator=(wstr&& s);
    wstr& operator=(const_pointer s);
    wstr& operator=(value_type c);
    wstr& operator=(std::initializer_list<value_type> il);
    wstr& operator+=(const wstr& s);
    wstr& operator+=(const_pointer s);
    wstr& operator+=(value_type c);
    wstr& operator+=(std::initializer_list<value_type> il);
    wstr& append(const wstr& s);
    wstr& append(const wstr& s, size_type pos, size_type len=npos);
    wstr& append(value_type c);
    wstr& append(const std::string& s, size_type pos=0, size_type len=npos, UINT src_codepage=CP_UTF8, DWORD flags=0);
    wstr& append(const char* s, size_type pos=0, size_type len=npos, UINT src_codepage=CP_UTF8, DWORD flags=0);
    wstr& assign(const wstr& s);
    wstr& assign(const wstr& s, size_type pos, size_type len=npos);
    wstr& assign(value_type c);
    wstr& assign(const std::string& s, size_type pos=0, size_type len=npos, UINT src_codepage=CP_UTF8, DWORD flags=0);
    wstr& assign(const char* s, size_type pos=0, size_type len=npos, UINT src_codepage=CP_UTF8, DWORD flags=0);
    wstr& assign(wstr&& s) noexcept;
    wstr& insert(size_type pos, const wstr& s);
    wstr& insert(size_type pos, const wstr& s, size_type src_pos, size_type src_len);
    wstr& insert(size_type pos, const std::string& s, size_type src_pos=0, size_type src_len=npos, UINT src_codepage=CP_UTF8, DWORD flags=0);
    wstr& insert(size_type pos, const char* s, size_type src_pos=0, size_type src_len=npos, UINT src_codepage=CP_UTF8, DWORD flags=0);
    wstr& erase_tail(size_type tail_len);
    wstr& replace(size_type pos, size_type len, const wstr& s);
    wstr& replace(size_type pos, size_type len, const wstr& s, size_type src_pos, size_type src_len);
    wstr& replace(iterator first, iterator last, const wstr& s);
    void swap(wstr& rhs);

    // extra modifiers
    wstr& operator-=(const wstr& s);  // prepend
    wstr& operator-=(const_pointer s);
    wstr& operator-=(value_type c);
    wstr& operator-=(std::initializer_list<value_type> il);
    wstr& prepend(const wstr& s);
    wstr& replace_all(const wstr& from, const wstr& to, size_type pos=0);
    wstr copy_replace_all(const wstr& from, const wstr& to) const;
    wstr& ltrim();
    wstr& ltrim(value_type c);
    wstr& rtrim();
    wstr& rtrim(value_type c);
    wstr& trim();
    wstr& trim(value_type c);
    wstr copy_ltrim() const;
    wstr copy_rtrim() const;
    wstr copy_trim() const;
    std::vector<wstr> split(const wstr& separator, bool case_sensitive, bool keep_empty_fields=false, size_type max_fields=0) const;
    std::vector<wstr> split_one_of(const wstr& separators, bool keep_empty_fields=false, size_type max_fields=0) const;
    wstr& join(const std::vector<wstr>& vec, const wstr& glue, bool keep_empty_fields=false, size_type start_index=0, size_type max_fields=0);

    // standard low-level string operations
    using container::c_str;
    using container::data;
    using container::get_allocator;
    using container::copy;
    using container::find;
    using container::rfind;
    using container::find_first_of;
    using container::find_last_of;
    using container::find_first_not_of;
    using container::find_last_not_of;
    // using container::compare;
    wstr substr(size_type pos=0, size_type len=npos) const;

    // overloaded standard string operations
    size_type find(const wstr& s, size_type pos=0) const noexcept;
    size_type rfind(const wstr& s, size_type pos=npos) const noexcept;
    size_type find_first_of(const wstr& s, size_type pos=0) const noexcept;
    size_type find_last_of(const wstr& s, size_type pos=npos) const noexcept;
    size_type find_first_not_of(const wstr& s, size_type pos=0) const noexcept;
    size_type find_last_not_of(const wstr& s, size_type pos=npos) const noexcept;

    // extra string operations
    size_type findi(const wstr& s, size_type pos=0) const;
    size_type rfindi(const wstr& s, size_type pos=0) const;
    bool begins_with(const wstr& s, bool case_sensitive) const;
    bool ends_with(const wstr& s, bool case_sensitive) const;
    bool contains(const wstr& s, bool case_sensitive, size_type pos=0) const;
    size_type begins_with_any_of(const std::vector<wstr>& vec, bool case_sensitive) const;
    size_type ends_with_any_of(const std::vector<wstr>& vec, bool case_sensitive) const;
    size_type contains_any_of(const std::vector<wstr>& vec, bool case_sensitive, size_type pos=0) const;

    // comparison
    // * cmp() method is not named "compare()" to avoid interfering with the
    //   standard method(s) and to emphasize the fact that its arguments and
    //   behavior have different meaning than the original compare() methods
    int cmp(const wstr& s, bool case_sensitive, size_type max_length=npos) const;
    bool equals(const wstr& s, bool case_sensitive) const;
    bool matches(const wstr& pattern, bool case_sensitive) const;
    size_type equals_any_of(const std::vector<wstr>& vec, bool case_sensitive) const;
    size_type matches_any_of(const std::vector<wstr>& vec, bool case_sensitive) const;

    // case converters
    wstr& lower(size_type pos=0, size_type len=npos);
    wstr& upper(size_type pos=0, size_type len=npos);
    wstr copy_lower(size_type pos=0, size_type len=npos) const;
    wstr copy_upper(size_type pos=0, size_type len=npos) const;

    // converters
    std::string to_narrow(size_type pos=0, size_type len=npos, UINT dest_codepage=CP_UTF8) const;
    std::string to_utf8(size_type pos=0, size_type len=npos) const;
    std::int64_t to_int(size_type pos=0, int from_base=0) const;
    std::uint64_t to_uint(size_type pos=0, int from_base=0) const;
    double to_float(size_type pos=0) const;
    wstr to_normalized() const;

    // introspection
    bool is_blank() const;  // tab and space
    bool is_space() const;  // [blank] + new lines and vertical tabs
    bool is_alnum() const;
    bool is_int(bool accept_blank) const;
    bool is_digit(bool accept_blank) const;
    bool is_hex(bool accept_blank) const;
    bool is_float(bool accept_blank) const;

    // format
    template <typename... Args> wstr& fmt(const fmt::basic_string_view<value_type>& format, Args&&... args);
    template <typename... Args> wstr& fmt_append(const fmt::basic_string_view<value_type>& format, Args&&... args);
    template <typename... Args> wstr& fmt_prepend(const fmt::basic_string_view<value_type>& format, Args&&... args);
    wstr& vfmt(const fmt::basic_string_view<value_type>& format, fmt::basic_format_args<fmt::buffer_context<value_type>> args);
    wstr& vfmt_append(const fmt::basic_string_view<value_type>& format, fmt::basic_format_args<fmt::buffer_context<value_type>> args);
    wstr& vfmt_prepend(const fmt::basic_string_view<value_type>& format, fmt::basic_format_args<fmt::buffer_context<value_type>> args);

    // operations on path
    wstr& operator/=(const wstr& s);
    wstr& operator/=(const_pointer s);
    wstr& operator/=(value_type c);
    bool path_is_absolute() const;
    bool path_has_trailsep() const;
    std::vector<wstr> path_split() const;
    wstr& path_join(const std::vector<wstr>& vec, size_type start_index=0, size_type max_fields=0);
    value_type path_drive_letter() const;
    wstr path_root(bool normalize=true, size_type* out_tail_pos=nullptr) const;
    wstr path_dir() const;
    wstr path_name() const;
    wstr path_title(bool remove_all_extensions=false) const;
    wstr path_ext(bool all) const;
    wstr path_strip_ext(bool remove_all_extensions=false) const;
    wstr& path_replace_ext(bool all, const wstr& new_extension);
    bool path_has_ext(const wstr& extension, bool case_sensitive=false) const;
    wstr path_last_dir_and_name() const;
    wstr& path_append(const wstr& tail);
    wstr& path_win();
    wstr& path_unix();
    wstr& path_native();
    wstr path_normalize() const;  // canonicalizes as well
    wstr path_normalize_case() const;
    int path_cmp(const wstr& rhs);  // path_normalize() MUST BE CALLED FIRST!

    // friend operators
    friend wstr operator+(const wstr& lhs, const wstr& rhs);
    friend wstr operator+(const wstr& lhs, const_pointer rhs);
    friend wstr operator+(const wstr& lhs, value_type rhs);
    friend wstr operator+(const_pointer lhs, const wstr& rhs);
    friend wstr operator+(value_type lhs, const wstr& rhs);

    friend wstr operator/(const wstr& lhs, const wstr& rhs);
    friend wstr operator/(const wstr& lhs, const_pointer rhs);
    friend wstr operator/(const wstr& lhs, value_type rhs);
    friend wstr operator/(const_pointer lhs, const wstr& rhs);
    friend wstr operator/(value_type lhs, const wstr& rhs);

    friend bool operator==(const wstr& lhs, const wstr& rhs);
    friend bool operator==(const wstr& lhs, const_pointer rhs);
    friend bool operator==(const_pointer lhs, const wstr& rhs);
    friend bool operator==(const wstr& lhs, const container& rhs);
    friend bool operator==(const container& lhs, const wstr& rhs);

    friend bool operator!=(const wstr& lhs, const wstr& rhs);
    friend bool operator!=(const wstr& lhs, const_pointer rhs);
    friend bool operator!=(const_pointer lhs, const wstr& rhs);
    friend bool operator!=(const wstr& lhs, const container& rhs);
    friend bool operator!=(const container& lhs, const wstr& rhs);

    friend bool operator<(const wstr& lhs, const wstr& rhs);
    friend bool operator<(const wstr& lhs, const_pointer rhs);
    friend bool operator<(const_pointer lhs, const wstr& rhs);
    friend bool operator<(const wstr& lhs, const container& rhs);
    friend bool operator<(const container& lhs, const wstr& rhs);

    friend bool operator<=(const wstr& lhs, const wstr& rhs);
    friend bool operator<=(const wstr& lhs, const_pointer rhs);
    friend bool operator<=(const_pointer lhs, const wstr& rhs);
    friend bool operator<=(const wstr& lhs, const container& rhs);
    friend bool operator<=(const container& lhs, const wstr& rhs);

    friend bool operator>(const wstr& lhs, const wstr& rhs);
    friend bool operator>(const wstr& lhs, const_pointer rhs);
    friend bool operator>(const_pointer lhs, const wstr& rhs);
    friend bool operator>(const wstr& lhs, const container& rhs);
    friend bool operator>(const container& lhs, const wstr& rhs);

    friend bool operator>=(const wstr& lhs, const wstr& rhs);
    friend bool operator>=(const wstr& lhs, const_pointer rhs);
    friend bool operator>=(const_pointer lhs, const wstr& rhs);
    friend bool operator>=(const wstr& lhs, const container& rhs);
    friend bool operator>=(const container& lhs, const wstr& rhs);

public:
    // case-insensitive find in a container
    template <class InputIt>
    static InputIt findi(InputIt first, InputIt last, const wstr& to_find);

    // comparison
    static int cmp(const wchar_t* a, const wchar_t* b, bool case_sensitive, size_type max_length=npos);
    static bool match(const_pointer string, const_pointer pattern, bool case_sensitive);

    // case converters
    static wchar_t lower(wchar_t c);
    static wchar_t upper(wchar_t c);
    static bool is_lower(wchar_t c);
    static bool is_upper(wchar_t c);
    static wstr path_change_case(const wstr& src, bool upper, size_type pos=0, size_type len=npos);
    static wstr path_change_case(const_pointer src, bool upper, size_type len=npos);

    // converters
    static std::string narrow(const std::wstring& s, size_type src_length=npos, UINT dest_codepage=CP_UTF8, DWORD flags=0);
    static std::string narrow(const wchar_t* s, size_type src_length=npos, UINT dest_codepage=CP_UTF8, DWORD flags=0);
    static wstr widen(const std::string& s, size_type src_length=npos, UINT src_codepage=CP_UTF8, DWORD flags=0);
    static wstr widen(const char* s, size_type src_length=npos, UINT src_codepage=CP_UTF8, DWORD flags=0);

    // introspection
    static bool is_blank(wchar_t c);  // tab and space
    static bool is_space(wchar_t c);  // [blank] + new lines and vertical tabs
    static bool is_alpha(wchar_t c);
    static bool is_alnum(wchar_t c);
    static bool is_digit(wchar_t c);
    static bool is_xdigit(wchar_t c);
    static bool is_cntrl(wchar_t c);
    static bool is_print(wchar_t c);
    static bool is_asciiletter(value_type c);
    static bool is_pathsep(value_type c);
    static bool is_pathsep_unix(value_type c);
    static bool is_pathsep_win(value_type c);
    static const_pointer path_separators();
    static value_type native_path_separator();
    static const_pointer wildcards();
    static value_type replacement_char();  // unicode replacement marker

    friend std::basic_ostream<wchar_t>& operator<<(std::basic_ostream<wchar_t>& out, const wstr& str);
};

}  // namespace cix


#include "wstr.inl.h"


static_assert(std::is_convertible_v<cix::wstr::view, cix::wstr>, "no std::basic_string_view to cix::wstr conversion");
static_assert(std::is_convertible_v<cix::wstr::container, cix::wstr>, "no std::basic_string to cix::wstr conversion");

// those require "class wstr : public std::wstring" instead of  "class wstr : private std::wstring"
static_assert(std::is_convertible_v<cix::wstr, cix::wstr::view>, "no cix::wstr to std::basic_string_view conversion");
static_assert(std::is_convertible_v<cix::wstr, cix::wstr::container>, "no cix::wstr to std::basic_string conversion");

inline std::basic_ostream<wchar_t>& operator<<(std::basic_ostream<wchar_t>& out, const cix::wstr& str)
{ out << static_cast<const cix::wstr::container&>(str); return out; }

inline std::basic_ostream<char>& operator<<(std::basic_ostream<char>& out, const cix::wstr& str)
{ out << str.to_utf8(); return out; }

inline ::fmt::basic_string_view<cix::wstr::value_type> to_string_view(const cix::wstr& str)
{ return str.to_string_view(); }

#endif  // #ifdef CIX_ENABLE_WSTR
