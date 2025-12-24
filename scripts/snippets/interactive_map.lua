-- Interactive Map (Aslan snippet)
-- Concept: a clickable floorplan.
-- Current bindings don't expose image nodes yet; this creates a rectangle placeholder.
-- Later: add an image node + hit-testing bindings.

aslan.execute('new_rect 40 80 640 360')
aslan.execute('new_text 50 90 240 30 "Plan (placeholder)"')
