# Aslan 🦁

Aslan is a lightweight, high-performance C/SDL2 document engine inspired by Notion, Airtable, and Canva. It features a block-based architecture, an append-only persistence model, and a powerful formula engine.

## ✨ Features

- **Block-Based Architecture**: Mix text, headings, tables, infinite canvases, and charts in a single document.
- **Modern Aesthetics**: Notion-style layout with Airtable-inspired table aesthetics, rounded checkboxes, and polished block cards ("Fiches").
- **Powerful Formula Engine**: Integrated **TinyExpr** C library for Excel-like formula calculations (`=ColumnA + ColumnB`).
- **Interactive Canvases**: Both infinite (Miro-style) and finite (Canva-style) drawing surfaces with vector nodes.
- **Append-Only History**: Robust persistence using an AOF (Append-Only File) for infinite undo/redo and crash recovery.
- **No-Silos Experience**: The app starts directly in the editor—no home screen, no friction.

## 🧱 Block Types

| Type | Description |
| :--- | :--- |
| **Text** | Clean, monospace-capable text editing. |
| **Heading 1** | Large section headers that automatically sync with page titles. |
| **Table** | Powerful column-store grid with multiple views (Grid, List, Board) and formulas. |
| **Canvas** | Infinite whiteboarding space for diagrams and strokes. |
| **Design** | Finite artboard for layouts and presentations. |
| **Chart** | Data visualizations linked directly to Table blocks. |

## 🛠 Tech Stack

- **Core**: C99 (Cross-platform)
- **GUI**: Nuklear (Immediate-mode)
- **Graphics**: SDL2 (Hardware accelerated)
- **Scripting**: Lua 5.4 (Plugins & Formulas)
- **Formula Engine**: TinyExpr (High-performance C parser)

## 🚀 Getting Started

### Prerequisites

- GCC / Clang
- SDL2 development libraries
- Lua 5.4 development libraries

### Building

```bash
make clean && make
```

### Running

```bash
./aslan
```

## ⌨️ Controls

- **Pan/Zoom**: Use Space + Drag or Mouse Wheel.
- **Selection**: Click blocks to activate; use handles to drag/reorder.
- **Formulas**: Start a cell with `=` in a formula column to use TinyExpr.
- **Shortcuts**: `Cmd+Z` (Undo), `Cmd+Shift+Z` (Redo).

## 📄 Persistence Model

Aslan uses a simple AOF-style log (`history.log`). Every interaction is logged as a replayable command, ensuring your data is safe and every action is reversible.

---
*Aslan is a prototype in active development.*
