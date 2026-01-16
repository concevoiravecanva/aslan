-- scripts/plugin_demo.lua
aslan.log("Plugin Demo Loaded!")

local show_demo = true
local rect_count = 0

function on_ui()
    if not show_demo then return end
    
    -- nk.begin(title, x, y, w, h, flags)
    if nk.begin("Lua Plugin Demo", 10, 100, 220, 150, nk.WINDOW_BORDER | nk.WINDOW_TITLE | nk.WINDOW_MOVABLE | nk.WINDOW_CLOSABLE) then
        nk.layout_row_dynamic(25, 1)
        nk.label("Hello from Lua Plugin!", nk.TEXT_LEFT)
        
        nk.layout_row_dynamic(30, 1)
        if nk.button("Create Random Rect") then
            rect_count = rect_count + 1
            local x = math.random(250, 700)
            local y = math.random(100, 500)
            local w = math.random(50, 150)
            local h = math.random(50, 150)
            aslan.execute(string.format("new_rect %d %d %d %d", x, y, w, h))
            aslan.log("Lua created rect #" .. rect_count)
        end
        
        nk.layout_row_dynamic(20, 1)
        nk.label("Rects created: " .. rect_count, nk.TEXT_LEFT)
    end
    nk.nk_end()
end
