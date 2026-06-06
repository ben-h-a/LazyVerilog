#include "inlay_hints.hpp"

#include "dynamic_file_index.hpp"
#include "syntax_index.hpp"
#include "../string_utils.hpp"

#include <algorithm>
#include <optional>
#include <slang/syntax/SyntaxTree.h>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace {


using ModuleMap = std::unordered_map<std::string, ModuleEntry>;

static void overlay_modules(ModuleMap& modules, const SyntaxIndex& index) {
    for (const auto& module : index.modules)
        modules[module.name] = module;
}

static ModuleMap build_module_map(const Analyzer& analyzer) {
    ModuleMap modules;

    if (auto project_index = analyzer.project_index_snapshot()) {
        for (const auto& [name, ref] : project_index->module_by_name) {
            if (ref.shard && ref.module_index < ref.shard->modules.size())
                modules[name] = ref.shard->modules[ref.module_index];
        }
    }

    analyzer.for_each_state(
        [&](const std::string&, const std::shared_ptr<const DocumentState>& state) {
            if (state && state->tree)
                overlay_modules(modules, get_structural_index(*state));
        });

    return modules;
}

static std::unordered_map<std::string, PortEntry> build_port_map(const ModuleEntry& module) {
    std::unordered_map<std::string, PortEntry> ports;
    for (const auto& port : module.ports)
        ports[port.name] = port;
    return ports;
}

static std::string padded(std::string text, size_t width) {
    if (text.size() < width)
        text.append(width - text.size(), ' ');
    return text;
}

static std::string display_port_direction(const std::string& direction) {
    // Keep the semantic direction stored in SyntaxIndex unchanged
    // (`input`/`output`/`inout`/`unknown`) and translate only the inlay-hint
    // presentation label here.  The short bracketed labels reduce visual noise
    // at instance connections:
    //
    //   .req(req)  ◀ logic
    //   .ack(ack)  ▶ logic
    //
    // The user request only called out input and output, so `inout` remains
    // spelled out for now rather than silently choosing an abbreviation such as
    // `[IO]` that may not match user expectations.
    if (direction == "input")
        return "◀";
    if (direction == "output")
        return "▶";
    if (direction == "unknown")
        return {};
    return direction;
}

static std::string display_port_type(std::string type) {
    // Many port declarations spell out the default net/type keyword
    // explicitly (`logic` or `wire`) even when that keyword does not add much
    // value to the inlay hint.  Keep the more informative suffix — for
    // example a packed dimension — but drop the redundant leading keyword.
    //
    // Examples:
    //   "logic"           -> ""
    //   "wire"            -> ""
    //   "logic [7:0]"     -> "[7:0]"
    //   "wire [5:0]"      -> "[5:0]"
    //
    // We intentionally keep other type names intact (e.g. `bit`, `int`,
    // `struct`, user-defined types) because they convey meaningful information.
    auto strip_prefix = [&](const char* prefix) -> bool {
        constexpr size_t prefix_len = 0; // placeholder to keep lambda local-only
        (void)prefix_len;
        const size_t len = std::char_traits<char>::length(prefix);
        if (type.rfind(prefix, 0) != 0)
            return false;
        type.erase(0, len);
        while (!type.empty() && type.front() == ' ')
            type.erase(type.begin());
        return true;
    };

    strip_prefix("logic");
    strip_prefix("wire");
    return type;
}

} // namespace

std::vector<lsInlayHint> provide_inlay_hints(const Analyzer& analyzer, const std::string& uri,
                                             int range_start_line, int range_end_line) {
    auto state = analyzer.get_state(uri);
    if (!state || !state->tree)
        return {};

    const auto lines = split_lines_view(state->text);
    const auto current_index = get_structural_index(*state);
    const auto modules = build_module_map(analyzer);
    std::vector<lsInlayHint> hints;

    for (const auto& inst : current_index.instances) {
        auto module_it = modules.find(inst.module_name);
        if (module_it == modules.end())
            continue;

        const auto port_map = build_port_map(module_it->second);
        if (port_map.empty())
            continue;

        const int lo = std::max(inst.start_line, range_start_line);
        const int hi = std::min(inst.end_line + 1, range_end_line + 1);

        struct Candidate {
            int line{0};
            int col{0};
            std::string direction;
            std::string type;
        };
        std::vector<Candidate> candidates;
        std::unordered_set<std::string> connected;

        for (const auto& conn : inst.connections) {
            const int line = conn.line > 0 ? conn.line - 1 : 0;
            connected.insert(conn.port_name);

            if (line < lo || line >= hi)
                continue;

            auto port_it = port_map.find(conn.port_name);
            if (port_it == port_map.end())
                continue;

            candidates.push_back(Candidate{
                .line = line,
                // Place the inlay hint immediately after the named port token,
                // not inside the connection parentheses.
                //
                // Slang gives us both:
                //   conn.col      -> the first character of the port name
                //                    in `.port_name(...)`
                //   conn.hint_col -> the first character of the connected
                //                    expression inside the parentheses
                //
                // The old inlay position used conn.hint_col, which rendered
                // the direction/type hint here:
                //
                //   .i_clk              (|hint ...)
                //
                // For aligned instance lists that is visually noisy because
                // the hint appears after a long run of alignment spaces.  For
                // inlay presentation, the useful anchor is the port itself:
                //
                //   .i_clk |hint        ( ... )
                //
                // Keep conn.hint_col unchanged for connection-edit features
                // that still need to locate/replace the expression text.
                .col = conn.col + static_cast<int>(conn.port_name.size()) + 1,
                .direction = display_port_direction(port_it->second.direction),
                .type = display_port_type(port_it->second.type),
            });
        }

        if (candidates.empty() && connected.empty())
            continue;

        if (inst.start_line >= range_start_line && inst.start_line <= range_end_line &&
            inst.start_line < (int)lines.size()) {
            size_t connected_count = 0;
            for (const auto& name : connected) {
                if (port_map.contains(name))
                    ++connected_count;
            }

            lsInlayHint coverage;
            coverage.position = lsPosition(inst.start_line, (int)lines[inst.start_line].size());
            coverage.label =
                std::to_string(connected_count) + "/" + std::to_string(port_map.size()) + " ports";
            coverage.kind = optional<lsInlayHintKind>(lsInlayHintKind::Parameter);
            coverage.paddingLeft = optional<bool>(true);
            coverage.paddingRight = optional<bool>(false);
            hints.push_back(std::move(coverage));
        }

        if (candidates.empty())
            continue;

        size_t max_dir = 0;
        size_t max_type = 0;
        for (const auto& candidate : candidates) {
            max_dir = std::max(max_dir, candidate.direction.size());
            max_type = std::max(max_type, candidate.type.size());
        }

        struct Label {
            int line{0};
            int col{0};
            std::string text;
        };
        std::vector<Label> labels;
        size_t max_label = 0;
        for (const auto& candidate : candidates) {
            std::vector<std::string> parts;
            auto dir = padded(candidate.direction, max_dir);
            if (dir.find_first_not_of(' ') != std::string::npos)
                parts.push_back(std::move(dir));

            if (max_type > 0) {
                auto type = padded(candidate.type, max_type);
                if (type.find_first_not_of(' ') != std::string::npos)
                    parts.push_back(std::move(type));
            }

            std::string label;
            for (size_t i = 0; i < parts.size(); ++i) {
                if (i > 0)
                    label += ' ';
                label += parts[i];
            }

            if (label.find_first_not_of(' ') == std::string::npos)
                continue;

            max_label = std::max(max_label, label.size());
            labels.push_back(Label{
                .line = candidate.line,
                .col = candidate.col,
                .text = std::move(label),
            });
        }

        for (auto& label : labels) {
            lsInlayHint hint;
            hint.position = lsPosition(label.line, label.col);
            hint.label = padded(std::move(label.text), max_label);
            hint.kind = optional<lsInlayHintKind>(lsInlayHintKind::Type);
            hint.paddingLeft = optional<bool>(false);
            hint.paddingRight = optional<bool>(true);
            hints.push_back(std::move(hint));
        }
    }

    return hints;
}
