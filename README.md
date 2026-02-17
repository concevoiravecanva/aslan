# Aslan 🦁

Aslan is a lightweight, high-performance C/SDL2 document engine that blends the structured simplicity of **Notion**, the columnar power of **Airtable**, and the visual freedom of **Canva**. 

Designed for 2024, Aslan prioritizes a "Warm White" aesthetic, generous margins, and a frictionless experience where you start writing immediately.

---

## ✨ Features (MVP)

### 🎨 Modern Aesthetic
- **Warm White Canvas**: Background set to `#FCFCFA` to reduce eye strain.
- **Apple Notes Layout**: Symmetric 120px margins and 80px top breathing room.
- **Polished Typography**: High contrast headings (56px) and comfortable line height (28px).
- **iOS-style UX**: Cyan focus rings (`#007AFF`) and smooth selection.

### 🧱 Block-Based Architecture
Mix and match different content types seamlessly:
- **Text & H1**: Fluid text editing with proéminent headers.
- **Table (V2 Engine)**: High-performance columnar storage with automatic unique naming and Excel-like formulas.
- **Infinite Canvas**: Miro-style whiteboarding with vector nodes.
- **Finite Artboard**: Pixel-perfect artboards for layouts and designs.
- **Highlighting**: Notion-style yellow highlights (`#FFED69`) for critical notes.

### ⚙️ Engine Core
- **AOF Persistence**: Every action is saved in an Append-Only File (`history.log`) for infinite undo/redo and crash recovery.
- **Column-Store Grid**: Tables use contiguous memory per column for SIMD-ready performance.
- **Formula Engine**: Native C integration using **TinyExpr** for real-time calculations.

---

## 🛠 Technical Stack

Aslan is built with simplicity and performance in mind:
- **Language**: C99 (Cross-platform core)
- **GUI Framework**: Nuklear (Immediate-mode)
- **Rendering**: SDL2 (Hardware accelerated)
- **Logic / Scripting**: Lua 5.4 (Plugins & Advanced formulas)
- **Math Parser**: TinyExpr (High-performance C expression evaluator)

---

## 🚀 Getting Started

### Prerequisites

- **macOS**: `brew install sdl2 lua`
- **Linux**: `apt install libsdl2-dev liblua5.4-dev`
- **Compiler**: GCC or Clang

### Building & Running

```bash
make clean && make
./aslan
```

---

## ⌨️ Controls & Shortcuts

- **Pan / Zoom**: `Space + Drag` or `Mouse Wheel`.
- **Block Selection**: Click on a block to activate. Drag handles to reorder.
- **Formulas**: In a Table "Formula" column, start with `=` (e.g., `=[Price] * 1.2`).
- **History**: `Cmd+Z` (Undo), `Cmd+Shift+Z` (Redo).

---

## 📄 Repository Structure

- `/src`: Core C implementation.
  - `/platform`: SDL2, Nuklear, and OS-specific rendering logic.
  - `/document`: Data models for pages, blocks, and the columnar engine.
  - `/scripting`: Lua host and bindings.
- `/scripts`: Built-in Lua plugins and tools.
- `/vendor`: Lightweight dependencies (Nuklear, TinyExpr).

---
*Aslan is an open-source prototype in active development. Created for speed, beauty, and control.*
