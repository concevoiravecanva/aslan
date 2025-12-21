# Aslan

## Save `.aslan` (snapshot)

Aslan currently follows a **Photoshop-like** model:

- A document saved as a `.aslan` file on disk contains a **single snapshot** (no built-in version history yet).
- Undo/Redo are session-based.

In the Inspector panel:

- Use **“Save As .aslan…”** to write the current document snapshot to a chosen folder.

### `.aslan` file format (v1)

The file is UTF-8 text and starts with:

- `-- aslan:1 --`

Then it contains replayable commands (one per line), in canonical order:

1. `doc_type ...`
2. `doc_meta width ...`
3. `doc_meta height ...`
4. `set_global ...` (optional)
5. `create_rect ...` / `create_text ...`
6. `set_visible ...` (only when hidden)
7. `bring_to_front ...` (z-order)

Future versions may add extra optional sections which older parsers can ignore.
