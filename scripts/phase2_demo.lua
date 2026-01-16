-- =============================================================================
-- PHASE 2 DEMO: THE SCRIPTING EXPLOSION
-- =============================================================================
-- This plugin demonstrates all Phase 2 features:
-- 1. Extended Nuklear-Lua Bridge (widgets, drawing, layouts)
-- 2. Hot-Reload (modify this file while running!)
-- 3. Node Behaviors (interactive nodes)
-- 4. Mini-App Architecture (custom node renderers)

aslan.log("Phase 2 Demo loaded! Modify this file for hot-reload.")

-- =============================================================================
-- STATE
-- =============================================================================

local demo_state = {
    show_window = true,
    show_drawing_demo = false,
    show_widget_demo = false,
    show_behavior_panel = false,
    
    -- Widget demo state
    slider_value = 50,
    checkbox_value = false,
    combo_selected = 1,
    combo_items = {"Option A", "Option B", "Option C", "Option D"},
    text_input = "Type here...",
    color_r = 100,
    color_g = 150,
    color_b = 200,
    progress = 0,
    
    -- Drawing demo state
    draw_x = 400,
    draw_y = 300,
    
    -- Behavior tracking
    behavior_node_id = nil
}

-- =============================================================================
-- MAIN DEMO WINDOW
-- =============================================================================

function on_ui()
    if not demo_state.show_window then return end
    
    local flags = nk.WINDOW_BORDER | nk.WINDOW_TITLE | nk.WINDOW_MOVABLE | nk.WINDOW_SCALABLE
    
    if nk.begin("Phase 2 Demo", 10, 10, 300, 400, flags) then
        -- Title
        nk.layout_row_dynamic(30, 1)
        nk.label("🚀 Phase 2: Scripting Explosion", nk.TEXT_CENTERED)
        
        nk.layout_row_dynamic(20, 1)
        nk.label("Hot-reload active!", nk.TEXT_CENTERED)
        
        nk.spacing(1)
        
        -- Demo sections
        nk.layout_row_dynamic(30, 1)
        if nk.button("Widget Demo") then
            demo_state.show_widget_demo = not demo_state.show_widget_demo
        end
        
        if nk.button("Drawing Demo") then
            demo_state.show_drawing_demo = not demo_state.show_drawing_demo
        end
        
        if nk.button("Behavior Panel") then
            demo_state.show_behavior_panel = not demo_state.show_behavior_panel
        end
        
        nk.spacing(1)
        
        -- Quick actions
        nk.layout_row_dynamic(25, 1)
        nk.label("Quick Actions:", nk.TEXT_LEFT)
        
        nk.layout_row_dynamic(30, 2)
        if nk.button("Create Rect") then
            local x = math.random(100, 600)
            local y = math.random(100, 400)
            aslan.execute(string.format("new_rect %d %d 80 60", x, y))
        end
        
        if nk.button("Create Grid") then
            local x = math.random(100, 400)
            local y = math.random(100, 300)
            aslan.execute(string.format("create_grid 0 Grid %d %d 300 200 10 5", x, y))
        end
        
        if nk.button("Create Clip") then
            local x = math.random(100, 400)
            local y = math.random(100, 300)
            aslan.execute(string.format("create_clip 0 Timeline %d %d 400 150", x, y))
        end
        
        if nk.button("Reload Scripts") then
            aslan.reload()
        end
        
        nk.spacing(1)
        
        -- Info
        nk.layout_row_dynamic(20, 1)
        local node_count = #(aslan.get_all_nodes() or {})
        nk.label("Nodes: " .. node_count, nk.TEXT_LEFT)
        
        local selected = aslan.get_selected_id()
        if selected then
            nk.label("Selected: " .. selected, nk.TEXT_LEFT)
            local node_type = aslan.get_node_type(selected)
            nk.label("Type: " .. (node_type or "?"), nk.TEXT_LEFT)
        else
            nk.label("Selected: none", nk.TEXT_LEFT)
        end
    end
    nk.nk_end()
    
    -- Sub-windows
    if demo_state.show_widget_demo then
        render_widget_demo()
    end
    
    if demo_state.show_drawing_demo then
        render_drawing_demo()
    end
    
    if demo_state.show_behavior_panel then
        render_behavior_panel()
    end
    
    -- Update progress animation
    demo_state.progress = (demo_state.progress + 1) % 101
end

-- =============================================================================
-- WIDGET DEMO
-- =============================================================================

function render_widget_demo()
    local flags = nk.WINDOW_BORDER | nk.WINDOW_TITLE | nk.WINDOW_MOVABLE | nk.WINDOW_CLOSABLE
    
    if nk.begin("Widget Demo", 320, 10, 280, 450, flags) then
        -- Sliders
        nk.layout_row_dynamic(25, 1)
        nk.label("Sliders:", nk.TEXT_LEFT)
        
        nk.layout_row_dynamic(25, 1)
        demo_state.slider_value = nk.slider_int(0, demo_state.slider_value, 100, 1)
        nk.label("Value: " .. demo_state.slider_value, nk.TEXT_LEFT)
        
        nk.spacing(1)
        
        -- Checkbox
        nk.layout_row_dynamic(25, 1)
        demo_state.checkbox_value = nk.check("Enable feature", demo_state.checkbox_value)
        
        nk.spacing(1)
        
        -- Combo box
        nk.layout_row_dynamic(25, 1)
        nk.label("Combo Box:", nk.TEXT_LEFT)
        demo_state.combo_selected = nk.combo(demo_state.combo_items, demo_state.combo_selected, 25, 200, 120)
        nk.label("Selected: " .. demo_state.combo_items[demo_state.combo_selected], nk.TEXT_LEFT)
        
        nk.spacing(1)
        
        -- Text input
        nk.layout_row_dynamic(25, 1)
        nk.label("Text Input:", nk.TEXT_LEFT)
        demo_state.text_input = nk.edit_string(demo_state.text_input)
        
        nk.spacing(1)
        
        -- Progress bar
        nk.layout_row_dynamic(25, 1)
        nk.label("Progress:", nk.TEXT_LEFT)
        nk.progress(demo_state.progress, 100, false)
        
        nk.spacing(1)
        
        -- Color picker
        nk.layout_row_dynamic(25, 1)
        nk.label("Color Picker:", nk.TEXT_LEFT)
        
        nk.layout_row_dynamic(150, 1)
        demo_state.color_r, demo_state.color_g, demo_state.color_b, _ = 
            nk.color_picker(demo_state.color_r, demo_state.color_g, demo_state.color_b, 255, nk.RGB)
        
        nk.layout_row_dynamic(30, 1)
        nk.button_color(demo_state.color_r, demo_state.color_g, demo_state.color_b, 255)
    end
    nk.nk_end()
end

-- =============================================================================
-- DRAWING DEMO
-- =============================================================================

function render_drawing_demo()
    local flags = nk.WINDOW_BORDER | nk.WINDOW_TITLE | nk.WINDOW_MOVABLE
    
    if nk.begin("Drawing Demo", 610, 10, 300, 300, flags) then
        nk.layout_row_dynamic(25, 1)
        nk.label("Custom Drawing Primitives", nk.TEXT_CENTERED)
        
        -- Get window position for drawing
        local wx, wy, ww, wh = nk.window_get_content_region()
        local cx = wx + 150
        local cy = wy + 150
        
        -- Draw various shapes
        local t = aslan.get_time()
        
        -- Animated circle
        local circle_x = cx + math.sin(t * 2) * 50
        local circle_y = cy + math.cos(t * 2) * 30
        nk.fill_circle(circle_x - 20, circle_y - 20, 40, 40, 
            demo_state.color_r, demo_state.color_g, demo_state.color_b, 200)
        
        -- Rotating lines
        for i = 0, 5 do
            local angle = t + (i * math.pi / 3)
            local lx = cx + math.cos(angle) * 80
            local ly = cy + math.sin(angle) * 80
            nk.stroke_line(cx, cy, lx, ly, 2, 255, 255, 255, 100)
        end
        
        -- Rectangle outline
        nk.stroke_rect(cx - 60, cy - 60, 120, 120, 8, 2, 100, 200, 255, 255)
        
        -- Triangle
        local tri_y = cy + 90
        nk.fill_triangle(
            cx - 30, tri_y + 30,
            cx + 30, tri_y + 30,
            cx, tri_y,
            255, 150, 100, 200
        )
    end
    nk.nk_end()
end

-- =============================================================================
-- BEHAVIOR PANEL
-- =============================================================================

function render_behavior_panel()
    local flags = nk.WINDOW_BORDER | nk.WINDOW_TITLE | nk.WINDOW_MOVABLE | nk.WINDOW_CLOSABLE
    
    if nk.begin("Node Behaviors", 320, 470, 280, 200, flags) then
        local selected = aslan.get_selected_id()
        
        nk.layout_row_dynamic(25, 1)
        if selected then
            nk.label("Selected Node: " .. selected, nk.TEXT_LEFT)
            
            local behavior = aslan.get_behavior(selected)
            if behavior then
                nk.label("Behavior: " .. behavior, nk.TEXT_LEFT)
                
                nk.layout_row_dynamic(30, 1)
                if nk.button("Remove Behavior") then
                    aslan.remove_behavior(selected)
                end
            else
                nk.label("No behavior attached", nk.TEXT_LEFT)
                
                nk.spacing(1)
                nk.layout_row_dynamic(30, 1)
                nk.label("Attach Behavior:", nk.TEXT_LEFT)
                
                nk.layout_row_dynamic(30, 1)
                if nk.button("Button Behavior") then
                    aslan.set_behavior(selected, "scripts/behaviors/button_behavior.lua")
                end
                
                if nk.button("Timer Behavior") then
                    aslan.set_behavior(selected, "scripts/behaviors/timer_behavior.lua")
                end
            end
        else
            nk.label("Select a node first", nk.TEXT_CENTERED)
        end
    end
    nk.nk_end()
end

-- =============================================================================
-- EVENT HANDLERS
-- =============================================================================

-- Called when a node is clicked
function on_node_click(node_id, x, y)
    aslan.log("Node " .. node_id .. " clicked at " .. x .. ", " .. y)
end

-- Called on key press
function on_key(key, mods)
    -- F5 to reload
    if key == 1073741886 then -- F5
        aslan.reload()
    end
    
    -- F1 to toggle demo window
    if key == 1073741882 then -- F1
        demo_state.show_window = not demo_state.show_window
    end
end
