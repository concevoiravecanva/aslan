# Aslan

Aslan is a small C/SDL2 + Lua-based canvas prototype with an append-only command log.

## Pages / Spaces (document structure)

Aslan documents can contain multiple **surfaces** (internally still called “pages”). Each surface has a **type**:

- **Space**: infinite canvas (Miro-like). This is the default.
- **Page**: fixed-size sheet (A4/A3/Letter/custom). Think “print layout”.
- **Table** *(placeholder)*: reserved for a structured tabletop / grid / spreadsheet-like surface.
- **Note** *(placeholder)*: reserved for a lightweight note surface.
- **Clip** *(placeholder)*: reserved for a clipped/embedded surface.

Only **Space** and **Page** have dedicated behavior today. **Table/Note/Clip** are persisted and visible in the UI, but currently behave like a generic surface (behavior TBD).

## Pan / Zoom

- Pan/zoom applies to both **Space** and **Page** surfaces.
- Zoom can be changed via mouse wheel and via UI buttons.
- The camera state is **persisted** (saved/reloaded), but it is **not undoable**.

## Persistence model

Aslan uses a simple AOF-style log (append-only file). On rewrite/export, it may emit a canonical, stable ordering.

### Surface type tokens

Surface types are persisted with:

- `page_type <page_id> space|page|table|note|clip`

Backward compatibility:

- `page_type <page_id> board` is accepted as **space** (legacy)
- `page_type <page_id> print` is accepted as **page** (legacy)

### Surface metadata

Surface metadata is stored with:

- `page_meta <page_id> <key> <value>`

Currently used keys:

- For **Page** (fixed): `width`, `height` (millimeters)
- For **Space** (infinite) and camera: `viewport.x`, `viewport.y`, `viewport.zoom`

Note: camera keys are intentionally excluded from undo/redo.

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
