-- =============================================================================
-- BUTTON BEHAVIOR
-- =============================================================================
-- Attach this behavior to a node to make it act as a clickable button.
-- 
-- Usage from another script:
--   aslan.set_behavior(node_id, "scripts/behaviors/button_behavior.lua")
--
-- Or manually define callbacks for specific nodes:
--   function on_click_42(x, y)
--     aslan.log("Button 42 clicked at " .. x .. ", " .. y)
--   end

aslan.log("Button Behavior module loaded!")

-- State for tracked buttons
local button_states = {}

local function get_button_state(node_id)
    if not button_states[node_id] then
        button_states[node_id] = {
            hover = false,
            pressed = false,
            click_count = 0,
            last_click_time = 0
        }
    end
    return button_states[node_id]
end

-- Global click handler for button nodes
function on_click(node_id, x, y)
    local state = get_button_state(node_id)
    state.click_count = state.click_count + 1
    state.last_click_time = aslan.get_time()
    
    aslan.log(string.format("Button %d clicked! (count: %d)", node_id, state.click_count))
    
    -- Example: create a new rectangle when clicked
    if state.click_count % 5 == 0 then
        local nx, ny = aslan.get_node_rect(node_id)
        aslan.execute(string.format("new_rect %f %f 50 50", nx + 100, ny))
        aslan.log("Spawned new rectangle!")
    end
end

-- Global hover handler
function on_hover(node_id)
    local state = get_button_state(node_id)
    if not state.hover then
        state.hover = true
        aslan.log("Hovering over node " .. node_id)
    end
end

-- Update handler (called each frame for nodes with behaviors)
function on_update(node_id, dt)
    local state = get_button_state(node_id)
    
    -- Reset hover state (will be set again if still hovering)
    state.hover = false
    
    -- Could add animations here based on dt
end
