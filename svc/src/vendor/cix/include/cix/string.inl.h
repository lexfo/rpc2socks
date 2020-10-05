// CIX C++ library
// Copyright (c) Jean-Charles Lefebvre
// SPDX-License-Identifier: MIT

namespace cix {
namespace string {

namespace detail
{
#ifdef _WIN32
    inline std::wstring widen(const char* src, size_type len, bool strict)
    {
        if (!src || !*src || !len)
            return std::wstring();

        const UINT src_cp = CP_UTF8;
        const DWORD flags = strict ? MB_ERR_INVALID_CHARS : 0;
        int ilen;

        // deal with unlikely overflow - MultiByteToWideChar expects an int
        if (len <= static_cast<size_type>(std::numeric_limits<int>::max()))
            ilen = static_cast<int>(len);
        else
            ilen = static_cast<size_type>(std::numeric_limits<int>::max());

        // get required size
        int count = MultiByteToWideChar(src_cp, flags, src, ilen, 0, 0);
        if (count <= 0)
        {
            assert(0);
            return std::wstring();
        }

        // convert
        std::wstring dest(static_cast<size_type>(count), 0);
        MultiByteToWideChar(src_cp, flags, src, ilen, dest.data(), count);
        while (dest.back() == 0)
            dest.erase(dest.size() - 1);

        return dest;
    }
#endif  // #ifdef _WIN32


#ifdef _WIN32
    inline std::string narrow(const wchar_t* src, size_type len, bool strict)
    {
        if (!src || !*src || !len)
            return std::string();

        const UINT dest_cp = CP_UTF8;
        DWORD flags = 0;
        int ilen;

        // WC_ERR_INVALID_CHARS flag is only supported on Vista or later
#if (WINVER >= 0x0600) || defined(WC_ERR_INVALID_CHARS)
        flags |= strict ? WC_ERR_INVALID_CHARS : 0;
#else
        CIX_UNVAR(strict);
#endif

        // deal with unlikely overflow - WideCharToMultiByte expects an int
        if (len <= static_cast<size_type>(std::numeric_limits<int>::max()))
            ilen = static_cast<int>(len);
        else
            ilen = static_cast<size_type>(std::numeric_limits<int>::max());

        // get required size
        int count = WideCharToMultiByte(dest_cp, flags, src, ilen, 0, 0, 0, 0);
        if (count <= 0)
        {
            assert(0);
            return std::string();
        }

        // convert
        std::string dest(static_cast<size_type>(count), 0);
        WideCharToMultiByte(
            dest_cp, flags, src, ilen, dest.data(), count, 0, 0);
        while (dest.back() == 0)
            dest.erase(dest.size() - 1);

        return dest;
    }
#endif  // #ifdef _WIN32


    // this fmt::arg_formatter forcefully casts signed integers to unsigned when
    // the 'x' or 'X' type specifier is used
    template <typename RangeT>
    class fmt_custom_arg_formatter : public ::fmt::arg_formatter<RangeT>
    {
    public:
        using base = ::fmt::arg_formatter<RangeT>;
        using base::range;
        using base::iterator;
        using base::format_specs;

    private:
        using char_type = typename range::value_type;
        using context_type = ::fmt::basic_format_context<iterator, char_type>;

    public:
        explicit fmt_custom_arg_formatter(
                context_type& ctx,
                ::fmt::basic_format_parse_context<char_type>* parse_ctx=nullptr,
                format_specs* spec=nullptr)
            : base(ctx, parse_ctx, spec)
            { }

        using base::operator();

        inline iterator operator()(std::int8_t value)
            { return this->_handle_signed_int(value); }

        inline iterator operator()(std::int16_t value)
            { return this->_handle_signed_int(value); }

        inline iterator operator()(std::int32_t value)
            { return this->_handle_signed_int(value); }

        inline iterator operator()(std::int64_t value)
            { return this->_handle_signed_int(value); }

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


    template <typename Char>
    inline std::basic_string<Char>
    join_stub_final(
        std::size_t final_length,
        const std::vector<std::basic_string_view<Char>>& glued_views) noexcept
    {
        std::basic_string<Char> out;

        out.reserve(final_length);
        for (const auto& view : glued_views)
            std::copy(view.begin(), view.end(), std::back_inserter(out));

        return out;
    }

    template <typename Char>
    inline void
    join_stub(
        std::size_t& out_length,
        std::vector<std::basic_string_view<Char>>& out_views,
        bool keep_empty,
        bool strip_glue,
        const std::basic_string_view<Char>& glue) noexcept
    {
        CIX_UNVAR(out_length);
        CIX_UNVAR(out_views);
        CIX_UNVAR(keep_empty);
        CIX_UNVAR(strip_glue);
        CIX_UNVAR(glue);
    }

    template <
        typename String,
        typename... Args,
        typename Char = char_t<String>,
        typename std::enable_if_t<is_string_viewable_v<String>, int> = 0>
    inline void
    join_stub(
        std::size_t& out_length,
        std::vector<std::basic_string_view<Char>>& out_views,
        bool keep_empty,
        bool strip_glue,
        const std::basic_string_view<Char>& glue,
        const String& next_,
        Args&&... args) noexcept
    {
        auto next = to_string_view(next_);

        if (strip_glue && !glue.empty())
        {
#if 0  // C++20 :(
            while (next.starts_with(glue))
                next.remove_prefix(glue.size());

            while (next.ends_with(glue))
                next.remove_suffix(glue.size());
#else
            const auto glue_size = glue.size();

            // strip prefix
            while (
                next.size() >= glue_size &&
                std::equal(
                    next.begin(),
                    std::next(next.begin(), glue_size),
                    glue.begin()))
            {
                next.remove_prefix(glue_size);
            }

            // strip suffix
            while (
                next.size() >= glue_size &&
                std::equal(
                    // this instead of (end - glue_size), so that we stick to
                    // ForwardIt
                    std::next(next.begin(), next.size() - glue_size),
                    next.end(),
                    glue.begin()))
            {
                next.remove_suffix(glue_size);
            }
#endif
        }

        if (keep_empty || !next.empty())
        {
            if (!out_views.empty())
            {
                out_views.push_back(glue);
                out_length += glue.size();
            }

            out_views.push_back(next);
            out_length += next.size();
        }

        join_stub(out_length, out_views, keep_empty, strip_glue, glue, args...);
    }

}  // namespace detail


template <typename Char>
inline constexpr bool char_is(Char c, ctype_mask masks)
{
    const auto& facet = std::use_facet<std::ctype<Char>>(std::locale::classic());
    return facet.is(masks, c);
}


template <typename Char>
inline constexpr bool isspace(Char c)
{
    return char_is(c, ctype_space);
}


template <
    typename Char,
    typename std::enable_if_t<is_char_v<Char>, int>>
inline size_type
strnlen_s(const Char* s, size_type max_length)
{
    const std::basic_string_view<Char> view{s, max_length};
    const size_type pos = view.find(Char(0));

    if (pos == view.npos)
    {
        assert(0);
        return max_length;
    }
    else
    {
        return pos;
    }
}


template <typename Char, size_type Count>
inline std::basic_string_view<Char>
to_string_view(const std::array<Char, Count>& s)
{
    return std::basic_string_view<Char>(
        s.data(),
        std::min(s.size(), std::char_traits<Char>::length(s.data())));
}


template <
    typename Char,
    typename Alloc,
    typename std::enable_if_t<is_char_v<Char>, int>>
inline std::basic_string_view<Char>
to_string_view(
    const std::vector<Char, Alloc>& s)
{
    return std::basic_string_view<Char>(
        s.data(),
        std::min(s.size(), std::char_traits<Char>::length(s.data())));
}


template <
    typename String,
    typename Char,
    typename std::enable_if_t<is_string_viewable_v<String>, int>>
inline std::basic_string<Char>
to_string(const String& s)
{
    const auto view = to_string_view(s);
    return std::basic_string<Char>(view.begin(), view.end());
}


template <typename Container>
typename Container::value_type*
prepare(
    Container& container,
    size_type required_len,
    size_type offset)
{
    container.resize(offset + required_len);
    return container.data();  // return &container[offset];
}


template <typename Container>
Container&
finalize(
    Container& container,
    size_type length,
    size_type offset)
{
    assert(length == npos || (offset + length) <= container.size());

    if (length == npos)
    {
        typename Container::value_type* p = &container[0];

        p += offset;
        length = offset;
        for (; length < container.size() && *p++; ++length) { ; }

        assert(
            (length < container.size() && !container[length]) ||
            length == container.size());
    }
    else
    {
        length += offset;
    }
    assert(length <= container.size());

    if (length < container.size())
    {
        container.erase(
            std::next(container.begin(), length),
            container.end());
    }

    return container;
}


#ifdef _WIN32
template <
    typename String,
    typename Char,
    typename std::enable_if_t<
        is_string_viewable_v<String> &&
        std::is_same_v<char, Char>, int>>
inline std::wstring widen_utf8_lenient(const String& input)
{
    const auto view = to_string_view(input);
    return detail::widen(view.data(), view.length(), false);
}
#endif


#ifdef _WIN32
template <
    typename String,
    typename Char,
    typename std::enable_if_t<
        is_string_viewable_v<String> &&
        std::is_same_v<char, Char>, int>>
inline std::wstring widen_utf8_strict(const String& input)
{
    const auto view = to_string_view(input);
    return detail::widen(view.data(), view.length(), true);
}
#endif


#ifdef _WIN32
template <
    typename String,
    typename Char,
    typename std::enable_if_t<
        is_string_viewable_v<String> &&
        std::is_same_v<wchar_t, Char>, int>>
inline std::string narrow_to_utf8_lenient(const String& input)
{
    const auto view = to_string_view(input);
    return detail::narrow(view.data(), view.length(), false);
}
#endif


#ifdef _WIN32
template <
    typename String,
    typename Char,
    typename std::enable_if_t<
        is_string_viewable_v<String> &&
        std::is_same_v<wchar_t, Char>, int>>
inline std::string narrow_to_utf8_strict(const String& input)
{
    const auto view = to_string_view(input);
    return detail::narrow(view.data(), view.length(), true);
}
#endif


template <
    typename StringA,
    typename StringB,
    typename Char,
    typename std::enable_if_t<
        is_string_viewable_v<StringA> &&
        is_string_viewable_v<StringB> &&
        std::is_same_v<char_t<StringA>, char_t<StringB>>, int>>
inline constexpr std::vector<std::basic_string_view<Char>>
split_one_of(
        const StringA& input,
        const StringB& seps,
        size_type max_split)
{
    const auto inputv = to_string_view(input);
    const auto sepsv = to_string_view(seps);
    std::vector<std::basic_string_view<Char>> out;
    auto base = inputv.begin();
    decltype(base) it;

    while (base != inputv.end())
    {
        if (max_split > 0 && out.size() >= max_split)
        {
            it = inputv.end();
        }
        else
        {
            it = std::find_first_of(
                base, inputv.end(),
                sepsv.begin(), sepsv.end());
        }

        out.push_back(decltype(out)::value_type(&*base, it - base));

        if (it == inputv.end())
            break;

        base = std::next(it);
    }

    return out;
}


template <
    typename String,
    typename UnaryPredicate,
    typename Char,
    typename std::enable_if_t<is_string_viewable_v<String>, int>>
inline constexpr std::vector<std::basic_string_view<Char>>
split_if(
    const String& input,
    UnaryPredicate pred,
    size_type max_split)
{
    const auto inputv = to_string_view(input);
    const auto sepsv = to_string_view(seps);
    std::vector<std::basic_string_view<Char>> out;
    auto base = inputv.begin();
    decltype(base) it;

    for (;;)
    {
        if (max_split > 0 && out.size() >= max_split)
            it = inputv.end();
        else
            it = std::find_if(base, inputv.end(), pred);

        out.push_back(decltype(out)::value_type(&*base, it - base));

        if (it == inputv.end())
            break;

        base = std::next(it);
    }

    return out;
}


template <
    typename String,
    typename Char,
    typename std::enable_if_t<is_string_viewable_v<String>, int>>
inline std::vector<std::basic_string_view<Char>>
split(
    const String& input,
    size_type max_split)
{
    auto pred = [](Char c) { return isspace(c); }
    return split_if(input, pred, max_split);
}


template <
    typename StringA,
    typename StringB,
    typename... Args,
    typename Char>
inline std::enable_if_t<
        is_string_viewable_v<StringA> &&
        is_string_viewable_v<StringB> &&
        std::is_same_v<char_t<StringA>, char_t<StringB>>,
    std::basic_string<Char>>
join(
    const StringA& glue_,
    const StringB& next,
    Args&&... args) noexcept
{
    std::vector<std::basic_string_view<Char>> glued_views;
    std::size_t final_strlen = 0;
    const auto glue = to_string_view(glue_);

    glued_views.reserve(2 * (1 + sizeof...(args)));  // +1 for glue and next

    detail::join_stub(
        final_strlen, glued_views, true, false, glue, next, args...);

    return detail::join_stub_final(final_strlen, glued_views);
}


template <
    typename String,
    typename Container,
    typename CharA,
    typename CharB>
inline std::enable_if_t<
        is_string_viewable_v<String> &&
        is_container_of_strings_v<Container> &&
        std::is_same_v<CharA, CharB>,
    std::basic_string<CharA>>
join(
    const String& glue_,
    const Container& elements) noexcept
{
    if (elements.empty())
        return {};

    std::vector<std::basic_string_view<CharA>> glued_views;
    std::size_t final_strlen = 0;
    const auto glue = to_string_view(glue_);

    glued_views.reserve(2 * elements.size());

    for (const auto& elem : elements)
        detail::join_stub(final_strlen, glued_views, true, false, glue, elem);

    return detail::join_stub_final(final_strlen, glued_views);
}


template <
    typename StringA,
    typename StringB,
    typename... Args,
    typename Char>
inline std::enable_if_t<
        is_string_viewable_v<StringA> &&
        is_string_viewable_v<StringB> &&
        std::is_same_v<char_t<StringA>, char_t<StringB>>,
    std::basic_string<Char>>
melt(
    const StringA& glue_,
    const StringB& next,
    Args&&... args) noexcept
{
    std::vector<std::basic_string_view<Char>> glued_views;
    std::size_t final_strlen = 0;
    const auto glue = to_string_view(glue_);

    glued_views.reserve(2 * (1 + sizeof...(args)));  // +1 for glue and next

    detail::join_stub(
        final_strlen, glued_views, false, false, glue, next, args...);

    return detail::join_stub_final(final_strlen, glued_views);
}


template <
    typename String,
    typename Container,
    typename CharA,
    typename CharB>
inline std::enable_if_t<
        is_string_viewable_v<String> &&
        is_container_of_strings_v<Container> &&
        std::is_same_v<CharA, CharB>,
    std::basic_string<CharA>>
melt(
    const String& glue_,
    const Container& elements) noexcept
{
    if (elements.empty())
        return {};

    std::vector<std::basic_string_view<CharA>> glued_views;
    std::size_t final_strlen = 0;
    const auto glue = to_string_view(glue_);

    glued_views.reserve(2 * elements.size());

    for (const auto& elem : elements)
        detail::join_stub(final_strlen, glued_views, false, false, glue, elem);

    return detail::join_stub_final(final_strlen, glued_views);
}


template <
    typename StringA,
    typename StringB,
    typename... Args,
    typename Char>
inline std::enable_if_t<
        is_string_viewable_v<StringA> &&
        is_string_viewable_v<StringB> &&
        std::is_same_v<char_t<StringA>, char_t<StringB>>,
    std::basic_string<Char>>
melt_stripped(
    const StringA& glue_,
    const StringB& next,
    Args&&... args) noexcept
{
    std::vector<std::basic_string_view<Char>> glued_views;
    std::size_t final_strlen = 0;
    const auto glue = to_string_view(glue_);

    glued_views.reserve(2 * (1 + sizeof...(args)));  // +1 for glue and next

    detail::join_stub(
        final_strlen, glued_views, false, true, glue, next, args...);

    return detail::join_stub_final(final_strlen, glued_views);
}


template <
    typename String,
    typename Container,
    typename CharA,
    typename CharB>
inline std::enable_if_t<
        is_string_viewable_v<String> &&
        is_container_of_strings_v<Container> &&
        std::is_same_v<CharA, CharB>,
    std::basic_string<CharA>>
melt_stripped(
    const String& glue_,
    const Container& elements) noexcept
{
    if (elements.empty())
        return {};

    std::vector<std::basic_string_view<CharA>> glued_views;
    std::size_t final_strlen = 0;
    const auto glue = to_string_view(glue_);

    glued_views.reserve(2 * elements.size());

    for (const auto& elem : elements)
        detail::join_stub(final_strlen, glued_views, false, true, glue, elem);

    return detail::join_stub_final(final_strlen, glued_views);
}


template <
    typename String,
    typename UnaryPredicate,
    typename Char,
    typename std::enable_if_t<is_string_viewable_v<String>, int>>
inline constexpr std::basic_string_view<Char>
ltrim_if(
    const String& input,
    UnaryPredicate to_remove)
{
    const auto inputv = to_string_view(input);
    const auto it = std::find_if_not(inputv.begin(), inputv.end(), to_remove);

    if (it == inputv.end())
        return {};

    return inputv.substr(std::distance(inputv.begin(), it));
}


template <
    typename String,
    typename UnaryPredicate,
    typename Char,
    typename std::enable_if_t<is_string_viewable_v<String>, int>>
inline constexpr std::basic_string_view<Char>
rtrim_if(
    const String& input,
    UnaryPredicate to_remove)
{
    const auto inputv = to_string_view(input);
    const auto rit =
        std::find_if_not(inputv.rbegin(), inputv.rend(), to_remove);

    if (rit == inputv.rend())
        return {};

    return inputv.substr(0, std::distance(inputv.begin(), rit.base()));
}


template <
    typename String,
    typename UnaryPredicate,
    typename Char,
    typename std::enable_if_t<is_string_viewable_v<String>, int>>
inline constexpr std::basic_string_view<Char>
trim_if(
    const String& input,
    UnaryPredicate to_remove)
{
    const auto tail = ltrim_if(input, to_remove);
    return rtrim_if(tail, to_remove);
}


template <
    typename StringA,
    typename StringB,
    typename StringC,
    typename Char,
    typename std::enable_if_t<
        is_string_viewable_v<StringA> &&
        is_string_viewable_v<StringB> &&
        is_string_viewable_v<StringC> &&
        std::is_same_v<char_t<StringA>, char_t<StringB>> &&
        std::is_same_v<char_t<StringA>, char_t<StringC>>, int>>
std::basic_string<Char>
replace_all(
    const StringA& input_,
    const StringB& from_,
    const StringC& to_)
{
    const auto input = to_string_view(input_);
    const auto from = to_string_view(from_);
    const auto to = to_string_view(to_);

    std::basic_string<Char> out;

    if (input.empty())
        return out;

    out.reserve(input.size());

    decltype(input)::size_type start = 0;
    auto out_it = std::back_inserter(out);

    do
    {
        auto pos = input.find(from.data(), start, from.size());

        if (pos != start)
        {
            const auto pos_it =
                (pos == input.npos) ? input.end() :
                std::next(input.begin(), pos);

            std::copy(std::next(input.begin(), start), pos_it, out_it);
        }

        std::copy(to.begin(), to.end(), out_it);

        if (pos == input.npos)
            break;

        start = pos + from.size();
    }
    while (start <= input.size());

    return out;
}


template <
    typename StringA,
    typename StringB,
    typename StringC,
    typename Char,
    typename std::enable_if_t<
        is_string_viewable_v<StringA> &&
        is_string_viewable_v<StringB> &&
        is_string_viewable_v<StringC> &&
        std::is_same_v<char_t<StringA>, char_t<StringB>> &&
        std::is_same_v<char_t<StringA>, char_t<StringC>>, int>>
std::basic_string<Char>
replace_all_of(
    const StringA& input_,
    const StringB& from_any_,
    const StringC& to_)
{
    const auto input = to_string_view(input_);
    const auto from_any = to_string_view(from_any_);
    const auto to = to_string_view(to_);

    std::basic_string<Char> out;

    if (input.empty())
        return out;

    out.reserve(input.size());

    decltype(input)::size_type start = 0;
    auto out_it = std::back_inserter(out);

    do
    {
        auto pos = input.find_first_of(from_any, start);

        if (pos != start)
        {
            const auto pos_it =
                (pos == input.npos) ? input.end() :
                std::next(input.begin(), pos);

            std::copy(std::next(input.begin(), start), pos_it, out_it);
        }

        std::copy(to.begin(), to.end(), out_it);

        if (pos == input.npos)
            break;

        start = pos + 1;
    }
    while (start <= input.size());

    return out;
}


template <
    typename String,
    typename... Args,
    typename Char,
    typename Container>
inline Container
fmt(const String& format, Args&&... args)
{
    typedef std::back_insert_iterator<Container> OutputIt;
    typedef ::fmt::basic_format_context<OutputIt, Char> FmtContext;

    Container dest;

    vfmt_to<String, Char, OutputIt, FmtContext>(
        std::back_inserter(dest),
        format,
        ::fmt::make_format_args<FmtContext>(args...));

    return dest;
}


template <
    typename Container,
    typename String,
    typename... Args,
    typename Char>
inline std::back_insert_iterator<Container>
fmt_to(
    Container& dest, const String& format, Args&&... args)
{
    typedef std::back_insert_iterator<Container> OutputIt;
    typedef ::fmt::basic_format_context<OutputIt, Char> FmtContext;

    return vfmt_to<String, Char, OutputIt, FmtContext>(
        std::back_inserter(dest),
        format,
        ::fmt::make_format_args<FmtContext>(args...));
}


template <
    typename Container,
    typename String,
    typename... Args,
    typename Char>
inline std::back_insert_iterator<Container>
fmt_to(
    std::back_insert_iterator<Container> out,
    const String& format,
    Args&&... args)
{
    typedef std::back_insert_iterator<Container> OutputIt;
    typedef ::fmt::basic_format_context<OutputIt, Char> FmtContext;

    return vfmt_to<String, Char, OutputIt, FmtContext>(
        out, format, ::fmt::make_format_args<FmtContext>(args...));
}


template <
    typename String,
    typename Char,
    typename OutputIt,
    typename FmtContext>
inline OutputIt
vfmt_to(
    OutputIt out,
    const String& format,
    ::fmt::basic_format_args<FmtContext> args)
{
    typedef ::fmt::internal::output_range<OutputIt, Char> Range;

    return ::fmt::vformat_to<
        detail::fmt_custom_arg_formatter<Range>, Char, FmtContext>(
            Range(out), format, args);  // ::fmt::to_string_view(format)
}


template <typename Char>
inline std::enable_if_t<is_char_v<Char>, Char>
to_lower(Char c)
{
    return std::use_facet<std::ctype<Char>>(std::locale::classic()).tolower(c);
}


template <typename Char>
inline std::enable_if_t<is_char_v<Char>, Char>
to_upper(Char c)
{
    return std::use_facet<std::ctype<Char>>(std::locale::classic()).toupper(c);
}


template <typename String, typename Char>
std::enable_if_t<
    is_string_viewable_v<String>,
    std::basic_string<Char>>
to_lower(const String& input_)
{
    const auto input = to_string_view(input_);
    if (input.empty())
        return {};

    std::basic_string<Char> output(input.begin(), input.end());

    std::use_facet<std::ctype<Char>>(std::locale::classic()).tolower(
        &output.front(),
        &output.back() + 1);

    return output;
}


template <typename String, typename Char>
std::enable_if_t<
    is_string_viewable_v<String>,
    std::basic_string<Char>>
to_upper(const String& input_)
{
    const auto input = to_string_view(input_);
    if (input.empty())
        return {};

    std::basic_string<Char> output(input.begin(), input.end());

    std::use_facet<std::ctype<Char>>(std::locale::classic()).toupper(
        &output.front(),
        &output.back() + 1);

    return output;
}

}  // namespace string
}  // namespace cix
