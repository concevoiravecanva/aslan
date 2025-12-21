-- scripts/tools.lua
aslan.log("Tools script loaded (Command-Log version)!")

local dragged_node_id = nil
local drag_mode = "none" -- "move" or "resize"
local resize_corner = nil -- "tl"|"tr"|"bl"|"br"
local offset_x = 0
local offset_y = 0
local offset_w = 0
local offset_h = 0

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

function on_mouse_down(x, y)
    -- Draw-rect tool
    if current_tool_mode() == TOOL_CREATE_RECT then
        draw_active = true
        draw_start_x = x
        draw_start_y = y

        -- Create a preview node (volatile, not in AOF)
        aslan.execute_volatile("new_rect " .. x .. " " .. y .. " 1 1")
        draw_preview_id = aslan.get_selected_id()
        if draw_preview_id then
            aslan.execute_volatile(string.format("select %d", draw_preview_id))
            aslan.log("Draw preview id=" .. draw_preview_id)
        end
        return
    end

    dragged_node_id = aslan.get_node_at(x, y)
    if dragged_node_id then
        -- On informe le moteur C de la sélection pour l'affichage des gizmos
        aslan.execute(string.format("select %d", dragged_node_id))

        local nx, ny, nw, nh = aslan.get_node_rect(dragged_node_id)

        -- Les poignées sont dans chaque coin, *à l'intérieur*, taille ~18px (doit matcher renderer.c)
        local handle = 18

        local in_tl = (x >= nx and x <= nx + handle and y >= ny and y <= ny + handle)
        local in_tr = (x >= nx + nw - handle and x <= nx + nw and y >= ny and y <= ny + handle)
        local in_bl = (x >= nx and x <= nx + handle and y >= ny + nh - handle and y <= ny + nh)
        local in_br = (x >= nx + nw - handle and x <= nx + nw and y >= ny + nh - handle and y <= ny + nh)

        if in_tl or in_tr or in_bl or in_br then
            drag_mode = "resize"
            if in_tl then resize_corner = "tl"
            elseif in_tr then resize_corner = "tr"
            elseif in_bl then resize_corner = "bl"
            else resize_corner = "br" end

            -- Pour éviter les sauts: on garde l'offset entre la souris et le coin "actif"
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

    -- Initialiser le pending state sur la géométrie courante
    pending_x, pending_y, pending_w, pending_h = nx, ny, nw, nh
    end
end

function on_mouse_move(x, y)
    -- Draw-rect preview update
    if draw_active and draw_preview_id then
        local rx, ry, rw, rh = rect_from_points(draw_start_x, draw_start_y, x, y)
        -- keep minimum size for visibility
        local min_size = 3
        rw = math.max(min_size, rw)
        rh = math.max(min_size, rh)

    rx, ry, rw, rh = snap_rect(rx, ry, rw, rh)
        aslan.execute_volatile(string.format("set_rect %d %f %f %f %f", draw_preview_id, rx, ry, rw, rh))
        return
    end

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
    -- Commit draw-rect
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

        -- Remove preview (move offscreen) and create committed rect
        if draw_preview_id then
            aslan.execute_volatile(string.format("set_rect %d -10000 -10000 1 1", draw_preview_id))
        end

        -- Create committed rect (this logs a single create_rect line)
        aslan.execute(string.format("new_rect %f %f %f %f", rx, ry, rw, rh))
        local new_id = aslan.get_selected_id()
        if new_id then
            aslan.execute(string.format("select %d", new_id))
        end

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
