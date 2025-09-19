# Contract: Window Manager Actions (Conceptual)

This contract describes user-visible actions and expected state changes. It is not an API surface but a behavioral contract for testing.

## Actions
- createWindow(title, size?): creates a floating window.
- dockWindow(windowId, targetRegion, position): attaches window into left/right/top/bottom or center as tab.
- undockWindow(windowId): moves window to floating.
- closeWindow(windowId): closes window and removes from layout.
- reorderTabs(regionId, newOrder[]): changes tab order within region.
- resizeRegion(regionId, delta): resizes region respecting minima.
- saveLayout(name?): saves current layout as named layout; if no name, persists as last-used.
- loadLayout(name): loads a named layout.

## Expected Outcomes
- Creating a window adds it to layout with state=floating.
- Docking updates region children and sets state=docked; active tab updates accordingly.
- Undocking sets state=floating and removes from region.
- Closing removes from layout and any region references.
- Resizing preserves minima and adjusts adjacent regions proportionally.
- Saving writes a serializable layout; loading restores positions and active tabs.
