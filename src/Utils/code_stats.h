#pragma once
// code_stats.h - header-only line counter for your own project.
//
// Usage:
//     #include "utils/code_stats.h"
//     code_stats::print();                      // scans PROJECT_ROOT/src
//     code_stats::print("C:/some/other/dir");   // or any folder
//
// Or grab the numbers yourself (e.g. to show in an ImGui tab):
//     auto t = code_stats::scan(root);  t.code, t.comments, t.files, ...
//
// Comment/string detection is a simple per-line scanner, so treat the
// numbers as very good estimates rather than compiler-exact.

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstddef>
#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace code_stats {

    namespace fs = std::filesystem;

#ifdef PROJECT_ROOT
    inline const fs::path default_root = fs::path(PROJECT_ROOT) / "src";
#else
    inline const fs::path default_root = "src";
#endif

    struct FileStats {
        fs::path path;
        std::size_t total = 0, code = 0, comments = 0, blank = 0, bytes = 0;
    };

    struct Totals {
        std::size_t files = 0, total = 0, code = 0, comments = 0, blank = 0, bytes = 0;
        std::size_t todos = 0, includes = 0, types = 0, longest_line = 0;
        std::string longest_line_file;
        std::map<std::string, std::size_t> lines_by_ext, files_by_ext;
        std::vector<FileStats> file_list;
        double seconds = 0.0;
    };

    namespace detail {

        inline bool is_source(const fs::path& p) {
            static const char* exts[] = { ".cpp", ".cc", ".cxx", ".c", ".h", ".hpp", ".hxx", ".inl", ".ipp" };
            auto e = p.extension().string();
            return std::ranges::any_of(exts, [&](const char* x) { return e == x; });
        }

        inline bool skip_dir(const std::string& n) {
            return n == ".git" || n == ".vs" || n == "out" || n == "build" || n == "_deps" ||
                n == "x64" || n.starts_with("cmake-build");
        }

        inline std::string commas(std::size_t n) {
            auto s = std::to_string(n);
            for (int i = static_cast<int>(s.size()) - 3; i > 0; i -= 3) s.insert(static_cast<std::size_t>(i), ",");
            return s;
        }

        inline double pct(std::size_t a, std::size_t b) { return b ? 100.0 * static_cast<double>(a) / static_cast<double>(b) : 0.0; }

        inline std::string bar(double percent, int width = 30) {
            int filled = static_cast<int>(percent / 100.0 * width + 0.5);
            return "[" + std::string(static_cast<std::size_t>(std::clamp(filled, 0, width)), '#') +
                std::string(static_cast<std::size_t>(width - std::clamp(filled, 0, width)), '.') + "]";
        }

        inline std::string_view trim(std::string_view s) {
            while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) s.remove_prefix(1);
            while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) s.remove_suffix(1);
            return s;
        }

        inline std::size_t count_word(std::string_view line, std::string_view word) {
            std::size_t n = 0, pos = 0;
            while ((pos = line.find(word, pos)) != std::string_view::npos) { ++n; pos += word.size(); }
            return n;
        }

    } // namespace detail

    inline Totals scan(const fs::path& root = default_root) {
        using clock = std::chrono::steady_clock;
        auto start = clock::now();
        Totals t;
        std::error_code ec;

        for (fs::recursive_directory_iterator it(root, fs::directory_options::skip_permission_denied, ec), end;
            !ec && it != end; it.increment(ec)) {
            const auto& p = it->path();
            if (it->is_directory(ec)) {
                if (detail::skip_dir(p.filename().string())) it.disable_recursion_pending();
                continue;
            }
            if (!it->is_regular_file(ec) || !detail::is_source(p)) continue;

            std::ifstream in(p);
            if (!in) continue;

            FileStats f;
            f.path = p;
            f.bytes = static_cast<std::size_t>(fs::file_size(p, ec));

            bool in_block = false;
            std::string line;
            while (std::getline(in, line)) {
                if (!line.empty() && line.back() == '\r') line.pop_back();
                ++f.total;
                if (line.size() > t.longest_line) {
                    t.longest_line = line.size();
                    t.longest_line_file = fs::relative(p, root, ec).generic_string();
                }

                bool code = false, comment = false;
                for (std::size_t i = 0; i < line.size(); ++i) {
                    char c = line[i], nx = i + 1 < line.size() ? line[i + 1] : '\0';
                    if (in_block) {
                        comment = true;
                        if (c == '*' && nx == '/') { in_block = false; ++i; }
                        continue;
                    }
                    if (std::isspace(static_cast<unsigned char>(c))) continue;
                    if (c == '/' && nx == '/') { comment = true; break; }
                    if (c == '/' && nx == '*') { comment = true; in_block = true; ++i; continue; }
                    code = true;
                    if (c == '"' || c == '\'') {                 // skip string/char literal
                        char q = c;
                        for (++i; i < line.size() && line[i] != q; ++i)
                            if (line[i] == '\\') ++i;
                    }
                }

                if (code) ++f.code;
                else if (comment) ++f.comments;
                else ++f.blank;

                auto tr = detail::trim(line);
                if (tr.starts_with("#include")) ++t.includes;
                if ((tr.starts_with("class ") || tr.starts_with("struct ")) && tr.find(';') == std::string_view::npos) ++t.types;
                t.todos += detail::count_word(line, "TODO") + detail::count_word(line, "FIXME");
            }

            auto ext = p.extension().string();
            t.files++;
            t.total += f.total; t.code += f.code; t.comments += f.comments; t.blank += f.blank; t.bytes += f.bytes;
            t.lines_by_ext[ext] += f.total;
            t.files_by_ext[ext]++;
            t.file_list.push_back(std::move(f));
        }

        t.seconds = std::chrono::duration<double>(clock::now() - start).count();
        return t;
    }

    inline void print(const Totals& t, const fs::path& root = default_root) {
        using detail::commas; using detail::pct; using detail::bar;
        std::error_code ec;
        auto line = [](std::string_view label, const std::string& v) { std::cout << std::format("  {:<16}{}\n", label, v); };

        std::cout << "\n==================== CODE STATS ====================\n";
        std::cout << "  " << root.generic_string() << "\n\n";
        if (t.files == 0) { std::cout << "  No source files found.\n\n"; return; }

        line("Files", commas(t.files));
        line("Total lines", commas(t.total));
        line("Code", std::format("{}  ({:.1f}%)", commas(t.code), pct(t.code, t.total)));
        line("Comments", std::format("{}  ({:.1f}%)", commas(t.comments), pct(t.comments, t.total)));
        line("Blank", std::format("{}  ({:.1f}%)", commas(t.blank), pct(t.blank, t.total)));
        line("Size", std::format("{:.1f} KB", static_cast<double>(t.bytes) / 1024.0));
        line("Avg lines/file", commas(t.total / t.files));
        line("#includes", commas(t.includes));
        line("Classes/structs", commas(t.types));
        line("TODO / FIXME", commas(t.todos));

        std::cout << "\n  Composition\n";
        std::cout << std::format("    code     {} {:5.1f}%\n", bar(pct(t.code, t.total)), pct(t.code, t.total));
        std::cout << std::format("    comments {} {:5.1f}%\n", bar(pct(t.comments, t.total)), pct(t.comments, t.total));
        std::cout << std::format("    blank    {} {:5.1f}%\n", bar(pct(t.blank, t.total)), pct(t.blank, t.total));

        std::cout << "\n  By file type\n";
        for (const auto& [ext, lines] : t.lines_by_ext)
            std::cout << std::format("    {:<6}{:>8} lines  {:>4} files  {}\n", ext, commas(lines),
                commas(t.files_by_ext.at(ext)), bar(pct(lines, t.total), 20));

        auto sorted = t.file_list;
        std::ranges::sort(sorted, [](const FileStats& a, const FileStats& b) { return a.total > b.total; });
        std::cout << "\n  Biggest files\n";
        for (std::size_t i = 0; i < std::min<std::size_t>(5, sorted.size()); ++i)
            std::cout << std::format("    {:>2}. {:>6} lines  {}\n", i + 1, commas(sorted[i].total),
                fs::relative(sorted[i].path, root, ec).generic_string());

        std::cout << "\n  Fun facts\n";
        std::cout << std::format("    Printed at 60 lines/page, that's ~{} pages.\n", commas(t.total / 60 + 1));
        std::cout << std::format("    Longest line: {} chars ({})\n", t.longest_line, t.longest_line_file);
        double cr = pct(t.comments, t.code);
        std::cout << "    Comment density: " << (cr < 5 ? "mysterious (the code speaks for itself, right?)"
            : cr < 20 ? "healthy"
            : "novelist")
            << "\n";
        std::cout << std::format("\n  Scanned in {:.1f} ms\n", t.seconds * 1000.0);
        std::cout << "====================================================\n\n";
    }

    inline void print(const fs::path& root = default_root) { print(scan(root), root); }

} // namespace code_stats