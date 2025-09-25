# Tasks - 005 ImGui Log Sink TextEdit Replacement

## Overview
Hard replacement of the legacy per-line ImGui log console with a read-only `ImGuiColorTextEdit` (`TextEditor`) driven view, providing level color highlighting, filtering, copy, clear, and autoscroll parity. Incremental append & search are deferred.

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
| T0.1 | Locate legacy render block | Identify the precise function/section rendering the log console lines. | Section located & commented with `// LEGACY_LOG_RENDER_START/END (to be removed)` before removal. |
| T0.2 | Add console state fields | Extend console/window state: `TextEditor logEditor; bool logEditorInitialized=false; size_t logLastSnapshotCount=0; uint64_t logLastFilterHash=0;` | Code compiles; state accessible in render path. |
| T0.3 | Decide timestamp source | Confirm whether timestamp is part of stored line string or needs formatting. | Document decision inline in code comment. |

## Phase 1 – Core Replacement
| ID | Task | Details | Acceptance Criteria |
|----|------|---------|---------------------|
| T1.1 | Implement language definition | Function `CreateLogLanguageDefinition()` returning customized `TextEditor::LanguageDefinition` with identifier color overrides for levels. | Function returns stable object; unit/local test prints palette mapping (optional). |
| T1.2 | Initialize editor | On first console render: set language, disable line numbers (`SetShowWhitespaces(false)`, `SetShowLineNumbers(false)`), set read-only, choose palette (Dark + tweaks). | First open of console shows empty read-only editor; no crash. |
| T1.3 | Hash utility | Implement lightweight hash (e.g., FNV-1a) over: snapshot size, active level mask bits, filter text. | Changing any input causes different hash (verified via debug log). |
| T1.4 | Rebuild helper | `RebuildLogEditorIfNeeded(snapshot, levelMask, filterText)` builds concatenated filtered lines with '\n'. | Editor updates only when any input changes; manual log spam shows responsive updates. |
| T1.5 | Level highlighting tokens | Ensure first token (e.g., `INFO`, `ERROR`) is picked up by language definition (identifiers map). | Visual color difference per level confirmed. |
| T1.6 | Timestamp coloring (basic) | Apply dim gray: either regex if available, or simple prefix parse `[HH:MM:SS]`. Option: preprocess line to wrap timestamp with sentinel token (e.g., `TS_`). | Timestamps rendered dim; malformed lines remain unaffected. |
| T1.7 | Autoscroll parity | Detect if user at bottom before rebuild (compare editor total lines vs current scroll/ cursor). After rebuild & if autoscroll enabled, move cursor to end (`SetCursorPosition`). | When autoscroll on, new logs keep view pinned; when scrolled up, no jump. |
| T1.8 | Clear integration | Clear button: clears sink buffer (existing) then explicitly sets editor text to empty, reset counters. | After Clear, editor blank; new logs append from scratch. |
| T1.9 | Copy integration | Copy button copies current editor full text (filtered view). | Clipboard contains same text as editor (spot-check). |
| T1.10 | Remove legacy code | Delete old immediate-mode loop. Remove any dead variables. | No references to legacy rendering remain; build passes. |
| T1.11 | Logging for debug | (Optional) Add `SPDLOG_DEBUG` statements guarded by `#ifdef` for rebuild events. | Can be toggled via define; no noise in release. |
| T1.12 | Acceptance pass | Manual verification of FR1–FR9 from spec. | Checklist completed & noted in spec PR. |

## Phase 2 – Quality & Hardening (Optional / Stretch)
| ID | Task | Details | Acceptance Criteria |
|----|------|---------|---------------------|
| T2.1 | Performance measurement | Instrument rebuild duration with scope timer (debug only). | Log line printed with ms on rebuild (debug builds). |
| T2.2 | Max displayed lines cap | Allow optional cap (e.g., 2,000) if performance dips. | Config define reduces rebuild time if large buffers tested. |
| T2.3 | Palette configuration hook | Add settings entry to switch dark/light palette for log console independently. | UI toggle persists (if settings infra present). |
| T2.4 | Incremental append prototype | Append only new lines if no filter & same level mask; fallback to full rebuild otherwise. | Append path validated (line count grows, no duplicate full text). |
| T2.5 | Basic in-console search | Simple inline search input filtering highlight (without narrowing lines). | Matching substrings visually emphasized. |

## Implementation Notes
- Rebuild strategy initial: full text replacement using `SetText()`. (TextEditor handles tokenization on assignment.)
- FNV-1a 64-bit hash: simple, fast—combine snapshot size, mask, and filter text bytes.
- At-bottom detection: track last visible line index (totalLines - 1) and current cursor or compute from scroll ratio; if editor API lacks scroll accessor, approximate by remembering we last autoscrolled and user did not move cursor.
- If timestamp regex is complex, fallback approach: before concatenation, detect `line.size() >= 10 && line[0]=='[' && line[9]==']'` then optionally prefix token like `TS_` and map `TS_` to gray; or rely on manual segment coloring in future enhancement.

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