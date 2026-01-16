-- scripts/tools.lua
aslan.log("Tools script loaded (Command-Log version)!")

-- Quick GRID debug (startup): lets you verify `aslan.grid_get("A1")` in the terminal.
-- Put some text in cell A1 of a GRID page, restart the app, and you should see it here.
do
    local pid = aslan.get_active_page_id and aslan.get_active_page_id() or nil
    local pt = (pid and aslan.get_page_type) and aslan.get_page_type(pid) or nil
    local v = (aslan.grid_get and aslan.grid_get("A1")) or nil
    aslan.log("[GRID] active_page=" .. tostring(pid) .. " type=" .. tostring(pt) .. " A1=" .. tostring(v))
end

-- SDL: KMOD_LSHIFT=0x0001, KMOD_RSHIFT=0x0002, KMOD_SHIFT=(L|R)=0x0003
local KMOD_SHIFT = 0x0003
local KMOD_CTRL  = 0x0040
local KMOD_ALT   = 0x0100
local KMOD_GUI   = 0x0400 -- Command on macOS

local dragged_node_id = nil
local multi_selection = {} -- Set of selected IDs: { [id] = true }
local drag_mode = "none" -- "move" or "resize"
local resize_corner = nil -- "tl"|"tr"|"bl"|"br"
local offset_x = 0
local offset_y = 0
local offset_w = 0
local offset_h = 0
local drag_start_mx = 0
local drag_start_my = 0

-- Dernière géométrie calculée pendant le drag (commit au mouse_up en une seule commande AOF)
local pending_x = nil
local pending_y = nil
local pending_w = nil
local pending_h = nil

-- Tool mode
-- 0 = select/move/resize
-- 1 = create rect (draw)
local TOOL_SELECT = 0
local TOOL_CREATE_RECT = 1
local TOOL_PAN = 2

-- Pan state (camera drag)
local pan_active = false
local pan_start_mx = 0
local pan_start_my = 0
local pan_start_sx = 0
local pan_start_sy = 0
local pan_start_vx = 0
local pan_start_vy = 0
local pan_start_vz = 1.0
local pan_pid = nil
local pan_pending_vx = nil
local pan_pending_vy = nil

-- Draw-rect state
local draw_active = false
local draw_start_x = 0
local draw_start_y = 0
local draw_preview_id = nil

local function current_tool_mode()
    local m = aslan.get_tool_mode()
    if not m then return TOOL_SELECT end
    return m
end

local function rect_from_points(x0, y0, x1, y1)
    local rx = math.min(x0, x1)
    local ry = math.min(y0, y1)
    local rw = math.abs(x1 - x0)
    local rh = math.abs(y1 - y0)
    return rx, ry, rw, rh
end

local function parse_bool(v)
    if not v then return false end
    if v == true then return true end
    v = tostring(v)
    v = string.lower(v)
    return (v == "1" or v == "true" or v == "yes" or v == "on")
end

-- NOTE: Le bridge Lua actuel n'expose pas (encore) une API `get_global()`.
-- Pour garder le drag fonctionnel, le snapping preview est désactivé par défaut.
-- On peut réactiver proprement quand on aura ajouté `aslan.get_global` côté C.
local function snap_enabled()
    return false
end

local function snap_step()
    return 10
end

local function snap_value(v, step)
    if not step or step <= 0 then return v end
    return math.floor((v / step) + 0.5) * step
end

local function snap_rect(x, y, w, h)
    if not snap_enabled() then return x, y, w, h end
    local step = snap_step()
    return snap_value(x, step), snap_value(y, step), snap_value(w, step), snap_value(h, step)
end

-- Mouse wheel zoom
-- C bridge currently provides 2 floats to event callbacks.
-- We use convention: on_mouse_wheel(delta, mx) (my isn't available yet).
--
-- Sane defaults:
-- - zoom starts at 1.0
-- - per notch: 1.12x (smooth but noticeable)
-- - clamp: [0.10, 12.0]
--
-- Coordinate convention (matches C side):
--   screen = (world - view) * zoom + center
-- With missing my, we keep X under cursor stable and keep Y centered.
function on_mouse_wheel(delta, mx)
    if not delta or delta == 0 then return end

    local pid = aslan.get_active_page_id()
    if not pid then return end

    -- Notes pages are linear docs (Notion-like), not a pan/zoom canvas.
    -- Ignore zoom on notes to avoid viewport spam in history.log.
    local pt = aslan.get_page_type and aslan.get_page_type(pid) or nil
    if pt == "note" or pt == 3 then
        return
    end

    local z = aslan.get_page_meta(pid, "viewport.zoom")
    local vx = aslan.get_page_meta(pid, "viewport.x")
    local vy = aslan.get_page_meta(pid, "viewport.y")
    if not z or z == 0 then z = 1.0 end
    if not vx then vx = 0.0 end
    if not vy then vy = 0.0 end

    local zoom_step = 1.12
    local min_zoom = 0.10
    local max_zoom = 12.0

    local z2 = z
    if delta > 0 then
        z2 = z * (zoom_step ^ delta)
    else
        z2 = z / (zoom_step ^ (-delta))
    end
    if z2 < min_zoom then z2 = min_zoom end
    if z2 > max_zoom then z2 = max_zoom end

    -- Keep the world X under the cursor stable.
    -- We don't have screen center or my, so we approximate center_x as 0 in this Lua layer.
    -- That still feels okay if you mostly wheel-zoom near center; later we can expose center.
    local mx0 = mx or 0.0
    local world_x = (mx0 / z) + vx
    local vx2 = world_x - (mx0 / z2)

    aslan.set_page_meta(pid, "viewport.zoom", z2)
    aslan.set_page_meta(pid, "viewport.x", vx2)
    -- keep y unchanged for now
    aslan.set_page_meta(pid, "viewport.y", vy)
end

-- Helper: Manage multi-selection
local function clear_selection()
    multi_selection = {}
    aslan.log("selection: cleared")
end

local function is_selected(id)
    return multi_selection[id] == true
end

local function selection_snapshot()
    local ids = {}
    for id, _ in pairs(multi_selection) do
        ids[#ids + 1] = tostring(id)
    end
    table.sort(ids)
    return "{" .. table.concat(ids, ",") .. "}"
end

local function select_add(id)
    if not id then return end
    multi_selection[id] = true
    aslan.log("selection: add " .. tostring(id) .. " -> " .. selection_snapshot())
end

local function select_toggle(id)
    if not id then return end
    if multi_selection[id] then
        multi_selection[id] = nil
    else
        multi_selection[id] = true
    end
    aslan.log("selection: toggle " .. tostring(id) .. " -> " .. selection_snapshot())
end

local function selection_count()
    local c = 0
    for _ in pairs(multi_selection) do c = c + 1 end
    return c
end

function group_selected()
    local count = selection_count()
    aslan.log("group_selected: count=" .. tostring(count))
    if count < 2 then
        aslan.log("Need at least 2 items to group.")
        return
    end

    -- Calculate bounding box
    local min_x, min_y = 100000, 100000
    local max_x, max_y = -100000, -100000

    for id, _ in pairs(multi_selection) do
        local r_x, r_y, r_w, r_h = aslan.get_node_rect(id)
        if r_x then
            if r_x < min_x then min_x = r_x end
            if r_y < min_y then min_y = r_y end
            if r_x + r_w > max_x then max_x = r_x + r_w end
            if r_y + r_h > max_y then max_y = r_y + r_h end
        end
    end
    
    local padding = 10
    min_x = min_x - padding
    min_y = min_y - padding
    local w = max_x - min_x + padding * 2
    local h = max_y - min_y + padding * 2
    
    -- Create group
    local gid = aslan.create_group(min_x, min_y, w, h)
    if gid then
        aslan.log("Created Group " .. gid)
        -- Make the visual group box non-intrusive for now (logical grouping only).
        aslan.execute(string.format("set_visible %u 0", gid))
        for id, _ in pairs(multi_selection) do
            aslan.group_attach(gid, id)
        end
        clear_selection()
        select_add(gid)
        aslan.execute("select " .. gid)
    end
end

function can_group_now()
    return selection_count() >= 2
end

function get_multi_selection_ids()
    local ids = {}
    for id, _ in pairs(multi_selection) do
        ids[#ids + 1] = id
    end
    table.sort(ids)
    return ids
end

function on_key_down(key, mods)
    -- 'g' is 103
    if key == 103 then
        group_selected()
    end
end

function on_mouse_down(x, y)
    -- Pan tool: drag camera
    if current_tool_mode() == TOOL_PAN then
        local pid = aslan.get_active_page_id()
        if not pid then return end

        -- Notes pages are linear docs (Notion-like), not a pan/zoom canvas.
        local pt = aslan.get_page_type and aslan.get_page_type(pid) or nil
        if pt == "note" or pt == 3 then
            return
        end

        pan_active = true
        pan_pid = pid
        pan_start_mx = x
        pan_start_my = y
        pan_start_sx = aslan.get_mouse_x() or 0
        pan_start_sy = aslan.get_mouse_y() or 0
        pan_start_vx = aslan.get_page_meta(pid, "viewport.x") or 0
        pan_start_vy = aslan.get_page_meta(pid, "viewport.y") or 0
        pan_start_vz = aslan.get_page_meta(pid, "viewport.zoom") or 1.0
        if pan_start_vz == 0 then pan_start_vz = 1.0 end
        return
    end

    -- Draw-rect tool
    if current_tool_mode() == TOOL_CREATE_RECT then
        draw_active = true
        draw_start_x = x
        draw_start_y = y
        draw_preview_id = nil
        return
    end

    local clicked_id = aslan.get_node_at(x, y)
    local mods = aslan.get_key_mods and aslan.get_key_mods() or 0
    local shift = (mods & KMOD_SHIFT) ~= 0
    
    dragged_node_id = clicked_id
    
    if clicked_id then
        if is_selected(clicked_id) and shift then
            select_toggle(clicked_id)
        else
            select_add(clicked_id)
        end
        aslan.execute("select " .. clicked_id)
    else
        if not shift then
            clear_selection()
            aslan.execute("select 0")
        end
        dragged_node_id = nil
    end

    if dragged_node_id then
        -- Only allow resize if single selection
        local can_resize = (selection_count() == 1)
        
        local nx, ny, nw, nh = aslan.get_node_rect(dragged_node_id)
        local handle = 18
        local in_tl = (x >= nx and x <= nx + handle and y >= ny and y <= ny + handle)
        local in_tr = (x >= nx + nw - handle and x <= nx + nw and y >= ny and y <= ny + handle)
        local in_bl = (x >= nx and x <= nx + handle and y >= ny + nh - handle and y <= ny + nh)
        local in_br = (x >= nx + nw - handle and x <= nx + nw and y >= ny + nh - handle and y <= ny + nh)

        if can_resize and (in_tl or in_tr or in_bl or in_br) then
            drag_mode = "resize"
            if in_tl then resize_corner = "tl"
            elseif in_tr then resize_corner = "tr"
            elseif in_bl then resize_corner = "bl"
            else resize_corner = "br" end

            if resize_corner == "tl" then
                offset_x = x - nx
                offset_y = y - ny
            elseif resize_corner == "tr" then
                offset_x = x - (nx + nw)
                offset_y = y - ny
            elseif resize_corner == "bl" then
                offset_x = x - nx
                offset_y = y - (ny + nh)
            elseif resize_corner == "br" then
                offset_x = x - (nx + nw)
                offset_y = y - (ny + nh)
            end
            aslan.log("Lua resizing node: " .. dragged_node_id .. " corner=" .. resize_corner)
        else
            drag_mode = "move"
            resize_corner = nil
            offset_x = x - nx
            offset_y = y - ny
            aslan.log("Lua dragging node: " .. dragged_node_id)
        end
    end
end

function on_mouse_move(x, y)
    if pan_active and pan_pid then
    -- Use SCREEN deltas so camera updates don’t change the basis mid-drag (prevents trembling).
    -- Convert pixels -> world units using the zoom captured at drag start.
    local sx = aslan.get_mouse_x() or 0
    local sy = aslan.get_mouse_y() or 0
    local dx_px = sx - pan_start_sx
    local dy_px = sy - pan_start_sy
    local dx_world = dx_px / pan_start_vz
    local dy_world = dy_px / pan_start_vz

    -- Dragging right should move content right (hand tool): camera moves left.
    local vx = pan_start_vx - dx_world
    local vy = pan_start_vy - dy_world

    -- For responsiveness we preview during drag, but without AOF spam.
    pan_pending_vx = vx
    pan_pending_vy = vy
    aslan.execute_volatile(string.format("page_meta %u viewport.x %f", pan_pid, vx))
    aslan.execute_volatile(string.format("page_meta %u viewport.y %f", pan_pid, vy))
        return
    end

    -- Draw-rect preview update (disabled)

    if dragged_node_id then
        if drag_mode == "move" then
            local new_x = x - offset_x
            local new_y = y - offset_y
            local _, _, w, h = aslan.get_node_rect(dragged_node_id)

            new_x, new_y, w, h = snap_rect(new_x, new_y, w, h)
            pending_x, pending_y, pending_w, pending_h = new_x, new_y, w, h

            -- Exécution volatile: pas de spam dans history.log
            aslan.execute_volatile(string.format("move %d %f %f", dragged_node_id, new_x, new_y))
        elseif drag_mode == "resize" then
            local nx, ny, nw, nh = aslan.get_node_rect(dragged_node_id)

            local min_size = 10
            local new_x, new_y, new_w, new_h = nx, ny, nw, nh

            if resize_corner == "br" then
                local corner_x = x - offset_x
                local corner_y = y - offset_y
                new_w = math.max(min_size, corner_x - nx)
                new_h = math.max(min_size, corner_y - ny)
            elseif resize_corner == "tr" then
                local corner_x = x - offset_x
                local corner_y = y - offset_y
                new_w = math.max(min_size, corner_x - nx)
                new_h = math.max(min_size, (ny + nh) - corner_y)
                new_y = (ny + nh) - new_h
            elseif resize_corner == "bl" then
                local corner_x = x - offset_x
                local corner_y = y - offset_y
                new_w = math.max(min_size, (nx + nw) - corner_x)
                new_h = math.max(min_size, corner_y - ny)
                new_x = (nx + nw) - new_w
            elseif resize_corner == "tl" then
                local corner_x = x - offset_x
                local corner_y = y - offset_y
                new_w = math.max(min_size, (nx + nw) - corner_x)
                new_h = math.max(min_size, (ny + nh) - corner_y)
                new_x = (nx + nw) - new_w
                new_y = (ny + nh) - new_h
            end

            pending_x, pending_y, pending_w, pending_h = new_x, new_y, new_w, new_h

            new_x, new_y, new_w, new_h = snap_rect(new_x, new_y, new_w, new_h)

            -- Exécution volatile pendant le drag
            aslan.execute_volatile(string.format("set_rect %d %f %f %f %f", dragged_node_id, new_x, new_y, new_w, new_h))
        end
    end
end

function on_mouse_up(x, y)
    if pan_active and pan_pid then
        -- Notes pages are linear docs (Notion-like), not a pan/zoom canvas.
        local pt = aslan.get_page_type and aslan.get_page_type(pan_pid) or nil
        if pt == "note" or pt == 3 then
            pan_active = false
            pan_pid = nil
            pan_pending_vx = nil
            pan_pending_vy = nil
            return
        end

        -- Commit a single camera update in AOF (option B: persisted, not undoable).
        if pan_pending_vx ~= nil and pan_pending_vy ~= nil then
            aslan.execute(string.format("page_meta %u viewport.x %f", pan_pid, pan_pending_vx))
            aslan.execute(string.format("page_meta %u viewport.y %f", pan_pid, pan_pending_vy))
        end
        pan_active = false
        pan_pid = nil
        pan_pending_vx = nil
        pan_pending_vy = nil
        return
    end

    if draw_active then
        draw_active = false
        local rx, ry, rw, rh = rect_from_points(draw_start_x, draw_start_y, x, y)
        local min_size = 10
        if rw < min_size or rh < min_size then
            -- Too small: just drop preview
            if draw_preview_id then
                aslan.execute_volatile(string.format("set_rect %d -10000 -10000 1 1", draw_preview_id))
            end
            draw_preview_id = nil
            return
        end

        aslan.create_frame(rx, ry, rw, rh)

        draw_preview_id = nil
        return
    end

    -- Commit final: une seule ligne dans l'AOF
    if dragged_node_id and pending_x and pending_y and pending_w and pending_h then
        aslan.execute(string.format("set_rect %d %f %f %f %f", dragged_node_id, pending_x, pending_y, pending_w, pending_h))
    end

    dragged_node_id = nil
    drag_mode = "none"
    resize_corner = nil
    pending_x, pending_y, pending_w, pending_h = nil, nil, nil, nil
end
