# 005 - ImGui Log Sink TextEdit Enhancement

## Summary
Replace the existing ImGui log console scroll region (fully replacing the old per-line ImGui rendering; no legacy mode retained) with an embedded `ImGuiColorTextEdit` (`TextEditor`) instance to enable:
- Level based colored highlighting (INFO green, WARN yellow/orange, ERROR red, CRITICAL bright red, DEBUG/TRACE cyan, timestamps dim gray)
- Better text selection / copy UX (multi-line, word selection, Ctrl+A etc.)
- Optional future features: in-buffer search, jump to next error, bookmarks.

This is a hard replacement: the previous immediate-mode line list will be removed. (A future experimental legacy toggle could be reintroduced if ever needed, but is explicitly out of scope now.)

## Motivation
The current console draws each log line with immediate-mode ImGui calls. This limits:
- Rich text or multi-color segments per line
- Advanced navigation / selection behaviors
- Extensible syntax highlighting & search

Using `ImGuiColorTextEdit` gives us a token coloring pipeline and built-in editing behaviors while staying read-only.

## Goals
1. Introduce a read-only `TextEditor` instance backing the log console.
2. Apply per-token coloring for log level tags and timestamp segments.
3. Preserve existing console controls (filters, clear, copy, level mask, text filter, buffer cap, autoscroll).
4. Efficiently rebuild editor contents only when inputs change (snapshot growth, filters, clear).
5. Maintain autoscroll semantics identical to current implementation.
6. Keep memory usage bounded by existing ring buffer capacity.

## Non-Goals (Initial Phase)
- Incremental append optimization (may rebuild full text initially)
- In-editor search UI (can leverage text filter + future enhancements)
- Bookmarking / jump-to navigation
- Persisting caret/selection across rebuilds

## User Stories
- As a developer I can visually distinguish log severities at a glance.
- As a developer I can select and copy multiple lines including wrapped text easily.
- As a developer I can still clear, filter, and auto-scroll like before.
<!-- Legacy revert user story removed: hard replacement requirement -->

## Functional Requirements
FR1: Provide a `TextEditor` initialized read-only, no line numbers by default (configurable later).
FR2: Rebuild editor text from filtered snapshot of log sink lines.
FR3: Level highlighting mapping:
- TRACE / DEBUG -> Cyan (or muted blue)
- INFO -> Green
- WARN -> Yellow / Orange
- ERROR -> Red
- CRITICAL -> Bright Red / Magenta accent
FR4: Timestamp pattern `^[\[]HH:MM:SS[\]]` colored dim gray.
FR5: Clear action empties both sink buffer and editor text; state resets.
FR6: Copy action copies exactly current editor visible text (all filtered lines concatenated).
FR7: Text filter (substring) narrows which lines populate the editor.
FR8: Autoscroll, when enabled and the view was already at bottom, scrolls to end after rebuild.
FR9: Rebuild triggers when (a) snapshot size changes, (b) filter text changes, (c) level mask changes, (d) clear invoked.
<!-- FR10 (legacy fallback) removed per updated requirement: no optional mode -->

## Performance Requirements
PR1: Full rebuild under 5 ms for <= 5,000 log lines on mid-tier CPU (initial target; later incremental optimization if exceeded).
PR2: Rebuild frequency limited to at most once per frame when changes detected.

## Data Model / State Additions
Console state structure extended with:
- `TextEditor logEditor;`
- `bool logEditorInitialized = false;`
- `size_t lastSnapshotCount = 0;`
- `uint64_t lastFilterHash = 0;`
<!-- legacyMode flag removed (hard replacement) -->

## Language Definition Strategy
Use a custom `LanguageDefinition`:
- Insert identifiers for each log level token.
- Provide a simple regex or identifier for timestamps (if regex support limited, preprocess lines to wrap timestamp in a token-like marker before setting text; alternative: color timestamps using line parsing—phase 2).
- Keep other tokens uncolored.

## API / Integration Changes
- Add helper: `static TextEditor::LanguageDefinition CreateLogLanguageDefinition();`
- Add helper to (re)build: `void RebuildLogEditorIfNeeded();`
- Wrap inside existing console render function (replacing old per-line region entirely).

## Risks & Mitigations
| Risk | Mitigation |
|------|------------|
| Large log buffers cause frame hitch on rebuild | Add incremental append in later iteration; consider future optimizations (no legacy fallback) |
| Timestamp coloring inaccurate | Use a conservative regex; if not available, skip initially |
| Palette conflicts with existing theme | Provide palette overrides only for used tokens |
| Memory duplication (editor keeps large contiguous string) | Ring buffer is bounded; monitor size and assert; optional max displayed lines cap |

## Acceptance Criteria
- All functional requirements FR1–FR9 demonstrated manually.
- No references to legacy mode remain in code or UI.
- No crashes or assertions when rapidly logging while interacting with UI.
- Colors visually applied per spec.

## Future Enhancements (Backlog)
- Incremental append & partial re-tokenization
- Search bar (Ctrl+F) & next/previous level navigation
- Line number toggle & wrap toggle persistence
- Click-to-filter (click a level token to isolate that level)
- Export logs button

## Implementation Notes
Initial version may choose the simplest coloring: rely on identifiers; preprocess each line to inject level token as first word (already present). If regex for timestamp not trivial, defer timestamp coloring to a second pass.

