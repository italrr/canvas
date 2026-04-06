# Canvas VS Code support

This starter extension adds:

- `.cv` language registration
- syntax highlighting for Canvas source
- comment/bracket/string behavior
- a starter dark theme named **Canvas Dark**

## Install locally

1. Copy this folder somewhere on disk.
2. In VS Code, open the Extensions view.
3. Use **Developer: Install Extension from Location...** if available, or package it with `vsce`.
4. Open a `.cv` file and set the language mode to **Canvas**.
5. Optionally switch to the **Canvas Dark** theme.

## Notes

This provides syntax coloring and bracket behavior.

It does **not** provide full parser-based diagnostics. VS Code can help with bracket matching and bracket pair colorization, but real syntax errors like full malformed forms would need a Language Server or custom diagnostics extension logic.
