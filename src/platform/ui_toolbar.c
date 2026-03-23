#include "ui_toolbar.h"
#include "ui_canvas.h"
#include "ui_shared.h"
#include "ui_host.h"

// Canvas Helper: Check if point is in toolbar area
// -----------------------------------------------------------------------------

int canvas_point_in_toolbar(float bx, float by, float bw, float bh,
                            float mx, float my, CanvasInteraction *cs) {
  const float btn_size = 36.0f;
  const float btn_spacing = 8.0f;
  const float toolbar_w = 7 * btn_size + 6 * btn_spacing + 24.0f; // 7 buttons
  const float toolbar_h = 52.0f;
  const float margin = 20.0f;

  float toolbar_x = bx + (bw - toolbar_w) / 2.0f;
  float toolbar_y = by + bh - toolbar_h - margin;

  // Basic toolbar hit
  if (mx >= toolbar_x && mx < toolbar_x + toolbar_w && my >= toolbar_y &&
      my < toolbar_y + toolbar_h)
    return 1;

  // Sub-menu hit
  if (cs->show_sub_menu) {
    const float menu_w = 140.0f;
    const float item_h = 32.0f;
    const float menu_padding = 8.0f;
    int items = (cs->active_cat == CANVAS_CAT_SHAPES) ? 4 : 3;
    float menu_h = items * item_h + 2 * menu_padding;

    // Calculate which button is active to find menu_x
    float btn_x_start = toolbar_x + 12.0f;
    float active_btn_x =
        btn_x_start + (int)cs->active_cat * (btn_size + btn_spacing);
    float menu_x = active_btn_x - menu_w / 2.0f + 16.0f;
    float menu_y = toolbar_y - menu_h - 12.0f;

    if (mx >= menu_x && mx < menu_x + menu_w && my >= menu_y &&
        my < menu_y + menu_h)
      return 1;
  }

  return 0;
}

// -----------------------------------------------------------------------------
// Canvas Helper: Render floating toolbar (Apple Freeform style)
// -----------------------------------------------------------------------------

void canvas_render_sub_menu(UIHost *ui, float x, float y, CanvasCategory cat) {
  CanvasInteraction *cs = &ui->canvas_state;
  const float item_h = 32.0f;
  const float menu_padding = 8.0f;
  const float menu_w = 140.0f;
  int item_count = 0;

  // Define sub-options based on category
  const char *items[8];
  const char *icons[8];
  CanvasSubOption sub_opts[8];

  if (cat == CANVAS_CAT_SELECT) {
    // Pan is now automatic when dragging background, so no menu needed
    item_count = 0; // Hide the menu entirely
  } else if (cat == CANVAS_CAT_SHAPES) {
    items[0] = "Rectangle";
    icons[0] = ICON_SHAPE;
    sub_opts[0] = CANVAS_SUB_SHAPE_RECT;
    items[1] = "Ellipse";
    icons[1] = "\xEE\xAD\xA4";
    sub_opts[1] = CANVAS_SUB_SHAPE_ELLIPSE; // Re-use shape for now
    items[2] = "Triangle";
    icons[2] = ICON_SHAPE;
    sub_opts[2] = CANVAS_SUB_SHAPE_TRIANGLE;
    items[3] = "Line";
    icons[3] = "\xEF\x84\xA5";
    sub_opts[3] = CANVAS_SUB_SHAPE_LINE;
    item_count = 4;
  } else if (cat == CANVAS_CAT_TEXT) {
    items[0] = "Heading";
    icons[0] = ICON_H1;
    sub_opts[0] = CANVAS_SUB_TEXT_HEADING;
    items[1] = "Body";
    icons[1] = ICON_TEXT;
    sub_opts[1] = CANVAS_SUB_TEXT_BODY;
    items[2] = "Sticky";
    icons[2] = ICON_STICKY;
    sub_opts[2] = CANVAS_SUB_TEXT_STICKY;
    item_count = 3;
  } else if (cat == CANVAS_CAT_MEDIA) {
    items[0] = "Image";
    icons[0] = ICON_IMAGE;
    sub_opts[0] = CANVAS_SUB_MEDIA_IMAGE;
    item_count = 1;
  }

  if (item_count == 0)
    return;

  float menu_h = item_count * item_h + 2 * menu_padding;
  float menu_x = x - menu_w / 2.0f + 16.0f; // Center relative to button
  float menu_y = y - menu_h - 12.0f;

  // Shadow
  nvgBeginPath(ui->vg);
  nvgRoundedRect(ui->vg, menu_x + 2, menu_y + 4, menu_w, menu_h, 12.0f);
  nvgFillColor(ui->vg, nvgRGBA(0, 0, 0, 30));
  nvgFill(ui->vg);

  // Background
  nvgBeginPath(ui->vg);
  nvgRoundedRect(ui->vg, menu_x, menu_y, menu_w, menu_h, 12.0f);
  nvgFillColor(ui->vg, nvgRGBA(255, 255, 255, 255));
  nvgFill(ui->vg);
  nvgStrokeColor(ui->vg, nvgRGBA(230, 230, 230, 255));
  nvgStrokeWidth(ui->vg, 1.0f);
  nvgStroke(ui->vg);

  for (int i = 0; i < item_count; i++) {
    float ir_x = menu_x + menu_padding, ir_y = menu_y + menu_padding + i * item_h;
    float ir_w = menu_w - 2 * menu_padding, ir_h = item_h;
    int hovered =
        (ui->mouse_x >= ir_x && ui->mouse_x < ir_x + ir_w &&
         ui->mouse_y >= ir_y && ui->mouse_y < ir_y + ir_h);

    if (hovered) {
      nvgBeginPath(ui->vg);
      nvgRoundedRect(ui->vg, ir_x, ir_y, ir_w, ir_h, 6.0f);
      nvgFillColor(ui->vg, nvgRGBA(0, 0, 0, 15));
      nvgFill(ui->vg);

      if (cs->mouse_just_pressed) {
        // Handle sub-option selection
        if (cat == CANVAS_CAT_SHAPES) {
          // Set pending_create to 10-13 for shapes
          cs->space_held = 0;    // Disable pan mode
          cs->pan_from_menu = 0; // Clear menu pan
          cs->is_panning = 0;    // Stop any active panning
          if (sub_opts[i] == CANVAS_SUB_SHAPE_RECT)
            cs->pending_create = 10;
          else if (sub_opts[i] == CANVAS_SUB_SHAPE_ELLIPSE)
            cs->pending_create = 11;
          else if (sub_opts[i] == CANVAS_SUB_SHAPE_TRIANGLE)
            cs->pending_create = 12;
          else if (sub_opts[i] == CANVAS_SUB_SHAPE_LINE)
            cs->pending_create = 13;
        } else if (cat == CANVAS_CAT_TEXT) {
          cs->tool = CANVAS_TOOL_SELECT;
          cs->space_held = 0;    // Disable pan mode
          cs->pan_from_menu = 0; // Clear menu pan
          cs->is_panning = 0;    // Stop any active panning
          // H1=6, Body=1, Sticky=2
          if (sub_opts[i] == CANVAS_SUB_TEXT_HEADING)
            cs->pending_create = 6;
          else if (sub_opts[i] == CANVAS_SUB_TEXT_BODY)
            cs->pending_create = 1;
          else if (sub_opts[i] == CANVAS_SUB_TEXT_STICKY)
            cs->pending_create = 2;
        } else if (cat == CANVAS_CAT_MEDIA) {
          cs->tool = CANVAS_TOOL_SELECT;
          cs->space_held = 0;    // Disable pan mode
          cs->pan_from_menu = 0; // Clear menu pan
          cs->is_panning = 0;    // Stop any active panning
          if (sub_opts[i] == CANVAS_SUB_MEDIA_IMAGE)
            cs->pending_create = 3;
        }
        cs->show_sub_menu = 0;
      }
    }

    // Icon
    if (ui->font_icons) {
      nvgFontSize(ui->vg, 20.0f);
      nvgFontFace(ui->vg, "Icons");
      nvgFillColor(ui->vg, nvgRGBA(80, 80, 80, 255));
      nvgTextAlign(ui->vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
      nvgText(ui->vg, ir_x + 8, ir_y + 16, icons[i], NULL);
    }
    // Label
    if (ui->font_inter) {
      nvgFontSize(ui->vg, ui->font_size_px);
      nvgFontFace(ui->vg, "Inter");
      nvgFillColor(ui->vg, nvgRGBA(40, 40, 40, 255));
      nvgTextAlign(ui->vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
      nvgText(ui->vg, ir_x + 36, ir_y + 16, items[i], NULL);
    }
  }
}


void canvas_render_toolbar(UIHost *ui, float bx, float by, float bw, float bh,
                           Block *block) {
  const float btn_size = 36.0f;
  const float btn_spacing = 8.0f;
  // Design gets an extra "Table layout" button,
  // canvas has one fewer
  int tool_count = (block->type == BLOCK_DESIGN) ? 5 : 4;
  const float toolbar_w =
      tool_count * btn_size + (tool_count - 1) * btn_spacing + 24.0f;
  const float toolbar_h = 52.0f;
  const float margin = 20.0f;

  float toolbar_x = bx + (bw - toolbar_w) / 2.0f;
  float toolbar_y = by + bh - toolbar_h - margin;

  CanvasInteraction *cs = &ui->canvas_state;

  // Shadow
  nvgBeginPath(ui->vg);
  nvgRoundedRect(ui->vg, toolbar_x + 2, toolbar_y + 4, toolbar_w, toolbar_h,
                 26.0f);
  nvgFillColor(ui->vg, nvgRGBA(0, 0, 0, 20));
  nvgFill(ui->vg);

  // Background
  nvgBeginPath(ui->vg);
  nvgRoundedRect(ui->vg, toolbar_x, toolbar_y, toolbar_w, toolbar_h, 26.0f);
  nvgFillColor(ui->vg, nvgRGBA(255, 255, 255, 255));
  nvgFill(ui->vg);
  nvgStrokeColor(ui->vg, nvgRGBA(220, 220, 220, 255));
  nvgStrokeWidth(ui->vg, 1.0f);
  nvgStroke(ui->vg);

  float btn_x = toolbar_x + 12.0f;
  float btn_y = toolbar_y + (toolbar_h - btn_size) / 2.0f;

  // 7 buttons (canvas) or 8 buttons (design):
  // last button is Linked Text
  const char *tool_icons[] = {ICON_SHAPE, ICON_TEXT, ICON_IMAGE, ICON_CHART,
                              "\xEF\x87\x9E"}; // U+F1DE sliders — insert
                                               // table layout

  for (int i = 0; i < tool_count; i++) {
    float btn_ix = btn_x + i * (btn_size + btn_spacing);
    float btn_iy = btn_y;

    // Map button index to category (skip
    // CANVAS_CAT_SELECT)
    int is_active = (cs->active_cat == (CanvasCategory)(i + 1));
    if (i == 3)
      is_active = (cs->pending_create == 11); // Chart active state (transient)
    if (i == 4)
      is_active = 0; // Table layout — one-shot,
                     // never stays active

    float rel_mx = ui->mouse_x - (ui->abs_offset_x + btn_ix);
    float rel_my = ui->mouse_y - (ui->abs_offset_y + btn_iy);
    int hovered = (rel_mx >= 0 && rel_mx < btn_size &&
                   rel_my >= 0 && rel_my < btn_size);

    if (is_active || hovered) {
      NVGcolor bg_color = is_active ? nvgRGBA(110, 110, 130, 25)
                                    : nvgRGBA(0, 0, 0, 10);
      nvgBeginPath(ui->vg);
      nvgRoundedRect(ui->vg, btn_ix, btn_iy, btn_size, btn_size, 8.0f);
      nvgFillColor(ui->vg, bg_color);
      nvgFill(ui->vg);
    }

    NVGcolor icon_color = is_active ? nvgRGBA(110, 110, 130, 255)
                                    : nvgRGB(60, 60, 60);

    {
      nvgFontSize(ui->vg, btn_size - 16);
      nvgFontFaceId(ui->vg, ui->nvgw.font_icons);
      nvgFillColor(ui->vg, icon_color);
      nvgTextAlign(ui->vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
      nvgText(ui->vg, btn_ix + btn_size / 2, btn_iy + btn_size / 2,
              tool_icons[i], NULL);
    }

    if (hovered && cs->mouse_just_pressed) {
      CanvasCategory cat = (CanvasCategory)(i + 1); // Skip
                                                    // CANVAS_CAT_SELECT
      if (cat == CANVAS_CAT_SHAPES) {
        // DEFAULT: Just create a rectangle
        cs->pending_create = 10;
        cs->show_sub_menu = 0;
      } else if (cat == CANVAS_CAT_TEXT) {
        // DIRECT ACTION: Create paragraph
        cs->pending_create = 1; // 1 = Body Text
        cs->show_sub_menu = 0;
      } else if (cat == CANVAS_CAT_MEDIA) {
        // DIRECT ACTION: Open image dialog
        cs->pending_create = 3; // 3 = Image
        cs->show_sub_menu = 0;
      } else if (i == 3) { // Chart button
                           // (direct action)
        cs->pending_create = 11;
        cs->show_sub_menu = 0;
      } else if (i == 4) { // Table layout button
                           // (direct action)
        cs->pending_create = 20;
        cs->show_sub_menu = 0;
      } else if (cs->active_cat == cat) {
        cs->show_sub_menu = !cs->show_sub_menu;
      } else {
        cs->active_cat = cat;
        cs->show_sub_menu = 1; // Open sub-menu for others
      }
    }

    // Render sub-menu if active
    if (is_active && cs->show_sub_menu) {
      canvas_render_sub_menu(ui, btn_ix, btn_iy,
                             (CanvasCategory)(i + 1)); // Skip
                                                       // CANVAS_CAT_SELECT
    }
  }
}

