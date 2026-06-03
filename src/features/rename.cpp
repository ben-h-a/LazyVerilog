#include "rename.hpp"
#include <algorithm>
#include <array>

std::optional<PrepareRenameResult> prepare_rename(const Analyzer& analyzer,
                                                  const lsTextDocumentPositionParams& params) {
    auto ident = analyzer.identifier_at(params.textDocument.uri.raw_uri_, params.position.line,
                                        params.position.character);
    if (!ident || ident->name.empty())
        return std::nullopt;

    PrepareRenameResult result;
    result.range.start = lsPosition(ident->line, ident->col);
    result.range.end = lsPosition(ident->line, ident->end_col);
    result.placeholder = ident->name;
    return result;
}

lsWorkspaceEdit provide_rename(const Analyzer& analyzer, const TextDocumentRename::Params& params) {
    lsWorkspaceEdit workspace_edit;
    auto ident = analyzer.identifier_at(params.textDocument.uri.raw_uri_, params.position.line,
                                        params.position.character);
    if (!ident || ident->name.empty())
        return workspace_edit;

    auto refs = analyzer.find_references(params.textDocument.uri.raw_uri_, params.position.line,
                                         params.position.character, true);
    std::map<std::string, std::vector<lsTextEdit>> changes;
    for (const auto& ref : refs) {
        lsTextEdit edit;
        edit.range.start = lsPosition(ref.line, ref.col);
        edit.range.end = lsPosition(ref.end_line, ref.end_col);
        edit.newText = params.newName;
        changes[ref.uri].push_back(std::move(edit));
    }
    if (!changes.empty())
        workspace_edit.changes = std::move(changes);
    return workspace_edit;
}
