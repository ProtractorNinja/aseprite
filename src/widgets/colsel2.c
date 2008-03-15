/* ASE - Allegro Sprite Editor
 * Copyright (C) 2001-2008  David A. Capello
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "config.h"

#include <assert.h>
#include <allegro.h>

#include "jinete/jinete.h"

#include "core/app.h"
#include "core/color.h"
#include "modules/gui.h"
#include "modules/gfx.h"
#include "modules/palette.h"
#include "raster/image.h"
#include "widgets/colsel2.h"
#include "widgets/paledit.h"
#include "widgets/tabs.h"

enum {
  MODEL_RGB,
  MODEL_HSV,
  MODEL_GRAY,
  MODEL_MASK
};

typedef struct Model
{
  const char *text;
  int model;
  int color_type;
  JWidget (*create)(void);
} Model;

typedef struct ColorSelector
{
  color_t color;
  bool palette_locked;
} ColorSelector;

static JWidget create_rgb_container(void);
static JWidget create_hsv_container(void);
static JWidget create_gray_container(void);
static JWidget create_mask_container(void);

static int colorselector_type(void);
static ColorSelector *colorselector_data(JWidget widget);
static bool colorselector_msg_proc(JWidget widget, JMessage msg);
static void colorselector_update_lock_button(JWidget widget);
static void colorselector_set_color2(JWidget widget, color_t color, bool select_index_entry);
static void colorselector_set_paledit_index(JWidget widget, int index);

static void select_tab_callback(JWidget tabs, void *data);
static int slider_change_signal(JWidget widget, int user_data);
static int paledit_change_signal(JWidget widget, int user_data);
static int lock_button_select_hook(JWidget widget, int user_data);

static Model models[] = {
  { "RGB",	MODEL_RGB,	COLOR_TYPE_RGB,		create_rgb_container },
  { "HSV",	MODEL_HSV,	COLOR_TYPE_RGB,		create_hsv_container },
  { "Gray",	MODEL_GRAY,	COLOR_TYPE_GRAY,	create_gray_container },
  { "Mask",	MODEL_MASK,	COLOR_TYPE_MASK,	create_mask_container },
  { NULL,	0,		0,			NULL }
};

JWidget colorselector_new(void)
{
  int guiscale = GUISCALE;
  JWidget window = jtooltip_window_new("");
  JWidget grid1 = jgrid_new(3, FALSE);
  JWidget grid2 = jgrid_new(5, FALSE);
  JWidget tabs = tabs_new(select_tab_callback);
  JWidget pal = paledit_new(current_palette, FALSE, 4);
  JWidget idx = jlabel_new("Index=888");
  JWidget lock = jbutton_new("");
  JWidget child;
  ColorSelector *colorselector = jnew(ColorSelector, 1);
  Model *m;

  colorselector->color = color_mask();
  colorselector->palette_locked = TRUE;

  /* palette */
  jwidget_set_name(pal, "pal");
  jwidget_add_tooltip_text(pal, _("Use SHIFT or CTRL to select ranges"));

  /* idx label */
  jwidget_set_name(idx, "idx");
  jwidget_set_min_size(idx, jwidget_get_text_length(idx), 0);

  /* lock button */
  jwidget_set_name(lock, "lock");
  add_gfxicon_to_button(lock, GFX_BOX_LOCK, JI_CENTER | JI_MIDDLE);

  /* tabs */
  jwidget_set_name(tabs, "tabs");
  jwidget_set_bg_color(tabs, window->bg_color);

  /* data for a better layout */
  grid1->child_spacing = 0;
  grid2->border_width.t = 3;
  jwidget_set_min_size(grid2, 200*guiscale, 0);
  jwidget_expansive(grid2, TRUE);

  /* append a tab for each color-model */
  for (m=models; m->text!=NULL; ++m) {
    tabs_append_tab(tabs, _(m->text), (void *)m);

    child = (*m->create)();
    jwidget_set_name(child, m->text);
    jgrid_add_child(grid2, child, 1, 1, JI_HORIZONTAL | JI_TOP);
  }

  /* add children */
  jgrid_add_child(grid2, pal, 1, 1, JI_RIGHT | JI_TOP);
  jgrid_add_child(grid1, tabs, 1, 1, JI_HORIZONTAL | JI_BOTTOM);
  jgrid_add_child(grid1, idx, 1, 1, JI_RIGHT | JI_BOTTOM);
  jgrid_add_child(grid1, lock, 1, 1, JI_RIGHT | JI_BOTTOM);
  jgrid_add_child(grid1, grid2, 3, 1, JI_HORIZONTAL | JI_VERTICAL);
  jwidget_add_child(window, grid1);

  /* hooks */
  jwidget_add_hook(window,
		   colorselector_type(),
		   colorselector_msg_proc, colorselector);

  HOOK(pal, SIGNAL_PALETTE_EDITOR_CHANGE, paledit_change_signal, 0);
  HOOK(lock, JI_SIGNAL_BUTTON_SELECT, lock_button_select_hook, 0);

  /* update the lock button */
  colorselector_update_lock_button(window);
  return window;
}

void colorselector_set_color(JWidget widget, color_t color)
{
  colorselector_set_color2(widget, color, TRUE);
}

color_t colorselector_get_color(JWidget widget)
{
  ColorSelector *colorselector = colorselector_data(widget);

  return colorselector->color;
}

static JWidget create_rgb_container(void)
{
  JWidget grid = jgrid_new(2, FALSE);
  JWidget rlabel = jlabel_new("R");
  JWidget glabel = jlabel_new("G");
  JWidget blabel = jlabel_new("B");
  JWidget alabel = jlabel_new("A");
  JWidget rslider = jslider_new(0, 255, 0);
  JWidget gslider = jslider_new(0, 255, 0);
  JWidget bslider = jslider_new(0, 255, 0);
  JWidget aslider = jslider_new(0, 255, 0);
  jgrid_add_child(grid, rlabel, 1, 1, JI_RIGHT);
  jgrid_add_child(grid, rslider, 1, 1, JI_HORIZONTAL);
  jgrid_add_child(grid, glabel, 1, 1, JI_RIGHT);
  jgrid_add_child(grid, gslider, 1, 1, JI_HORIZONTAL);
  jgrid_add_child(grid, blabel, 1, 1, JI_RIGHT);
  jgrid_add_child(grid, bslider, 1, 1, JI_HORIZONTAL);
  jgrid_add_child(grid, alabel, 1, 1, JI_RIGHT);
  jgrid_add_child(grid, aslider, 1, 1, JI_HORIZONTAL);

  jwidget_set_name(rslider, "rgb_r");
  jwidget_set_name(gslider, "rgb_g");
  jwidget_set_name(bslider, "rgb_b");
  jwidget_set_name(aslider, "rgb_a");

  HOOK(rslider, JI_SIGNAL_SLIDER_CHANGE, slider_change_signal, 0);
  HOOK(gslider, JI_SIGNAL_SLIDER_CHANGE, slider_change_signal, 0);
  HOOK(bslider, JI_SIGNAL_SLIDER_CHANGE, slider_change_signal, 0);
  HOOK(aslider, JI_SIGNAL_SLIDER_CHANGE, slider_change_signal, 0);

  return grid;
}

static JWidget create_hsv_container(void)
{
  JWidget grid = jgrid_new(2, FALSE);
  JWidget hlabel = jlabel_new("H");
  JWidget slabel = jlabel_new("S");
  JWidget vlabel = jlabel_new("V");
  JWidget alabel = jlabel_new("A");
  JWidget hslider = jslider_new(0, 255, 0);
  JWidget sslider = jslider_new(0, 255, 0);
  JWidget vslider = jslider_new(0, 255, 0);
  JWidget aslider = jslider_new(0, 255, 0);
  jgrid_add_child(grid, hlabel, 1, 1, JI_RIGHT);
  jgrid_add_child(grid, hslider, 1, 1, JI_HORIZONTAL);
  jgrid_add_child(grid, slabel, 1, 1, JI_RIGHT);
  jgrid_add_child(grid, sslider, 1, 1, JI_HORIZONTAL);
  jgrid_add_child(grid, vlabel, 1, 1, JI_RIGHT);
  jgrid_add_child(grid, vslider, 1, 1, JI_HORIZONTAL);
  jgrid_add_child(grid, alabel, 1, 1, JI_RIGHT);
  jgrid_add_child(grid, aslider, 1, 1, JI_HORIZONTAL);

  jwidget_set_name(hslider, "hsv_h");
  jwidget_set_name(sslider, "hsv_s");
  jwidget_set_name(vslider, "hsv_v");
  jwidget_set_name(aslider, "hsv_a");

  HOOK(hslider, JI_SIGNAL_SLIDER_CHANGE, slider_change_signal, 0);
  HOOK(sslider, JI_SIGNAL_SLIDER_CHANGE, slider_change_signal, 0);
  HOOK(vslider, JI_SIGNAL_SLIDER_CHANGE, slider_change_signal, 0);
  HOOK(aslider, JI_SIGNAL_SLIDER_CHANGE, slider_change_signal, 0);

  return grid;
}

static JWidget create_gray_container(void)
{
  JWidget grid = jgrid_new(2, FALSE);
  JWidget klabel = jlabel_new("V");
  JWidget alabel = jlabel_new("A");
  JWidget vslider = jslider_new(0, 255, 0);
  JWidget aslider = jslider_new(0, 255, 0);
  jgrid_add_child(grid, klabel, 1, 1, JI_RIGHT);
  jgrid_add_child(grid, vslider, 1, 1, JI_HORIZONTAL);
  jgrid_add_child(grid, alabel, 1, 1, JI_RIGHT);
  jgrid_add_child(grid, aslider, 1, 1, JI_HORIZONTAL);

  jwidget_set_name(vslider, "gray_v");
  jwidget_set_name(aslider, "gray_a");

  HOOK(vslider, JI_SIGNAL_SLIDER_CHANGE, slider_change_signal, 0);
  HOOK(aslider, JI_SIGNAL_SLIDER_CHANGE, slider_change_signal, 0);

  return grid;
}

static JWidget create_mask_container(void)
{
  return jlabel_new("M");
}

static int colorselector_type(void)
{
  static int type = 0;
  if (!type)
    type = ji_register_widget_type();
  return type;
}

static ColorSelector *colorselector_data(JWidget widget)
{
  return (ColorSelector *)jwidget_get_data(widget,
					   colorselector_type());
}

static bool colorselector_msg_proc(JWidget widget, JMessage msg)
{
  ColorSelector *colorselector = colorselector_data(widget);

  switch (msg->type) {

    case JM_DESTROY:
      jfree(colorselector);
      break;

  }

  return FALSE;
}

static void colorselector_update_lock_button(JWidget widget)
{
  ColorSelector *colorselector = colorselector_data(widget);
  JWidget lock = jwidget_find_name(widget, "lock");

  if (colorselector->palette_locked) {
    set_gfxicon_in_button(lock, GFX_BOX_LOCK);
    jwidget_add_tooltip_text(lock, _("Press here to edit the palette"));
  }
  else {
    set_gfxicon_in_button(lock, GFX_BOX_UNLOCK);
    jwidget_add_tooltip_text(lock, _("Press here to lock the palette"));
  }
}

static void colorselector_set_color2(JWidget widget, color_t color, bool select_index_entry)
{
  ColorSelector *colorselector = colorselector_data(widget);
  JWidget tabs = jwidget_find_name(widget, "tabs");
  int imgtype = app_get_current_image_type();
  Model *m = tabs_get_selected_tab(tabs);
  JWidget rgb_rslider = jwidget_find_name(widget, "rgb_r");
  JWidget rgb_gslider = jwidget_find_name(widget, "rgb_g");
  JWidget rgb_bslider = jwidget_find_name(widget, "rgb_b");
  JWidget rgb_aslider = jwidget_find_name(widget, "rgb_a");
  JWidget hsv_hslider = jwidget_find_name(widget, "hsv_h");
  JWidget hsv_sslider = jwidget_find_name(widget, "hsv_s");
  JWidget hsv_vslider = jwidget_find_name(widget, "hsv_v");
  JWidget hsv_aslider = jwidget_find_name(widget, "hsv_a");
  JWidget gray_vslider = jwidget_find_name(widget, "gray_v");
  JWidget gray_aslider = jwidget_find_name(widget, "gray_a");

  colorselector->color = color;

  jslider_set_value(rgb_rslider, color_get_red(imgtype, color));
  jslider_set_value(rgb_gslider, color_get_green(imgtype, color));
  jslider_set_value(rgb_bslider, color_get_blue(imgtype, color));
  jslider_set_value(rgb_aslider, color_get_alpha(imgtype, color));
  jslider_set_value(hsv_hslider, color_get_hue(imgtype, color));
  jslider_set_value(hsv_sslider, color_get_saturation(imgtype, color));
  jslider_set_value(hsv_vslider, color_get_value(imgtype, color));
  jslider_set_value(hsv_aslider, color_get_alpha(imgtype, color));
  jslider_set_value(gray_vslider, color_get_value(imgtype, color));
  jslider_set_value(gray_aslider, color_get_alpha(imgtype, color));
  
  switch (color_type(color)) {
    case COLOR_TYPE_MASK:
      m = models+MODEL_MASK;
      break;
    case COLOR_TYPE_RGB:
      m = models+MODEL_RGB;
      break;
    case COLOR_TYPE_INDEX:
      if (m != models+MODEL_RGB &&
	  m != models+MODEL_HSV) {
	m = models+MODEL_RGB;
      }
      break;
    case COLOR_TYPE_HSV:
      m = models+MODEL_HSV;
      break;
    case COLOR_TYPE_GRAY:
      m = models+MODEL_GRAY;
      break;
    default:
      assert(FALSE);
  }

  tabs_select_tab(tabs, m);
  select_tab_callback(tabs, m);

  if (select_index_entry) {
    switch (color_type(color)) {
      case COLOR_TYPE_INDEX:
	colorselector_set_paledit_index(widget, color_get_index(IMAGE_INDEXED, color));
	break;
      case COLOR_TYPE_MASK:
	colorselector_set_paledit_index(widget, 0);
	break;
      default: {
	int palr = color_get_red  (IMAGE_RGB, color)/4;
	int palg = color_get_green(IMAGE_RGB, color)/4;
	int palb = color_get_blue (IMAGE_RGB, color)/4;
	int i = bestfit_color(current_palette, palr, palg, palb);
	if (i >= 0 && i < 256)
	  colorselector_set_paledit_index(widget, i);
	else
	  colorselector_set_paledit_index(widget, -1);
	break;
      }
    }
  }
}

static void colorselector_set_paledit_index(JWidget widget, int index)
{
  JWidget pal = jwidget_find_name(widget, "pal");
  JWidget idx = jwidget_find_name(widget, "idx");
  JWidget lock = jwidget_find_name(widget, "lock");
  char buf[256];

  if (index >= 0) {
    paledit_select_color(pal, index);
    sprintf(buf, "Index=%d", index);

    jwidget_enable(lock);
  }
  else {
    paledit_select_range(pal, -1, -1, PALETTE_EDITOR_RANGE_NONE);
    sprintf(buf, "None");

    jwidget_disable(lock);
  }

  jwidget_set_text(idx, buf);
}

static void select_tab_callback(JWidget tabs, void *data)
{
  JWidget window = jwidget_get_window(tabs);
  Model *selected_model = (Model *)data;
  JWidget child;
  Model *m;
  bool something_change = FALSE;

  for (m=models; m->text!=NULL; ++m) {
    child = jwidget_find_name(window, m->text);

    if (m == selected_model) {
      if (child->flags & JI_HIDDEN) {
	jwidget_show(child);
	something_change = TRUE;
      }
    }
    else {
      if (!(child->flags & JI_HIDDEN)) {
	jwidget_hide(child);
	something_change = TRUE;
      }
    }
  }

  if (something_change)
    jwidget_relayout(window);
}

static int slider_change_signal(JWidget widget, int user_data)
{
  JWidget window = jwidget_get_window(widget);
  ColorSelector *colorselector = colorselector_data(window);
  JWidget tabs = jwidget_find_name(window, "tabs");
  JWidget pal = jwidget_find_name(window, "pal");
  Model *m = tabs_get_selected_tab(tabs);
  color_t color = colorselector->color;
  int i, palr, palg, palb;
  
  switch (m->model) {
    case MODEL_RGB: {
      JWidget rslider = jwidget_find_name(window, "rgb_r");
      JWidget gslider = jwidget_find_name(window, "rgb_g");
      JWidget bslider = jwidget_find_name(window, "rgb_b");
      JWidget aslider = jwidget_find_name(window, "rgb_a");
      int r = jslider_get_value(rslider);
      int g = jslider_get_value(gslider);
      int b = jslider_get_value(bslider);
      int a = jslider_get_value(aslider);
      color = color_rgb(r, g, b, a);
      break;
    }
    case MODEL_HSV: {
      JWidget hslider = jwidget_find_name(window, "hsv_h");
      JWidget sslider = jwidget_find_name(window, "hsv_s");
      JWidget vslider = jwidget_find_name(window, "hsv_v");
      JWidget aslider = jwidget_find_name(window, "hsv_a");
      int h = jslider_get_value(hslider);
      int s = jslider_get_value(sslider);
      int v = jslider_get_value(vslider);
      int a = jslider_get_value(aslider);
      color = color_hsv(h, s, v, a);
      break;
    }
    case MODEL_GRAY: {
      JWidget vslider = jwidget_find_name(window, "gray_v");
      JWidget aslider = jwidget_find_name(window, "gray_a");
      int v = jslider_get_value(vslider);
      int a = jslider_get_value(aslider);
      color = color_gray(v, a);
      break;
    }
  }

  palr = color_get_red  (IMAGE_RGB, color)/4;
  palg = color_get_green(IMAGE_RGB, color)/4;
  palb = color_get_blue (IMAGE_RGB, color)/4;
  
  /* if the palette is locked then we have to search for the closest
     color to the RGB values */
  if (colorselector->palette_locked) {
    i = bestfit_color(current_palette, palr, palg, palb);
    if (i >= 0 && i < 256)
      colorselector_set_paledit_index(window, i);
  }
  /* the palette is unlocked, we have to modify the select entries */
  else {
    bool array[256];

    paledit_get_selected_entries(pal, array);
    for (i=0; i<256; ++i)
      if (array[i])
	set_current_color(i, palr, palg, palb);

    jwidget_dirty(pal);

    i = paledit_get_2nd_color(pal);
    if (i >= 0)
      color = color_index(i);
  }

  colorselector_set_color2(window, color, FALSE);
  jwidget_emit_signal(window, SIGNAL_COLORSELECTOR_COLOR_CHANGED);
  return 0;
}

static int paledit_change_signal(JWidget widget, int user_data)
{
  JWidget window = jwidget_get_window(widget);
  bool array[256];
  color_t color = colorselector_get_color(window);
  int i;

  paledit_get_selected_entries(widget, array);
  for (i=0; i<256; ++i)
    if (array[i]) {
      color = color_index(i);
      break;
    }

  colorselector_set_color2(window, color, TRUE);
  jwidget_emit_signal(window, SIGNAL_COLORSELECTOR_COLOR_CHANGED);
  return 0;
}

static int lock_button_select_hook(JWidget widget, int user_data)
{
  JWidget window = jwidget_get_window(widget);
  ColorSelector *colorselector = colorselector_data(window);

  colorselector->palette_locked = !colorselector->palette_locked;
  colorselector_update_lock_button(window);
  return TRUE;
}
