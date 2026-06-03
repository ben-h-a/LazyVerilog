#pragma once

#include "LibLsp/lsp/textDocument/foldingRange.h"
#include "analyzer.hpp"
#include <vector>

std::vector<FoldingRange> provide_folding_range(const Analyzer& analyzer,
                                                const FoldingRangeRequestParams& params);
