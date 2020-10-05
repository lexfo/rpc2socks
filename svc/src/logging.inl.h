// Copyright (c) Lexfo
// SPDX-License-Identifier: BSD-3-Clause

namespace logging {

template <
    typename String,
    typename... Args,
    typename Char>
inline void write(level_t level, const String& format, Args&&... args)
{
    typedef std::basic_string<Char> Container;
    typedef std::back_insert_iterator<Container> OutputIt;
    typedef fmt::basic_format_context<OutputIt, Char> FmtContext;

    Container msg;

    xstr::vfmt_to<String, Char, OutputIt, FmtContext>(
        std::back_inserter(msg),
        format,
        fmt::make_format_args<FmtContext>(args...));

    logging::write(level, std::move(msg));
}


#ifdef _DEBUG
template <
    typename String,
    typename... Args,
    typename Char>
void trace(
    const char* src_file, unsigned src_line,
    const String& format, Args&&... args)
{
    typedef std::basic_string<Char> Container;
    typedef std::back_insert_iterator<Container> OutputIt;
    typedef fmt::basic_format_context<OutputIt, Char> FmtContext;

    Container msg;

    xstr::vfmt_to<String, Char, OutputIt, FmtContext>(
        std::back_inserter(msg),
        format,
        fmt::make_format_args<FmtContext>(args...));

    logging::trace(src_file, src_line, std::move(msg));
}
#endif

}  // namespace logging
