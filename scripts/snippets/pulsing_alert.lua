-- Pulsing Alert (Aslan snippet)
-- Concept: animate a warning label.
-- Current Aslan Lua bindings do not expose per-frame draw calls or time.
-- This snippet creates a TEXT node that you can later animate once a frame API exists.

aslan.execute('new_text 60 60 360 60 "URGENT"')
