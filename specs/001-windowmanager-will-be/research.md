# Research: WindowManager (Dockable Windows)

## Decisions
- Docking model: Regions left/right/top/bottom + center-to-tab
- Grouping UI: Tabbed interface with overflow navigation
- Resizing constraints: Minimum region size 200×120 logical units; window-specific minima may override
- Persistence: Auto-save last layout on exit; auto-restore on launch; setting to disable
- Keyboard control: Must support focus cycle, dock/undock, close; mappings configurable/documented
- Multi-monitor/DPI: Robust handling; relocate to primary if missing
- Scale: Support 50+ concurrent windows without corruption

## Rationale
- Tabbed docking is familiar and proven for complex tool UIs (keeps layout predictable).
- Minimum sizes prevent unreadable/untouchable UI controls and avoid infinite layouts.
- Auto-persistence improves UX and accelerates workflows; opt-out respects user preference.
- Keyboard accessibility is a baseline usability requirement for power users.
- Multi-monitor/DPI resilience avoids lost windows and support issues.

## Alternatives Considered
- Stacked (non-tabbed) groups → rejected due to discoverability and navigation overhead.
- No automatic persistence → rejected; too much friction for users.
- Arbitrary freeform placement without docking hints → rejected; error-prone and confusing.

## Open Items
- File format for layout storage: JSON preferred for readability.
- Exact keyboard mappings: to be defined in quickstart and help overlay.
- DPI handling specifics: proportional scaling vs. fixed logical units; align with project conventions.
