#include "analyzer.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

namespace {

using Clock = std::chrono::steady_clock;

std::optional<std::filesystem::path> find_repo_root_with_opentitan() {
    auto dir = std::filesystem::current_path();
    for (int depth = 0; depth < 8; ++depth) {
        const auto candidate = dir / "tests" / "rtl" / "opentitan";
        if (std::filesystem::exists(candidate) && std::filesystem::is_directory(candidate))
            return dir;
        if (!dir.has_parent_path() || dir == dir.parent_path())
            break;
        dir = dir.parent_path();
    }
    return std::nullopt;
}

bool is_sv_like_file(const std::filesystem::path& path) {
    const auto ext = path.extension().string();
    return ext == ".sv" || ext == ".svh" || ext == ".v" || ext == ".vh";
}

std::vector<std::string> collect_opentitan_sv_files(const std::filesystem::path& repo_root) {
    std::vector<std::string> files;
    const auto root = repo_root / "tests" / "rtl" / "opentitan";

    std::error_code ec;
    std::filesystem::recursive_directory_iterator it(root, std::filesystem::directory_options::skip_permission_denied,
                                                     ec);
    const std::filesystem::recursive_directory_iterator end;
    for (; !ec && it != end; it.increment(ec)) {
        if (it->is_regular_file(ec) && is_sv_like_file(it->path()))
            files.push_back(std::filesystem::absolute(it->path()).lexically_normal().string());
    }

    std::sort(files.begin(), files.end());
    files.erase(std::unique(files.begin(), files.end()), files.end());
    return files;
}

} // namespace

TEST_CASE("benchmark: OpenTitan initial parse/index wall time", "[.benchmark][opentitan]") {
    const auto repo_root = find_repo_root_with_opentitan();
    if (!repo_root) {
        WARN("tests/rtl/opentitan is unavailable; skipping OpenTitan benchmark");
        return;
    }

    auto files = collect_opentitan_sv_files(*repo_root);
    if (files.empty()) {
        WARN("tests/rtl/opentitan contains no SystemVerilog-like files; skipping benchmark");
        return;
    }

    Analyzer analyzer;
    // Keep this benchmark focused on parse/index work.  The production default
    // debounce is useful for UI notification coalescing, but it would add a
    // fixed sleep-like component to the timing and make baseline comparisons
    // harder to interpret.
    analyzer.set_project_index_publish_debounce_ms(0);

    const auto start = Clock::now();
    analyzer.set_extra_files(files);
    analyzer.wait_for_background_index_idle();
    const auto elapsed = std::chrono::duration<double>(Clock::now() - start).count();

    const auto snapshot = analyzer.project_index_snapshot();
    const size_t indexed = snapshot ? snapshot->shards.size() : 0;

    std::cout << "\n[OpenTitan initial parse/index] files=" << files.size()
              << " indexed_shards=" << indexed << " seconds=" << elapsed << "\n";

    // This is a measurement test, not a portability-sensitive performance gate:
    // machines, filesystems, and build modes vary too much for a universal
    // threshold.  The assertion only catches a completely broken benchmark path.
    CHECK(indexed > 0);
}
