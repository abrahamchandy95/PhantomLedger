# cmake/IncludeLayerLint.cmake
#
# L2 include-layer lint, in the source-list-audit style: every project
# file is mapped to a layer by its path, every `#include
# "phantomledger/..."` edge is checked against the allowed layering
# DAG, and an edge that points the wrong way is reported at configure
# time. The DAG below is the L1 job-library graph (root
# CMakeLists.txt) plus the four header-only vocabulary trees.
#
# FATAL BY DEFAULT (L2 close-out, 2026-07-19): the full-tree report
# reached clean after untangling rounds A/B/C, so an illegal edge now
# FAILS the configure — the layering is law. Configure with
# -DPL_LAYER_LINT_FATAL=OFF as a temporary escape hatch on exploratory
# refactor branches; never merge with it off. The DAG itself changes
# only by deliberate amendment in a named commit.
#
# FINDINGS LEDGER (full-tree scan 2026-07-19, this DAG):
#   AMENDED (legal by design):
#     activity -> transactions   ~40 edges: the spending simulator
#                                consumes the transaction vocabulary
#                                (record/draft/factory) and the
#                                clearing book (soft screens, emission
#                                gate) — transactions sits below
#                                activity; pl_activity links pl_ledger.
#   CLOSED (untangling rounds A/B/C, 2026-07-19 — colocate by consumer /
#   producer; invert driven seams):
#     primitives -> encoding     txn_readback moved to exporter/sinks/
#     primitives -> entities     (both edges were txn_readback's; its
#                                consumers were all export-side)
#     synth -> transactions      membership_filter moved to exporter/
#                                standard/ (filtering transactions is
#                                an export view; synth never reads the
#                                corpus)
#     taxonomies -> entities     the institutional account catalog
#                                moved to entities/
#                                institutional_accounts.hpp (it IS
#                                entity-key vocabulary; namespace
#                                counterparties unchanged)
#     relationships -> activity  Contacts moved to its PRODUCER:
#                                relationships/social/contacts.hpp
#                                (namespace relationships::social; the
#                                commerce view now names the true
#                                origin)
#     diagnostics -> activity    spending-emission Stats moved INTO the
#                                spending layer: activity/spending/
#                                diagnostics.{hpp,cpp} (namespace
#                                activity::spending::diagnostics);
#                                pl_diagnostics keeps the logger only
#     activity -> transfers      round C-1 DEPENDENCY INVERSION: the
#                                simulator drives the activity-owned
#                                CardCycleBilling interface (simulator/
#                                card_cycle_billing.hpp); transfers'
#                                CardCycleDriver implements it and the
#                                wiring sites are transfers-side
#                                (legit/routines/spending*.cpp), so the
#                                arrow points down; the declared
#                                pl_activity -> pl_transfers link cycle
#                                is removed from the root CMakeLists
#     transfers -> pipeline      round C-2: settlement chunking policy
#                                moved to the pipeline's TransferStage
#                                (settlementChunking — chunking is
#                                orchestration policy; RunScope carried
#                                it but assembly never read it), and
#                                the legit builder now consumes the
#                                transfers-owned WorldInputs view,
#                                adapted from the pipeline bundles at
#                                legitWorldInputs(); the declared
#                                pl_transfers -> pl_pipeline link cycle
#                                is removed from the root CMakeLists
#   OPEN: none — the report reached clean at round C-2 and the
#   PL_LAYER_LINT_FATAL default flipped ON in the L2 close-out commit.
#
# Two things are ALWAYS fatal, escape hatch or not, audit-style:
#   * a file whose path maps to no known layer;
#   * an include of an unknown "phantomledger/<component>".
# The layer map must be complete, or the lint is theater.

option(PL_LAYER_LINT_FATAL
    "Treat include-layer violations as configure errors" ON)

# ------------------------------------------------------------- the DAG
#
# Vocabulary trees (header-only, no job library, the shared language —
# any job layer may include them; they themselves may only look down):
#   taxonomies   enum/type vocabulary            -> (nothing)
#   entities     keys, records, registries       -> primitives, taxonomies
#   encoding     ID rendering/parsing layouts    -> + entities
#   math         amount/count/timing models      -> + entities
#
# Job layers (mirror the L1 static-library links, transitively):
#   primitives   -> taxonomies
#   diagnostics  -> primitives            (+ vocabulary)
#   synth        -> primitives, diagnostics, relationships  (pl_world)
#   relationships-> primitives, diagnostics, synth          (pl_world)
#   transactions -> primitives            (pl_ledger)
#   activity     -> primitives, diagnostics, synth, relationships,
#                   transactions          (amended 2026-07-19)
#   transfers    -> + activity
#   pipeline     -> + transfers
#   exporter     -> + pipeline            (pl_export)
#   app          -> + exporter

set(PL_LINT_VOCAB taxonomies entities encoding math)

set(PL_LINT_ALLOWED_taxonomies "")
set(PL_LINT_ALLOWED_entities primitives taxonomies)
set(PL_LINT_ALLOWED_encoding primitives taxonomies entities)
set(PL_LINT_ALLOWED_math primitives taxonomies entities)

set(PL_LINT_ALLOWED_primitives taxonomies)
set(PL_LINT_ALLOWED_diagnostics primitives ${PL_LINT_VOCAB})
set(PL_LINT_ALLOWED_synth
    primitives diagnostics relationships ${PL_LINT_VOCAB})
set(PL_LINT_ALLOWED_relationships
    primitives diagnostics synth ${PL_LINT_VOCAB})
set(PL_LINT_ALLOWED_transactions primitives ${PL_LINT_VOCAB})
set(PL_LINT_ALLOWED_activity
    primitives diagnostics synth relationships transactions
    ${PL_LINT_VOCAB})
set(PL_LINT_ALLOWED_transfers
    primitives diagnostics synth relationships transactions activity
    ${PL_LINT_VOCAB})
set(PL_LINT_ALLOWED_pipeline
    primitives diagnostics synth relationships transactions activity
    transfers ${PL_LINT_VOCAB})
set(PL_LINT_ALLOWED_exporter
    primitives diagnostics synth relationships transactions activity
    transfers pipeline ${PL_LINT_VOCAB})
set(PL_LINT_ALLOWED_app
    primitives diagnostics synth relationships transactions activity
    transfers pipeline exporter ${PL_LINT_VOCAB})

# ------------------------------------------------------------- helpers

# Layer of a repo-relative path: src/<layer>/... or
# include/phantomledger/<layer>/... . Unmapped -> "".
function(pl_lint_layer_of path out)
    if(path MATCHES "^src/([a-zA-Z0-9_]+)/")
        set(${out} "${CMAKE_MATCH_1}" PARENT_SCOPE)
    elseif(path MATCHES "^include/phantomledger/([a-zA-Z0-9_]+)/")
        set(${out} "${CMAKE_MATCH_1}" PARENT_SCOPE)
    else()
        set(${out} "" PARENT_SCOPE)
    endif()
endfunction()

# ------------------------------------------------------------ the lint
#
# Call with the full list of repo-relative translation units (the
# audit union plus src/app/main.cpp); project headers are discovered
# here so the lint and the build can never disagree about the header
# set. Both quote and angle-bracket include forms are scanned.

function(pl_run_include_layer_lint)
    file(GLOB_RECURSE _lintHeadersAbs CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/include/phantomledger/*.hpp"
    )

    set(_lintFiles ${ARGN})
    foreach(hdr IN LISTS _lintHeadersAbs)
        file(RELATIVE_PATH rel "${CMAKE_CURRENT_SOURCE_DIR}" "${hdr}")
        list(APPEND _lintFiles "${rel}")
    endforeach()

    set(_violations)
    set(_fileCount 0)
    set(_edgeCount 0)

    foreach(file IN LISTS _lintFiles)
        pl_lint_layer_of("${file}" _layer)
        if(_layer STREQUAL "")
            message(FATAL_ERROR
                "include-layer lint: no layer mapping for '${file}' — "
                "extend the map in cmake/IncludeLayerLint.cmake")
        endif()
        if(NOT DEFINED PL_LINT_ALLOWED_${_layer})
            message(FATAL_ERROR
                "include-layer lint: unknown layer '${_layer}' for "
                "'${file}' — add it to the DAG in "
                "cmake/IncludeLayerLint.cmake")
        endif()

        math(EXPR _fileCount "${_fileCount} + 1")

        file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/${file}" _includeLines
            REGEX "^[ \t]*#[ \t]*include[ \t]+[\"<]phantomledger/")

        foreach(line IN LISTS _includeLines)
            string(REGEX MATCH "phantomledger/([a-zA-Z0-9_]+)([./])[^\">]*"
                _incPath "${line}")
            if(_incPath STREQUAL "")
                continue()
            endif()
            set(_dep "${CMAKE_MATCH_1}")
            if(CMAKE_MATCH_2 STREQUAL ".")
                message(FATAL_ERROR
                    "include-layer lint: '${file}' includes "
                    "'${_incPath}' — root-level project headers have "
                    "no layer; move it into a layer tree")
            endif()
            if(NOT DEFINED PL_LINT_ALLOWED_${_dep})
                message(FATAL_ERROR
                    "include-layer lint: '${file}' includes unknown "
                    "layer '${_dep}' ('${_incPath}') — add it to the "
                    "DAG in cmake/IncludeLayerLint.cmake")
            endif()

            math(EXPR _edgeCount "${_edgeCount} + 1")

            if(NOT _dep STREQUAL _layer
               AND NOT _dep IN_LIST PL_LINT_ALLOWED_${_layer})
                list(APPEND _violations
                    "  ${file}  [${_layer} -/-> ${_dep}]  ${_incPath}")
            endif()
        endforeach()
    endforeach()

    list(REMOVE_DUPLICATES _violations)
    list(LENGTH _violations _violationCount)

    if(_violationCount EQUAL 0)
        message(STATUS
            "include-layer lint: clean (${_fileCount} files, "
            "${_edgeCount} project-include edges)")
        return()
    endif()

    string(JOIN "\n" _violationBlock ${_violations})
    if(PL_LAYER_LINT_FATAL)
        message(FATAL_ERROR
            "include-layer lint: ${_violationCount} illegal edge(s) "
            "across ${_fileCount} files:\n${_violationBlock}\n"
            "Fix the edges or amend the DAG in "
            "cmake/IncludeLayerLint.cmake (named commit).")
    else()
        message(WARNING
            "include-layer lint (ESCAPE HATCH — do not merge): "
            "${_violationCount} illegal edge(s) across ${_fileCount} "
            "files (${_edgeCount} edges checked):\n${_violationBlock}\n"
            "The build continues because PL_LAYER_LINT_FATAL=OFF.")
    endif()
endfunction()
