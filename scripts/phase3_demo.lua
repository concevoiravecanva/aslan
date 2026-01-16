-- =============================================================================
-- PHASE 3 DEMO: ADVANCED CREATIVE FEATURES
-- =============================================================================
-- This demo showcases:
-- 1. Clip Engine (FFmpeg integration) - Video thumbnails and metadata
-- 2. Note-to-Timeline Sync - Script-driven video editing
-- 3. SVG/PDF Export - High-quality canvas export

aslan.log("Phase 3 Demo: Advanced Creative Features loaded!")

-- =============================================================================
-- STATE
-- =============================================================================

local demo_state = {
    show_window = true,
    show_export_panel = false,
    show_timeline_panel = false,
    
    -- Export options
    export_format = 1,
    export_formats = {"SVG", "PDF"},
    export_title = "My Canvas",
    export_scale = 1.0,
    
    -- Timeline sync
    selected_note = nil,
    selected_clip = nil,
    timeline_mode = false,
}

-- =============================================================================
-- MAIN WINDOW
-- =============================================================================

function on_ui()
    if not demo_state.show_window then return end
    
    local flags = nk.WINDOW_BORDER | nk.WINDOW_TITLE | nk.WINDOW_MOVABLE
    
    if nk.begin("Phase 3: Creative Features", 10, 420, 300, 280, flags) then
        nk.layout_row_dynamic(25, 1)
        nk.label("🎬 Moodboard to Movie", nk.TEXT_CENTERED)
        
        nk.spacing(1)
        
        -- Feature buttons
        nk.layout_row_dynamic(30, 1)
        
        if nk.button("📊 Export Panel") then
            demo_state.show_export_panel = not demo_state.show_export_panel
        end
        
        if nk.button("🎞️ Timeline Sync Panel") then
            demo_state.show_timeline_panel = not demo_state.show_timeline_panel
        end
        
        nk.spacing(1)
        
        -- Quick create buttons
        nk.layout_row_dynamic(25, 1)
        nk.label("Create Elements:", nk.TEXT_LEFT)
        
        nk.layout_row_dynamic(30, 2)
        
        if nk.button("📝 Note") then
            local x = math.random(100, 500)
            local y = math.random(100, 300)
            aslan.execute(string.format("create_note 0 Script %d %d 300 250", x, y))
            aslan.log("Created Note node")
        end
        
        if nk.button("🎬 Clip") then
            local x = math.random(100, 500)
            local y = math.random(100, 300)
            aslan.execute(string.format("create_clip 0 Timeline %d %d 400 150", x, y))
            aslan.log("Created Clip node")
        end
        
        nk.spacing(1)
        
        -- Status info
        nk.layout_row_dynamic(20, 1)
        nk.label("Status:", nk.TEXT_LEFT)
        
        local selected = aslan.get_selected_id()
        if selected then
            local node_type = aslan.get_node_type(selected)
            local type_name = "Unknown"
            if node_type == aslan.NODE_NOTE then type_name = "Note"
            elseif node_type == aslan.NODE_CLIP then type_name = "Clip"
            elseif node_type == aslan.NODE_GRID then type_name = "Grid"
            elseif node_type == aslan.NODE_RECT then type_name = "Rect"
            elseif node_type == aslan.NODE_FRAME then type_name = "Frame"
            end
            nk.label("Selected: " .. type_name .. " #" .. selected, nk.TEXT_LEFT)
        else
            nk.label("No selection", nk.TEXT_LEFT)
        end
    end
    nk.nk_end()
    
    -- Sub-panels
    if demo_state.show_export_panel then
        render_export_panel()
    end
    
    if demo_state.show_timeline_panel then
        render_timeline_panel()
    end
end

-- =============================================================================
-- EXPORT PANEL
-- =============================================================================

function render_export_panel()
    local flags = nk.WINDOW_BORDER | nk.WINDOW_TITLE | nk.WINDOW_MOVABLE | nk.WINDOW_CLOSABLE
    
    if nk.begin("Export Canvas", 320, 420, 280, 250, flags) then
        nk.layout_row_dynamic(25, 1)
        nk.label("Export Format:", nk.TEXT_LEFT)
        
        nk.layout_row_dynamic(30, 1)
        demo_state.export_format = nk.combo(demo_state.export_formats, 
            demo_state.export_format, 25, 150, 80)
        
        nk.spacing(1)
        
        nk.layout_row_dynamic(25, 1)
        nk.label("Document Title:", nk.TEXT_LEFT)
        
        nk.layout_row_dynamic(30, 1)
        demo_state.export_title = nk.edit_string(demo_state.export_title)
        
        nk.spacing(1)
        
        nk.layout_row_dynamic(25, 1)
        nk.label("Scale:", nk.TEXT_LEFT)
        
        nk.layout_row_dynamic(25, 1)
        demo_state.export_scale = nk.slider_float(0.25, demo_state.export_scale, 4.0, 0.25)
        nk.label(string.format("%.0f%%", demo_state.export_scale * 100), nk.TEXT_CENTERED)
        
        nk.spacing(1)
        
        nk.layout_row_dynamic(35, 1)
        if nk.button("🖼️ Export Now") then
            local format = demo_state.export_formats[demo_state.export_format]
            local filename = "aslan_export." .. string.lower(format)
            
            local success = false
            if format == "SVG" then
                success = aslan.export_svg(filename, demo_state.export_title, demo_state.export_scale)
            elseif format == "PDF" then
                success = aslan.export_pdf(filename, demo_state.export_title, "A4", true)
            end
            
            if success then
                aslan.log("✓ Exported to: " .. filename)
            else
                aslan.log("✗ Export failed: " .. filename)
            end
        end
    end
    nk.nk_end()
end

-- =============================================================================
-- TIMELINE SYNC PANEL
-- =============================================================================

function render_timeline_panel()
    local flags = nk.WINDOW_BORDER | nk.WINDOW_TITLE | nk.WINDOW_MOVABLE | nk.WINDOW_CLOSABLE
    
    if nk.begin("Timeline Sync", 610, 420, 300, 280, flags) then
        nk.layout_row_dynamic(25, 1)
        nk.label("Note-to-Timeline Workflow", nk.TEXT_CENTERED)
        
        nk.spacing(1)
        
        -- Instructions
        nk.layout_row_dynamic(20, 1)
        nk.label("1. Create a Note (your script)", nk.TEXT_LEFT)
        nk.label("2. Create Clip nodes (your videos)", nk.TEXT_LEFT)
        nk.label("3. Drag clips to Note paragraphs", nk.TEXT_LEFT)
        nk.label("4. Export as EDL or timeline", nk.TEXT_LEFT)
        
        nk.spacing(1)
        
        -- Mode toggle
        nk.layout_row_dynamic(30, 1)
        demo_state.timeline_mode = nk.check("Enable Timeline Mode", demo_state.timeline_mode)
        
        if demo_state.timeline_mode then
            nk.layout_row_dynamic(20, 1)
            nk.label_colored("✓ Drag clips to Note rows!", nk.TEXT_CENTERED, 100, 255, 100, 255)
        end
        
        nk.spacing(1)
        
        -- Export options
        nk.layout_row_dynamic(25, 1)
        nk.label("Export Timeline:", nk.TEXT_LEFT)
        
        nk.layout_row_dynamic(30, 2)
        if nk.button("EDL") then
            aslan.log("Exporting timeline as EDL...")
        end
        
        if nk.button("Text") then
            aslan.log("Exporting timeline as text...")
        end
        
        nk.spacing(1)
        
        -- Stats
        nk.layout_row_dynamic(20, 1)
        local nodes = aslan.get_all_nodes() or {}
        local note_count = 0
        local clip_count = 0
        
        for _, id in ipairs(nodes) do
            local t = aslan.get_node_type(id)
            if t == aslan.NODE_NOTE then note_count = note_count + 1
            elseif t == aslan.NODE_CLIP then clip_count = clip_count + 1
            end
        end
        
        nk.label("Notes: " .. note_count .. " | Clips: " .. clip_count, nk.TEXT_LEFT)
    end
    nk.nk_end()
end

-- =============================================================================
-- VIDEO INFO DISPLAY (when Clip is selected)
-- =============================================================================

function render_video_info(node_id)
    -- This would display FFmpeg metadata for the selected clip
    -- For demo purposes, we show placeholder info
    
    local flags = nk.WINDOW_BORDER | nk.WINDOW_TITLE
    
    if nk.begin("Video Info", 920, 420, 200, 180, flags) then
        nk.layout_row_dynamic(20, 1)
        nk.label("📹 Clip #" .. node_id, nk.TEXT_LEFT)
        
        nk.spacing(1)
        
        -- Placeholder metadata
        nk.label("Duration: 00:05:23", nk.TEXT_LEFT)
        nk.label("Resolution: 1920x1080", nk.TEXT_LEFT)
        nk.label("Codec: H.264", nk.TEXT_LEFT)
        nk.label("FPS: 24", nk.TEXT_LEFT)
        
        nk.spacing(1)
        
        nk.layout_row_dynamic(25, 1)
        if nk.button("Generate Thumbnail") then
            aslan.log("Generating thumbnail for clip " .. node_id)
        end
    end
    nk.nk_end()
end

-- =============================================================================
-- KEYBOARD SHORTCUTS
-- =============================================================================

function on_key(key, mods)
    -- F6 to toggle Phase 3 demo
    if key == 1073741887 then -- F6
        demo_state.show_window = not demo_state.show_window
    end
    
    -- E for export panel
    if key == 101 and mods > 0 then -- Ctrl+E or Cmd+E
        demo_state.show_export_panel = not demo_state.show_export_panel
    end
    
    -- T for timeline panel  
    if key == 116 and mods > 0 then -- Ctrl+T or Cmd+T
        demo_state.show_timeline_panel = not demo_state.show_timeline_panel
    end
end
