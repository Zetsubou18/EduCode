# Third-party components

| Component | Pinned version | License / source | Role |
|---|---|---|---|
| Qt | 5.15.2 | [Qt licensing](https://doc.qt.io/archives/qt-5.15/licensing.html); LGPL/GPL/commercial alternatives | C++/QML and WebEngine runtime |
| Monaco Editor | 0.34.1 | MIT; [Microsoft Monaco](https://github.com/microsoft/monaco-editor) | Mature VS Code-derived editor; Python Monarch highlighting, editor widgets and navigation |
| Pyright | 1.1.408 | MIT; [Microsoft Pyright](https://github.com/microsoft/pyright) | Python LSP and static analysis using the project's interpreter |
| xterm.js / fit addon | 5.3.0 / 0.8.0 | MIT; [xterm.js](https://github.com/xtermjs/xterm.js) | Terminal display and input; pinned for Qt 5 Chromium compatibility |
| node-pty | 1.1.0 | MIT; [Microsoft node-pty](https://github.com/microsoft/node-pty) | ConPTY on Windows and PTY on Linux/macOS; used by VS Code |
| Codicons | 0.0.36 | MIT; [VS Code Codicons](https://github.com/microsoft/vscode-codicons) | Interface icon font |
| Material Icon Theme | 5.28.0 | MIT; [Material Icon Theme](https://github.com/material-extensions/vscode-material-icon-theme) | Folder, Python, JSON, document SVGs |
| Node.js | 22.15.0 on this machine | MIT and bundled third-party notices; [Node.js](https://github.com/nodejs/node) | Bundled Pyright and PTY process runtime |

Dependencies were selected for maintained production usage, local operation and Windows/Linux/macOS support. Monaco and xterm provide their established popup, keyboard, cursor and terminal rendering instead of reimplementing an editor or terminal. Pyright is configured with `diagnosticMode=openFilesOnly` and basic checking. The application only keeps the active document open to the LSP.

Original npm license files are copied beside the distributed runtime packages. Material icons retain their license in `assets/icons`. Node/Qt license texts are in `assets/licenses`. Qt libraries are dynamically linked and shipped as separate replaceable DLLs. Qt WebEngine also incorporates Chromium and its third-party code under their original licenses; see [Qt WebEngine licensing](https://doc.qt.io/archives/qt-5.15/qtwebengine-licensing.html).

Exact Qt source archives: [Qt 5.15.2 source](https://download.qt.io/archive/qt/5.15/5.15.2/single/). The application's complete C++/QML source and build scripts are available in this project. When redistributing a release, preserve these notices and provide the corresponding Qt/Chromium source and license material required by the selected Qt license; this repository does not declare a license for the user's original EduCode code or supplied artwork.

User-provided `app_logo.png` and `welcome_background.png` were copied as provided. They are not part of any third-party icon package.
