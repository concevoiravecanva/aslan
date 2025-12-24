-- Live Clock (Aslan snippet)
-- Concept: show a live clock.
-- Current bindings don't expose timers, so we create a placeholder text.
-- Once Aslan exposes time/os APIs, you can update it periodically via set_text.

aslan.execute('new_text 40 40 220 40 "00:00:00"')
