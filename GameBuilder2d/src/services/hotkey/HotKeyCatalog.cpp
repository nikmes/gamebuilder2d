#include "services/hotkey/HotKeyCatalog.h"
#include "services/hotkey/HotKeyRegistrationBuilder.h"

namespace gb2d::hotkeys {

HotKeyRegistration buildDefaultCatalog() {
    HotKeyRegistrationBuilder builder;
    builder.reserve(22);

    builder.withDefaults("File", "Global");
    builder.addWithDefaults(actions::OpenFileDialog,
                            "Open File",
                            "Ctrl+O",
                            "Open the universal file picker.");
    builder.addWithDefaults(actions::OpenImageDialog,
                            "Open Image",
                            "Ctrl+Shift+O",
                            "Open the image-focused picker with filtering.");

    builder.withDefaults("View", "Global");
    builder.addWithDefaults(actions::ToggleEditorFullscreen,
                            "Toggle Editor Fullscreen",
                            "F11",
                            "Toggle fullscreen mode for the editor viewport.");
    builder.addWithDefaults(actions::FocusTextEditor,
                            "Focus Text Editor",
                            "Ctrl+Shift+E",
                            "Spawn or focus the Text Editor window.");
    builder.addWithDefaults(actions::ShowConsole,
                            "Show Console",
                            "Ctrl+Shift+C",
                            "Spawn or focus the Console window.");
    builder.addWithDefaults(actions::SpawnDockWindow,
                            "New Dock Window",
                            "Ctrl+Shift+N",
                            "Create a new empty dockable window.");

    builder.withDefaults("Settings", "Global");
    builder.addWithDefaults(actions::OpenConfigurationWindow,
                            "Open Configuration",
                            "Ctrl+,",
                            "Open the Configuration window for editor settings.");
    builder.addWithDefaults(actions::OpenHotkeySettings,
                            "Open Hotkey Settings",
                            "Ctrl+Alt+K",
                            "Open the Hotkeys customization panel.");

    builder.withDefaults("Layouts", "Global");
    builder.addWithDefaults(actions::SaveLayout,
                            "Save Layout",
                            "Ctrl+Alt+S",
                            "Persist the current dock layout using the active name.");
    builder.addWithDefaults(actions::OpenLayoutManager,
                            "Open Layout Manager",
                            "Ctrl+Alt+L",
                            "Open the layout management popup to load or delete layouts.");

    builder.withDefaults("Code Editor", "Code Editor");
    builder.addWithDefaults(actions::CodeNewFile,
                            "New File",
                            "Ctrl+N",
                            "Create a new untitled tab in the code editor.");
    builder.addWithDefaults(actions::CodeOpenFile,
                            "Open File",
                            "Ctrl+Shift+O",
                            "Open a file into the active code editor.");
    builder.addWithDefaults(actions::CodeSaveFile,
                            "Save File",
                            "Ctrl+S",
                            "Save the active code editor tab.");
    builder.addWithDefaults(actions::CodeSaveFileAs,
                            "Save File As",
                            "Ctrl+Shift+S",
                            "Save the active tab to a new location.");
    builder.addWithDefaults(actions::CodeSaveAll,
                            "Save All",
                            "Ctrl+Alt+S",
                            "Save all open tabs in the code editor.");
    builder.addWithDefaults(actions::CodeCloseTab,
                            "Close Tab",
                            "Ctrl+W",
                            "Close the active code editor tab.");
    builder.addWithDefaults(actions::CodeCloseAllTabs,
                            "Close All Tabs",
                            "Ctrl+Shift+W",
                            "Close all tabs in the code editor.");

    builder.withDefaults("Game Window", "Game Window");
    builder.addWithDefaults(actions::GameToggleFullscreen,
                            "Toggle Game Fullscreen",
                            "Alt+Enter",
                            "Request fullscreen mode for the active game window.");
    builder.addWithDefaults(actions::GameReset,
                            "Reset Game",
                            "Ctrl+R",
                            "Reset the currently running embedded game.");
    builder.addWithDefaults(actions::GameCycleNext,
                            "Next Game",
                            "Ctrl+Tab",
                            "Cycle to the next registered game.");
    builder.addWithDefaults(actions::GameCyclePrev,
                            "Previous Game",
                            "Ctrl+Shift+Tab",
                            "Cycle to the previous registered game.");

    builder.withDefaults("Fullscreen", "Fullscreen Session");
    builder.addWithDefaults(actions::FullscreenExit,
                            "Exit Fullscreen Session",
                            "Esc",
                            "Exit the current fullscreen gameplay session.");

    return std::move(builder).build();
}

} // namespace gb2d::hotkeys
