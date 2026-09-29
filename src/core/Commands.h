#pragma once
#include <QVariantMap>
namespace Commands {
inline QVariantMap defaultBindings() {
    return {{"save", "Ctrl+S"},
            {"saveAll", "Ctrl+Shift+S"},
            {"find", "Ctrl+F"},
            {"replace", "Ctrl+H"},
            {"close", "Ctrl+W"},
            {"quickOpen", "Ctrl+P"},
            {"run", "F5"},
            {"back", "Alt+Left"},
            {"palette", "Ctrl+Shift+P"},
            {"split", "Ctrl+Alt+S"},
            {"terminal", "Ctrl+`"},
            {"toggleExplorer", "Ctrl+B"},
            {"notifications", "Ctrl+Shift+N"},
            {"browser", "Ctrl+Shift+B"}};
}
} // namespace Commands
