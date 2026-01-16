-- =============================================================================
-- CLIP MINI-APP
-- =============================================================================
-- A Lua-driven video clip timeline editor for NODE_TYPE_CLIP nodes.
-- This implements a mini timeline with clip management inside the Aslan canvas.
--
-- Register with: aslan.register_app(aslan.NODE_CLIP, "render_clip_app")

aslan.log("Clip Mini-App loaded!")

-- Clip app state
local clip_states = {}

local function get_clip_state(node_id)
    if not clip_states[node_id] then
        clip_states[node_id] = {
            playhead = 0.0,
            is_playing = false,
            zoom = 1.0,
            scroll_x = 0,
            selected_clip = -1,
            clips = {
                -- Demo clips for visualization
                { name = "Intro", start_time = 0.0, end_time = 5.0, color = {100, 150, 255} },
                { name = "Scene 1", start_time = 5.0, end_time = 15.0, color = {150, 200, 100} },
                { name = "Scene 2", start_time = 15.0, end_time = 25.0, color = {200, 150, 100} },
            }
        }
    end
    return clip_states[node_id]
end

-- Configuration
local TIMELINE_HEIGHT = 60
local HEADER_HEIGHT = 30
local TRACK_HEIGHT = 40
local PIXELS_PER_SECOND = 20

-- Color palette
local COLORS = {
    bg = {25, 28, 32, 255},
    timeline_bg = {35, 38, 42, 255},
    playhead = {255, 80, 80, 255},
    ruler = {80, 85, 95, 255},
    ruler_text = {140, 145, 155, 255}
}

-- Render the clip timeline app
function render_clip_app(node_id, x, y, w, h)
    local state = get_clip_state(node_id)
    local pps = PIXELS_PER_SECOND * state.zoom
    
    local win_flags = nk.WINDOW_BORDER | nk.WINDOW_NO_SCROLLBAR
    local title = "Timeline#" .. node_id
    
    if nk.begin(title, x, y, w, h, win_flags) then
        local cx, cy, cw, ch = nk.window_get_content_region()
        
        -- Transport controls
        nk.layout_row_dynamic(30, 5)
        
        if nk.button(state.is_playing and "⏸" or "▶") then
            state.is_playing = not state.is_playing
        end
        
        if nk.button("⏹") then
            state.is_playing = false
            state.playhead = 0
        end
        
        if nk.button("⏮") then
            state.playhead = 0
        end
        
        if nk.button("−") then
            state.zoom = math.max(0.25, state.zoom * 0.8)
        end
        
        if nk.button("+") then
            state.zoom = math.min(4.0, state.zoom * 1.25)
        end
        
        -- Time display
        nk.layout_row_dynamic(25, 1)
        local time_str = string.format("Time: %.2fs | Zoom: %.0f%%", state.playhead, state.zoom * 100)
        nk.label(time_str, nk.TEXT_CENTERED)
        
        -- Timeline area with custom drawing
        nk.layout_space_begin(nk.STATIC, ch - 70, 100)
        
        -- Draw timeline background
        local tl_x = x + 10
        local tl_y = y + 65
        local tl_w = w - 20
        local tl_h = ch - 80
        
        nk.fill_rect(tl_x, tl_y, tl_w, tl_h, 4,
            COLORS.timeline_bg[1], COLORS.timeline_bg[2], 
            COLORS.timeline_bg[3], COLORS.timeline_bg[4])
        
        -- Draw time ruler
        local max_time = 60 -- 1 minute timeline
        local ruler_y = tl_y + 5
        
        for t = 0, max_time do
            local tx = tl_x + 10 + (t * pps) - state.scroll_x
            if tx >= tl_x and tx <= tl_x + tl_w then
                -- Major tick every 5 seconds
                if t % 5 == 0 then
                    nk.stroke_line(tx, ruler_y, tx, ruler_y + 15, 1,
                        COLORS.ruler[1], COLORS.ruler[2], COLORS.ruler[3], COLORS.ruler[4])
                else
                    nk.stroke_line(tx, ruler_y, tx, ruler_y + 8, 1,
                        COLORS.ruler[1], COLORS.ruler[2], COLORS.ruler[3], 128)
                end
            end
        end
        
        -- Draw clips
        local track_y = tl_y + 30
        
        for i, clip in ipairs(state.clips) do
            local clip_x = tl_x + 10 + (clip.start_time * pps) - state.scroll_x
            local clip_w = (clip.end_time - clip.start_time) * pps
            
            if clip_x + clip_w >= tl_x and clip_x <= tl_x + tl_w then
                local is_selected = (i == state.selected_clip)
                local alpha = is_selected and 255 or 200
                
                -- Clip rectangle
                nk.fill_rect(clip_x, track_y, clip_w, TRACK_HEIGHT, 4,
                    clip.color[1], clip.color[2], clip.color[3], alpha)
                
                -- Selection border
                if is_selected then
                    nk.stroke_rect(clip_x, track_y, clip_w, TRACK_HEIGHT, 4, 2,
                        255, 255, 255, 255)
                end
                
                -- Check for click on clip
                if nk.input_mouse_click_in_rect(nk.BUTTON_LEFT, 
                    clip_x, track_y, clip_w, TRACK_HEIGHT) then
                    state.selected_clip = i
                end
            end
        end
        
        -- Draw playhead
        local playhead_x = tl_x + 10 + (state.playhead * pps) - state.scroll_x
        if playhead_x >= tl_x and playhead_x <= tl_x + tl_w then
            nk.stroke_line(playhead_x, ruler_y, playhead_x, track_y + TRACK_HEIGHT, 2,
                COLORS.playhead[1], COLORS.playhead[2], COLORS.playhead[3], COLORS.playhead[4])
            -- Playhead triangle
            nk.fill_triangle(
                playhead_x - 6, ruler_y,
                playhead_x + 6, ruler_y,
                playhead_x, ruler_y + 10,
                COLORS.playhead[1], COLORS.playhead[2], COLORS.playhead[3], COLORS.playhead[4])
        end
        
        nk.layout_space_end()
    end
    nk.nk_end()
    
    -- Update playhead if playing
    if state.is_playing then
        state.playhead = state.playhead + 0.016 -- ~60fps
        
        -- Find max end time
        local max_end = 0
        for _, clip in ipairs(state.clips) do
            max_end = math.max(max_end, clip.end_time)
        end
        
        if state.playhead > max_end then
            state.playhead = 0
        end
    end
end

-- Register this app for clip nodes
aslan.register_app(aslan.NODE_CLIP, "render_clip_app")

-- Click handler for clip timeline
function on_click_clip(node_id, x, y)
    local state = get_clip_state(node_id)
    -- Could implement scrubbing here
end
