# Task Breakdown — Audio Manager Window

## Clarifications & Preparations
- [x] T201 — Confirm AudioManager inspection APIs (snapshots, preview, events)
  - ✓ Inventory snapshots working (captureSoundInventorySnapshot, captureMusicInventorySnapshot)
  - ✓ Event subscription system implemented and functional
  - ✓ AudioEventSink interface implemented by AudioManagerWindow
- [x] T202 — Define UI layout and component details
  - ✓ Split-pane layout: Asset list (left) + Details tabs (right)
  - ✓ Three tabs: Preview, Config, Diagnostics
- [x] T203 — Outline state management for previews and config
  - ✓ Event-driven inventory refresh (inventoryDirty_ flag)
  - ✓ Event log with timestamp formatting
  - ⚠ Preview state management pending (T205)

## API Integration
- [x] T204 — Integrate inventory snapshots and event subscriptions
  - ✓ Event subscription in constructor with cleanup in destructor
  - ✓ Inventory snapshots on demand with automatic refresh on asset load/unload events
  - ✓ Event handler with proper logging and inventory dirty tracking
  - ⚠ Fixed mutex deadlock in publishAudioEvent (removed double lock)
- [x] T205 — Implement preview session management
  - ✓ Preview API integrated into UI play/stop flows for sounds and music
  - ✓ Preview state tracking with playback handle, key, and type bookkeeping
  - ✓ Resource cleanup on stop/asset switch including handle reset

## UI Implementation
- [x] T206 — Scaffold AudioManagerWindow and register with WindowManager
  - ✓ Window registered in WindowManager as "audio_manager"
  - ✓ Basic render structure with ImGui layout
  - ✓ Window lifecycle (constructor/destructor) properly manages subscriptions
- [x] T207 — Implement asset list with metadata and play buttons
  - ✓ Asset list shows sounds and music separately with counts
  - ✓ Tooltips display metadata (duration, ref count, sample rate, channels, path)
  - ✓ Placeholder assets shown in gray
  - ✓ Refresh button to manually update inventory
  - ✓ Play/stop buttons wired to preview logic for each asset
- [x] T208 — Add preview controls (play, stop, progress, params)
  - ✓ Preview panel renders asset metadata alongside control widgets
  - ✓ Implemented sound preview with live volume/pan/pitch updates
  - ✓ Music preview supports play/stop, pause/resume, and volume adjustments
  - ✓ Added progress/time display for music preview with live updates
  - ✓ Highlight paused state and cleanly reset UI when playback completes
- [x] T209 — Integrate config editing for audio settings
  - ✓ Config tab shows live device status and configuration values
  - ✓ Volume sliders, enable toggle, and max sound slots are fully editable
  - ✓ Search path list editor with add/remove controls implemented
  - ✓ Apply button persists via ConfigurationManager, reinitializes AudioManager, and reloads assets
  - ✓ Revert button restores last applied settings and clears in-progress edits
- [x] T210 — Display diagnostics and event log
  - ✓ Diagnostics panel shows metrics (initialized, device ready, silent mode, counts)
  - ✓ Event log with clear button and automatic size limiting (max 100 entries)
  - ✓ Event types properly formatted with timestamps
  - ✓ Shows key and details for each event

## State & Persistence
- [x] T211 — Handle dirty state and apply/save for config
  - ✓ Dirty state highlighted in UI with tab badge and warning banner
  - ✓ Apply flow persists via ConfigurationManager and reinitializes AudioManager
  - ✓ Discard flow restores baseline and modal guards prevent accidental close
  - ✓ Close prompt enforces apply/discard decisions before window shuts
- [x] T212 — Manage preview cleanup and resource handling
  - ✓ Stop preview when switching assets or starting a new one
  - ✓ Stop preview in destructor to avoid leaking playback
  - ✓ Clear playback handle/state on stop and audio events
  - ✓ Handle device-not-ready / silent-mode fallbacks and error UI messaging

## Testing & QA
- [ ] T213 — Unit tests for UI components and API integration
  - Need: Test event subscription/unsubscription
  - Need: Test inventory refresh logic
  - Need: Test event log size limiting
- [ ] T214 — Integration tests for preview and config changes
  - Need: Test preview playback with actual audio files
  - Need: Test config changes persist correctly
  - Need: Test volume changes take effect
- [x] T215 — Manual QA for audio playback and UI
  - ✓ Window renders without crashing
  - ✓ Asset list shows loaded sounds
  - ✓ Event subscription works (verified with logs)
  - ✓ Mutex deadlock fixed and verified
  - ⚠ Need to test with actual audio files loaded
  - ⚠ Need to test preview functionality once implemented

## Documentation
- [ ] T216 — Update docs for AudioManagerWindow usage
  - Need: Document how to open the Audio Manager window
  - Need: Document preview controls usage
  - Need: Document config editing workflow
  - Need: Document event log interpretation

---

## 🎯 Next Priority Tasks

### **Immediate Next Steps (High Priority):**

1. **T212 — Preview Cleanup**
  - Gracefully handle device-not-ready or silent-mode scenarios
  - Add user feedback when preview fails to start or is auto-stopped

2. **T211 — Config Dirty State**
  - Track edits in the Config tab with a dirty flag
  - Persist confirmed changes to config.json via ConfigurationManager
  - Provide revert/cancel flow for in-progress edits

### **Follow-up Tasks (Medium Priority):**
3. T213 — Unit tests for event handling
4. T214 — Integration tests with real audio

### **Polish & Documentation (Lower Priority):**
7. T215 — Extended manual QA
8. T216 — Documentation updates

---

## 🐛 Issues Fixed
- ✅ Mutex deadlock in AudioManager::publishAudioEvent (removed std::scoped_lock, caller now holds lock)
- ✅ AudioManager initialization error checking added to GameBuilder2d.cpp</content>
<parameter name="filePath">c:\Users\nikme\source\repos\GameBuilder2d\specs\012-audio-manager-window\tasks.md