#pragma once
#include "../document_state.hpp"
#include "../syntax_index.hpp"
#include "../config.hpp"
#include <string>
#include <vector>

std::string autowire_apply(
    const DocumentState& state, const SyntaxIndex* opened_index,
    const ProjectIndexSnapshot* project_index,
    const AutowireOptions& options, int target_line = -1);

std::vector<std::string> autowire_preview(
    const DocumentState& state, const SyntaxIndex* opened_index,
    const ProjectIndexSnapshot* project_index,
    const AutowireOptions& options, int target_line = -1);

// Convenience overloads for tests and single-file callers.  They keep the
// common "current file only" case compact without requiring a dummy project
// snapshot.
std::string autowire_apply(
    const DocumentState& state, const SyntaxIndex& opened_index,
    const AutowireOptions& options, int target_line = -1);

std::vector<std::string> autowire_preview(
    const DocumentState& state, const SyntaxIndex& opened_index,
    const AutowireOptions& options, int target_line = -1);
