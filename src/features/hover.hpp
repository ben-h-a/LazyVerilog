#pragma once
#include "LibLsp/lsp/textDocument/hover.h"
#include "LibLsp/lsp/lsTextDocumentPositionParams.h"
#include "LibLsp/JsonRpc/serializer.h"
#include "analyzer.hpp"

std::optional<TextDocumentHover::Result> provide_hover(const Analyzer& analyzer,
                                                       const lsTextDocumentPositionParams& params);

// textDocument/hover returns Hover | null. lspcpp's response wrapper stores a
// non-optional Result, so an empty/default Result would otherwise serialize as
// {"contents":null}. Neovim 0.11 treats that nested JSON null as vim.NIL
// (truthy), bypasses its nil guard, and crashes. Emit null at the hover-result
// level when there is no content so clients receive the LSP-correct shape.
void Reflect(Writer& visitor, TextDocumentHover::Result& value);
