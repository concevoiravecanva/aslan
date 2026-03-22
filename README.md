# Aslan 🦁

Aslan is a lightweight, high-performance document engine written in C99, featuring a unique **Hybrid Split-View** architecture. It combines the power of structured data (Airtable) with the freedom of vector canvases (Canva/Miro) in a unified, block-based environment.

---

## 🚀 Key Features

### 📐 Hybrid Split-View Editor
The primary interface is a draggable split-view that allows you to work across different paradigms simultaneously:
- **Draggable Splitter**: Seamlessly adjust the ratio between left and right panes.
- **Layout Presets**: Double-click the splitter to cycle between 50/50, Max-Left (3/97), and Max-Right (97/3) modes.
- **Universal View Switching**: Toggle either pane between **Table**, **Design**, **Canvas**, **Kanban**, and **Calendar** views.

### 📊 Columnar Table Engine
A high-performance grid system designed for data density and speed:
- **Column-Store Architecture**: Data is stored contiguously per column, allowing for efficient memory access and SIMD optimizations.
- **Dual Formula Engine**: 
  - **TinyExpr**: Integrated C parser for high-performance math (`=Price * 1.2`).
  - **Lua Scripting**: Advanced logic and data manipulation using Lua 5.4.
- **Unique Naming**: Automatic generation of unique column names to prevent collisions.

### 🎨 Vector Node System
Used in both **Canvas** (infinite) and **Design** (finite/multi-page) blocks:
- **Vector Primitives**: Lines, Rects, Ellipses, Paths, and custom Strokes.
- **Node-Based Architecture**: Hierarchical tree-based rendering with spatial indexing for fast hit-testing.
- **Infinite Space**: Miro-style infinite whiteboarding for brainstorming and diagrams.

### 💾 Robust Persistence (AOF)
- **Append-Only History**: Every interaction is logged as a replayable command in `history.log`.
- **Infinite Undo/Redo**: Full state recovery and backtracking via command inverse logic.
- **Crash Recovery**: Never lose data; the state is replayed from the log upon startup.

---

## 🛠 Technical Stack

Aslan is built for speed and portability:
- **Core Engine**: C99 (Cross-platform)
- **Graphics**: SDL2 (Hardware accelerated)
- **GUI**: NanoVG & Clay (Vector-based immediate-mode GUI)
- **Scripting**: Lua 5.4 (Sandbox implementation)
- **Formulas**: TinyExpr (Expression evaluator)
- **OS Integration**: Native macOS file dialogs via `osascript`.

---

## 🚀 Getting Started

### Prerequisites
- **macOS**: `brew install sdl2 lua`
- **Linux**: `apt install libsdl2-dev liblua5.4-dev`
- **Compiler**: GCC or Clang

### Build & Run
```bash
make clean && make
./aslan
```

---

## ⌨️ Controls & Navigation

- **Split View Control**: Drag the center bar to resize. Double-click to toggle view modes.
- **Pan / Zoom**: `Space + Drag` (Canvas/Design) or `Mouse Wheel`.
- **Keyboard Shortcuts**:
  - `Cmd+Z`: Undo
  - `Cmd+Shift+Z`: Redo
  - `Cmd+S`: Save Snapshot

---
*Aslan is an open-source prototype targeting performance and creative control.*
