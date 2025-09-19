# Data Model: WindowManager

## Entities

### Window
- id: string
- title: string
- state: enum [floating, docked]
- minSize: { width: int, height: int } (optional)
- lastFocusTimestamp: datetime (optional)

### DockRegion
- id: string
- position: enum [left, right, top, bottom, center]
- size: { width: int, height: int }
- children: [WindowRef or DockRegionRef]
- activeTabId: string (when tabbed)

### Layout
- id: string
- regions: [DockRegion]
- windows: [Window]
- lastSaved: datetime
- name: string (optional; for named layouts)

## Relationships and Rules
- A Window belongs to exactly one of: floating space or a DockRegion.
- DockRegion may contain multiple windows (tabbed) or nested regions (split views).
- Layout must be serializable/deserializable for persistence.
- Enforce minimum sizes: region.width >= 200, region.height >= 120 unless overridden by Window.minSize.
- On restore, if a display from saved state is missing, relocate regions to primary display.
