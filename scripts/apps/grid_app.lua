-- =============================================================================
-- GRID MINI-APP
-- =============================================================================
-- A Lua-driven spreadsheet renderer for NODE_TYPE_GRID nodes.
-- This replaces the hardcoded C grid rendering with a flexible Lua implementation.
--
-- Register with: aslan.register_app(aslan.NODE_GRID, "render_grid_app")

aslan.log("Grid Mini-App loaded!")

-- Grid app state (per-node state stored here)
local grid_states = {}

local function get_grid_state(node_id)
    if not grid_states[node_id] then
        grid_states[node_id] = {
            scroll_x = 0,
            scroll_y = 0,
            selected_row = 0,
            selected_col = 0,
            editing = false,
            edit_text = ""
        }
    end
    return grid_states[node_id]
end

-- Configuration
local CELL_WIDTH = 80
local CELL_HEIGHT = 24
local HEADER_HEIGHT = 28
local ROW_HEADER_WIDTH = 40
local GRID_ROWS = 20
local GRID_COLS = 10

-- Color palette (dark theme)
local COLORS = {
    bg = {30, 32, 36, 255},
    header_bg = {45, 48, 52, 255},
    cell_bg = {38, 40, 44, 255},
    cell_selected_bg = {60, 80, 120, 255},
    border = {55, 58, 62, 255},
    text = {220, 220, 220, 255},
    header_text = {160, 165, 175, 255}
}

-- Convert column index to letter (0 = A, 1 = B, etc.)
local function col_to_letter(col)
    if col < 26 then
        return string.char(65 + col)
    else
        return string.char(64 + math.floor(col / 26)) .. string.char(65 + (col % 26))
    end
end

-- Render the grid app
function render_grid_app(node_id, x, y, w, h)
    local state = get_grid_state(node_id)
    
    -- Create window for grid
    local win_flags = nk.WINDOW_BORDER | nk.WINDOW_NO_SCROLLBAR
    local title = "Grid#" .. node_id
    
    if nk.begin(title, x, y, w, h, win_flags) then
        -- Calculate visible area
        local content_x, content_y, content_w, content_h = nk.window_get_content_region()
        
        local visible_cols = math.floor((content_w - ROW_HEADER_WIDTH) / CELL_WIDTH) + 1
        local visible_rows = math.floor((content_h - HEADER_HEIGHT) / CELL_HEIGHT) + 1
        
        -- Use layout space for absolute positioning
        nk.layout_space_begin(nk.STATIC, content_h, 1000)
        
        -- Draw column headers
        for col = 0, math.min(visible_cols, GRID_COLS - 1) do
            local cx = ROW_HEADER_WIDTH + col * CELL_WIDTH - state.scroll_x
            if cx >= ROW_HEADER_WIDTH - CELL_WIDTH and cx < content_w then
                nk.layout_space_push(cx, 0, CELL_WIDTH, HEADER_HEIGHT)
                nk.label(col_to_letter(col), nk.TEXT_CENTERED)
            end
        end
        
        -- Draw row headers and cells
        for row = 0, math.min(visible_rows, GRID_ROWS - 1) do
            local ry = HEADER_HEIGHT + row * CELL_HEIGHT - state.scroll_y
            if ry >= HEADER_HEIGHT - CELL_HEIGHT and ry < content_h then
                -- Row header
                nk.layout_space_push(0, ry, ROW_HEADER_WIDTH, CELL_HEIGHT)
                nk.label(tostring(row + 1), nk.TEXT_CENTERED)
                
                -- Cells
                for col = 0, math.min(visible_cols, GRID_COLS - 1) do
                    local cx = ROW_HEADER_WIDTH + col * CELL_WIDTH - state.scroll_x
                    if cx >= ROW_HEADER_WIDTH - CELL_WIDTH and cx < content_w then
                        nk.layout_space_push(cx, ry, CELL_WIDTH, CELL_HEIGHT)
                        
                        local is_selected = (row == state.selected_row and col == state.selected_col)
                        
                        if is_selected then
                            nk.style_push_button_color(
                                COLORS.cell_selected_bg[1],
                                COLORS.cell_selected_bg[2],
                                COLORS.cell_selected_bg[3],
                                COLORS.cell_selected_bg[4]
                            )
                        end
                        
                        -- Get cell value
                        local cell_value = aslan.grid_node_get(node_id, row, col) or ""
                        
                        -- Show as button for selection, or as edit field if editing
                        if is_selected and state.editing then
                            state.edit_text, flags = nk.edit_string(state.edit_text, nk.EDIT_FIELD | nk.EDIT_SIG_ENTER)
                            if flags and (flags & nk.EDIT_COMMITTED) ~= 0 then
                                aslan.grid_node_set(node_id, row, col, state.edit_text)
                                state.editing = false
                            end
                        else
                            if nk.button(cell_value) then
                                state.selected_row = row
                                state.selected_col = col
                                state.editing = true
                                state.edit_text = cell_value
                            end
                        end
                        
                        if is_selected then
                            nk.style_pop_color()
                        end
                    end
                end
            end
        end
        
        nk.layout_space_end()
    end
    nk.nk_end()
end

-- Register this app for grid nodes
aslan.register_app(aslan.NODE_GRID, "render_grid_app")

-- Keyboard navigation behavior
function on_key(key, mods)
    -- Arrow key navigation for selected grid
    local selected_id = aslan.get_selected_id()
    if not selected_id then return end
    
    local node_type = aslan.get_node_type(selected_id)
    if node_type ~= aslan.NODE_GRID then return end
    
    local state = get_grid_state(selected_id)
    
    -- Arrow keys (SDL keycodes: left=1073741904, right=1073741903, up=1073741906, down=1073741905)
    if key == 1073741904 then -- Left
        state.selected_col = math.max(0, state.selected_col - 1)
    elseif key == 1073741903 then -- Right
        state.selected_col = math.min(GRID_COLS - 1, state.selected_col + 1)
    elseif key == 1073741906 then -- Up
        state.selected_row = math.max(0, state.selected_row - 1)
    elseif key == 1073741905 then -- Down
        state.selected_row = math.min(GRID_ROWS - 1, state.selected_row + 1)
    elseif key == 13 then -- Enter
        state.editing = true
        state.edit_text = aslan.grid_node_get(selected_id, state.selected_row, state.selected_col) or ""
    elseif key == 27 then -- Escape
        state.editing = false
    end
end
