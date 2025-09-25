# Tasks - 005 ImGui Log Sink TextEdit Replacement

## Current Progress Snapshot (Phase 1 Complete → Entering Phase 2)
Phase 0 and Phase 1 have been fully implemented. The log console now uses a read-only TextEditor with level + timestamp highlighting, filtering, copy, clear, and autoscroll parity. Incremental append optimization (baseline: no filter + full level mask) is implemented and live. We are transitioning to Phase 2 hardening & performance instrumentation.

Key implemented optimizations beyond original Phase 1 scope:
- Incremental append path with cached previous raw lines & emitted count.
- Cached editor text buffer to avoid pulling text on every frame.
- Skip `SetText` when no visible changes (empty append or unchanged rebuilt buffer via size/content checks & char count heuristic).
- Conditional autoscroll only when text actually changed.
- Timestamp styling via synthetic token (no regex cost each frame).

Next immediate Phase 2 priorities (proposed):
1. T2.1 Instrument rebuild/append timings (debug-only stats) + counters for skipped vs executed `SetText`.
2. New Task T2.6 Ring buffer truncation (front-eviction) detection to safely broaden incremental applicability.
3. New Task T2.7 Filter-aware incremental append (maintain filtered index mapping or fallback heuristics).
4. (Optional later) Search highlighting (T2.5) & palette configurability (T2.3) if still desired.

---

## Overview
Hard replacement of the legacy per-line ImGui log console with a read-only `ImGuiColorTextEdit` (`TextEditor`) driven view, providing level color highlighting, filtering, copy, clear, and autoscroll parity. Incremental append & search were initially deferred; incremental append (baseline) is now delivered early.

## Conventions
- Source integration lives in existing window/console management code (likely `WindowManager.cpp` / related state headers).
- New helpers kept `static` or anonymous namespace unless reused elsewhere.
- Performance target: full rebuild < 5 ms for ≤ 5,000 lines (development build). If exceeded, open a follow-up perf ticket.

## Assumptions
- Existing log sink snapshot API returns stable vector/array of lines with level + text + timestamp (or we can reconstruct timestamp embedded at start of line).
- Ring buffer capacity already bounded (no unbounded growth risk).
- `TextEditor` is already available in the build (confirmed via previous integration for file editing).

## Phase 0 – Preparation
| ID | Task | Details | Acceptance Criteria |
|----|------|---------|---------------------|
| T0.1 | [x] Locate legacy render block | Identify the precise function/section rendering the log console lines. | Section located & commented with `// LEGACY_LOG_RENDER_START/END (to be removed)` before removal. |
| T0.2 | [x] Add console state fields | Extend console/window state: `TextEditor logEditor; bool logEditorInitialized=false; size_t logLastSnapshotCount=0; uint64_t logLastFilterHash=0;` | Code compiles; state accessible in render path. |
| T0.3 | [x] Decide timestamp source | Confirm whether timestamp is part of stored line string or needs formatting. | Decision: use sink-provided timestamp, injected as synthetic token for styling. |

## Phase 1 – Core Replacement (All Completed)
| ID | Task | Details | Acceptance Criteria |
|----|------|---------|---------------------|
| T1.1 | [x] Implement language definition | Function `CreateLogLanguageDefinition()` returning customized `TextEditor::LanguageDefinition` with identifier color overrides for levels. | Function returns stable object; palette stable. |
| T1.2 | [x] Initialize editor | On first console render: set language, disable line numbers (`SetShowWhitespaces(false)`, `SetShowLineNumbers(false)`), set read-only, palette applied. | Empty read-only editor appears; no crash. |
| T1.3 | [x] Hash utility | Lightweight FNV-1a over snapshot size, level mask bits, filter text. | Hash changes when any input changes. |
| T1.4 | [x] Rebuild helper | `RebuildLogEditorIfNeeded(...)` full rebuild path implemented. | Updates only when needed (hash delta). |
| T1.5 | [x] Level highlighting tokens | First token mapped to level color. | Distinct colors per level. |
| T1.6 | [x] Timestamp coloring (basic) | Synthetic timestamp token inserted and colored dim. | Timestamps uniformly dim; malformed untouched. |
| T1.7 | [x] Autoscroll parity | Detect bottom; autoscroll only when user was at bottom and content changed. | Behavior matches legacy console. |
| T1.8 | [x] Clear integration | Clear resets sink + editor + counters. | Editor blank post-clear; new lines show. |
| T1.9 | [x] Copy integration | Copy button copies filtered text. | Clipboard matches editor (spot-check). |
| T1.10 | [x] Remove legacy code | Old immediate-mode loop removed. | No legacy references remain. |
| T1.11 | [x] Logging for debug | Optional debug logs guarded (where enabled). | No release noise. |
| T1.12 | [x] Acceptance pass | Manual FR1–FR9 verified. | Phase 1 sign-off recorded. |

## Phase 2 – Quality & Hardening (In Progress)
| ID | Task | Details | Acceptance Criteria |
|----|------|---------|---------------------|
| T2.1 | Performance measurement | Instrument rebuild & append duration + counters for full rebuilds, incremental appends, skipped `SetText`. | Debug metrics visible; negligible overhead when disabled. |
| T2.2 | Max displayed lines cap | Allow optional cap (e.g., 2,000) if performance dips. | Define / setting reduces rebuild time with very large buffers. |
| T2.3 | Palette configuration hook | Add settings entry to switch dark/light palette for log console independently. | UI toggle persists (if settings infra present). |
| T2.4 | [x] Incremental append prototype | Append only new lines if no filter & full level mask; fallback otherwise. | Works; no duplicate lines; SetText skipped when no visible delta. |
| T2.5 | Basic in-console search | Simple inline search input highlighting (non-filtering). | Matching substrings emphasized. |
| T2.6 | [x] Ring truncation detection | Detect when ring buffer evicts front so incremental path re-synchronizes (prefix comparison + metric). | Falls back to full rebuild on mismatch; metric increments; no line loss. |
| T2.7 | Filter-aware incremental append | Maintain filtered state or heuristic to allow incremental append with active filter / partial level mask. | Incremental still correct under filter changes; correctness tests pass. |

NOTE: T2.4 delivered early and is live. Focus now shifts to instrumentation (T2.1) then correctness broadening (T2.6, T2.7) before optional UX features.

## Implementation Notes
- Rebuild strategy initial: full text replacement using `SetText()`. (TextEditor handles tokenization on assignment.)
- Current: Hybrid with incremental append path (no filter + full mask, no front truncation) plus multiple no-op avoidance checks.
- FNV-1a 64-bit hash: simple, fast—combine snapshot size, mask, and filter text bytes.
- At-bottom detection: track last visible line index (totalLines - 1) and current cursor or compute from scroll ratio; if editor API lacks scroll accessor, approximate by remembering we last autoscrolled and user did not move cursor.
- If timestamp regex is complex, fallback approach: before concatenation, detect `line.size() >= 10 && line[0]=='[' && line[9]==']'` then optionally prefix token like `TS_` and map `TS_` to gray; or rely on manual segment coloring in future enhancement. Implemented synthetic token approach.

## Deliverables
- Code changes in console render path & new helpers.
- Removal commit of legacy rendering loop.
- Spec annotation referencing completion (update spec acceptance section if needed).

## Exit Criteria (for this feature)
- All Phase 1 tasks complete.
- Manual checklist for FR1–FR9 documented.
- No build warnings introduced related to unused legacy pieces.

## Out of Scope
- Multi-buffer log views, persistence to disk, advanced search heuristics.

---
Use this file to track progress (mark tasks with [x] as they are completed in your PR iterations).