# Quickstart: ConfigurationManager

## Initialize
- Call `ConfigurationManager::loadOrDefault()` at startup

## Read values
- `getBool("feature.enabled")`
- `getInt("window.width")`
- `getDouble("physics.gravity")`
- `getString("ui.theme")`
- `getStringList("recent.files")`

## Write values
- `set("ui.theme", "light")`; then `save()`

## Observe changes
- Register callback; it will be invoked after successful save

## Overrides
- Set environment variables, e.g. `GB2D_WINDOW__WIDTH=1024`
