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

function on_mouse_down(x, y)
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
    if dragged_node_id then
        if drag_mode == "move" then
            local new_x = x - offset_x
            local new_y = y - offset_y
            local _, _, w, h = aslan.get_node_rect(dragged_node_id)
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

            -- Exécution volatile pendant le drag
            aslan.execute_volatile(string.format("set_rect %d %f %f %f %f", dragged_node_id, new_x, new_y, new_w, new_h))
        end
    end
end

function on_mouse_up(x, y)
    -- Commit final: une seule ligne dans l'AOF
    if dragged_node_id and pending_x and pending_y and pending_w and pending_h then
        aslan.execute(string.format("set_rect %d %f %f %f %f", dragged_node_id, pending_x, pending_y, pending_w, pending_h))
    end

    dragged_node_id = nil
    drag_mode = "none"
    resize_corner = nil
    pending_x, pending_y, pending_w, pending_h = nil, nil, nil, nil
end
