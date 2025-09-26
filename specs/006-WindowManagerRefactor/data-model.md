# Data Model: Modular Windows and Layout JSON

## Manager Metadata (layout.json root)
- `version: int` – schema version
- `next_id: int` – next window id counter
- `last_folder: string` – last file dialog folder
- `recent_files: string[]` – MRU
- `windows: WindowEntry[]`

### WindowEntry
- `id: string` – e.g. "win-7"
- `type: string` – `IWindow::typeId()`
- `title: string`
- `open: bool`
- `state: object` – window-specific JSON

## IWindow State Examples

### ConsoleLogWindow.state
- `autoscroll: bool`
- `max_lines: int`
- `buffer_cap: int`
- `level_mask: int`
- `text_filter: string`
- `search: { query: string, case: bool }`

### CodeEditorWindow.state
- `tabs: string[]` – file paths
- `current: int`
- `last_folder: string`
- (optional) `cursor_positions: { [path: string]: { line: int, column: int } }`

### FilePreviewWindow.state
- `path: string` – last opened file
- (derived) `kind: string` – text/image (may be omitted; recomputed)
