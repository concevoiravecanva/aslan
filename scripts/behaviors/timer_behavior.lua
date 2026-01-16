-- =============================================================================
-- TIMER BEHAVIOR
-- =============================================================================
-- Makes a node pulse/animate based on time.
-- Demonstrates the on_update callback for continuous animations.
--
-- Usage:
--   aslan.set_behavior(node_id, "scripts/behaviors/timer_behavior.lua")

aslan.log("Timer Behavior module loaded!")

-- Timer state per node
local timer_states = {}

local function get_timer_state(node_id)
    if not timer_states[node_id] then
        -- Get initial position
        local x, y, w, h = aslan.get_node_rect(node_id)
        timer_states[node_id] = {
            elapsed = 0,
            base_x = x or 0,
            base_y = y or 0,
            base_w = w or 100,
            base_h = h or 100,
            amplitude = 20,
            frequency = 2.0 -- Hz
        }
    end
    return timer_states[node_id]
end

-- Update callback - called every frame
function on_update(node_id, dt)
    local state = get_timer_state(node_id)
    state.elapsed = state.elapsed + dt
    
    -- Calculate oscillation
    local phase = state.elapsed * state.frequency * math.pi * 2
    local offset_x = math.sin(phase) * state.amplitude
    local offset_y = math.cos(phase) * state.amplitude * 0.5
    
    -- Apply position offset (volatile - not saved to history)
    local new_x = state.base_x + offset_x
    local new_y = state.base_y + offset_y
    
    aslan.set_node_pos(node_id, new_x, new_y)
end

-- Click to reset animation
function on_click(node_id, x, y)
    local state = get_timer_state(node_id)
    state.elapsed = 0
    
    -- Update base position to current click
    state.base_x = x - state.amplitude
    state.base_y = y - state.amplitude
    
    aslan.log("Timer reset for node " .. node_id)
end
