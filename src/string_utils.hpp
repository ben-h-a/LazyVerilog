#pragma once
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

inline std::string trim_copy(std::string text) {
    auto first = std::find_if_not(text.begin(), text.end(),
                                   [](unsigned char c) { return std::isspace(c); });
    auto last = std::find_if_not(text.rbegin(), text.rend(),
                                  [](unsigned char c) { return std::isspace(c); }).base();
    if (first >= last)
        return {};
    return std::string(first, last);
}

/// Count UTF-16 code units from byte offset `pos` until a newline or end of
/// string.  LSP positions use UTF-16 columns, while lazyverilog stores document
/// text as UTF-8, so this helper advances over one UTF-8 scalar at a time and
/// counts supplementary-plane scalars as two UTF-16 units.  Malformed UTF-8 is
/// deliberately treated as one byte / one UTF-16 unit to keep offset math
/// monotonic for partially-edited documents.
inline size_t utf16_units_until_newline(std::string_view text, size_t pos) {
    size_t units = 0;
    while (pos < text.size() && text[pos] != '\n') {
        unsigned char c = static_cast<unsigned char>(text[pos]);
        int bytes = 1;
        int width = 1;
        if (c < 0x80) {
            bytes = 1;
        } else if ((c & 0xE0) == 0xC0) {
            bytes = 2;
        } else if ((c & 0xF0) == 0xE0) {
            bytes = 3;
        } else if ((c & 0xF8) == 0xF0) {
            bytes = 4;
            width = 2;
        }

        bool valid_sequence = pos + static_cast<size_t>(bytes) <= text.size();
        for (int i = 1; valid_sequence && i < bytes; ++i) {
            unsigned char cc = static_cast<unsigned char>(text[pos + static_cast<size_t>(i)]);
            valid_sequence = (cc & 0xC0) == 0x80;
        }
        if (!valid_sequence) {
            bytes = 1;
            width = 1;
        }

        units += static_cast<size_t>(width);
        pos += static_cast<size_t>(bytes);
    }
    return units;
}


inline std::optional<std::string> read_file_text_optional(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in)
        return std::nullopt;

    // Prefer a single allocation/read for regular files.  The fallback keeps the
    // helper robust for paths where the size cannot be queried or the stream is
    // not seekable.  Large RTL sources are common, so avoiding repeated string
    // growth keeps project/background parsing from wasting allocator work.
    std::error_code ec;
    const auto size = std::filesystem::file_size(path, ec);
    if (!ec) {
        std::string text(size, '\0');
        if (size == 0)
            return text;
        in.read(text.data(), static_cast<std::streamsize>(text.size()));
        text.resize(static_cast<size_t>(in.gcount()));
        return text;
    }

    std::string text;
    in.seekg(0, std::ios::end);
    if (const auto end = in.tellg(); end > 0)
        text.reserve(static_cast<size_t>(end));
    in.seekg(0, std::ios::beg);
    text.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    return text;
}

inline std::string read_file_text_or_empty(const std::filesystem::path& path) {
    return read_file_text_optional(path).value_or(std::string{});
}

inline std::filesystem::path normalize_filesystem_path(const std::filesystem::path& path) {
    std::error_code ec;
    auto absolute = std::filesystem::absolute(path, ec);
    if (ec)
        absolute = path;
    return absolute.lexically_normal();
}

inline std::string uri_from_path(const std::filesystem::path& path) {
    return "file://" + normalize_filesystem_path(path).string();
}

/// Split source text into logical lines without copying.  The returned
/// string_views refer to the input buffer, so callers must keep the source text
/// alive for as long as they use the views.  The behavior intentionally matches
/// the formatter/index helpers this replaces: a trailing newline produces a
/// final empty line, and an empty input produces one empty line.
inline std::vector<std::string_view> split_lines_view(std::string_view text) {
    std::vector<std::string_view> lines;
    size_t start = 0;
    while (start <= text.size()) {
        const size_t end = text.find('\n', start);
        if (end == std::string_view::npos) {
            lines.push_back(text.substr(start));
            break;
        }
        lines.push_back(text.substr(start, end - start));
        start = end + 1;
    }
    if (lines.empty())
        lines.push_back({});
    return lines;
}

/// Owning-string convenience wrapper for callers that edit lines in place or
/// store them independently of the original source buffer.
inline std::vector<std::string> split_lines_owned(std::string_view text) {
    const auto views = split_lines_view(text);
    std::vector<std::string> lines;
    lines.reserve(views.size());
    for (std::string_view line : views)
        lines.emplace_back(line);
    return lines;
}
