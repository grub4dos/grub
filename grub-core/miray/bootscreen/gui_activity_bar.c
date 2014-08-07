/* gui_activity_bar.c - GUI activity bar component, based on GUI progress bar */
/*
 *  Extention for GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2008,2009  Free Software Foundation, Inc.
 *  Copyright (C) 2014 Miray Software <oss@miray.de>
 *
 *  GRUB is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  GRUB is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with GRUB.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <grub/mm.h>
#include <grub/misc.h>
#include <grub/gui.h>
#include <grub/font.h>
#include <grub/gui_string_util.h>
#include <grub/gfxmenu_view.h>
#include <grub/gfxwidgets.h>
#include <grub/i18n.h>
#include <grub/color.h>

#include <grub/time.h>
#include "gui_activity_bar.h"
#include "miray_screen.h"
#include "miray_gfx_screen.h"

static const unsigned bar_highlight_width = 31;
static const unsigned bar_step            = 1;

static const grub_uint32_t miray_progress_tick_throttle_ms = 5;

struct grub_gui_activity_bar
{
  struct grub_gui_activity activity;

  grub_gui_container_t parent;
  grub_video_rect_t bounds;
  char *id;
  int visible;
  unsigned step;
  int value;
  int end;
  unsigned highlight_width;
  grub_video_rgba_color_t border_color;
  grub_video_rgba_color_t bg_color;
  grub_video_rgba_color_t fg_color;

  char *theme_dir;
  int need_to_recreate_pixmaps;
  int pixmapbar_available;
  char *bar_pattern;
  char *highlight_pattern;
  grub_gfxmenu_box_t bar_box;
  grub_gfxmenu_box_t highlight_box;
  int highlight_overlay;
};

typedef struct grub_gui_activity_bar *grub_gui_activity_bar_t;

static void
activity_bar_destroy (void *vself)
{
  grub_gui_activity_bar_t self = vself;
  grub_free (self->theme_dir);
  grub_free (self->id);
  grub_free (self);
}

static const char *
activity_bar_get_id (void *vself)
{
  grub_gui_activity_bar_t self = vself;
  return self->id;
}

static int
activity_bar_is_instance (void *vself __attribute__((unused)), const char *type)
{
  return grub_strcmp (type, "component") == 0;
}


static void
draw_filled_rect_bar (grub_gui_activity_bar_t self, unsigned int barstart, unsigned int barend)
{
  /* Set the progress bar's frame.  */
  grub_video_rect_t f;
  f.x = 1;
  f.y = 1;
  //f.width = self->bounds.width - 2;
  //f.height = self->bounds.height - 2;
   f.width = self->activity.component.w;
   f.height = self->activity.component.h;

  /* Border.  */
  grub_video_fill_rect (grub_video_map_rgba_color (self->border_color),
                        f.x - 1, f.y - 1,
                        f.width + 2, f.height + 2);

  barstart = grub_max(barstart, f.x);
  barend = grub_min((unsigned int)barend, f.width + f.x);

  /* Bar background */
  if ((unsigned int)barstart > f.x)
    grub_video_fill_rect (grub_video_map_rgba_color (self->bg_color),
                        f.x, f.y,
                        barstart - f.x, f.height);
  if ((unsigned int)barend < f.width + f.x)
    grub_video_fill_rect (grub_video_map_rgba_color (self->bg_color),
                        barend + 1, f.y,
                        f.width - (barend + 1), f.height);

  /* Bar foreground.  */
  grub_video_fill_rect (grub_video_map_rgba_color (self->fg_color),
                        barstart, f.y,
                        barend - barstart + 1, f.height);
}


static void
activity_bar_paint (void *vself, const grub_video_rect_t *region)
{
  grub_gui_activity_bar_t self = vself;
  grub_video_rect_t vpsave;

  if (! self->visible)
    return;
  if (!grub_video_have_common_points (region, &self->bounds))
    return;

  grub_gui_set_viewport (&self->bounds, &vpsave);

  int barstart = self->value;
  int barend = self->value + self->highlight_width;

  draw_filled_rect_bar (self, barstart, barend);

  grub_gui_restore_viewport (&vpsave);
}


static void
progress_bar_paint (void *vself, const grub_video_rect_t *region)
{
  grub_gui_activity_bar_t self = vself;
  grub_video_rect_t vpsave;

  if (! self->visible)
    return;
  if (!grub_video_have_common_points (region, &self->bounds))
    return;

  if (self->end == 0)
    return;

  grub_gui_set_viewport (&self->bounds, &vpsave);

  int barstart = (int)0;
  int barend   = self->end != 0 ? grub_divmod64(self->value * (grub_uint64_t)self->activity.component.w, self->end, 0) : 0;

  draw_filled_rect_bar (self, barstart, barend);

  grub_gui_restore_viewport (&vpsave);
}


static void
activity_bar_set_parent (void *vself, grub_gui_container_t parent)
{
  grub_gui_activity_bar_t self = vself;
  self->parent = parent;
}

static grub_gui_container_t
activity_bar_get_parent (void *vself)
{
  grub_gui_activity_bar_t self = vself;
  return self->parent;
}

static void
activity_bar_set_bounds (void *vself, const grub_video_rect_t *bounds)
{
  grub_gui_activity_bar_t self = vself;
  self->bounds = *bounds;
}

static void
activity_bar_get_bounds (void *vself, grub_video_rect_t *bounds)
{
  grub_gui_activity_bar_t self = vself;
  *bounds = self->bounds;
}

static void
activity_bar_get_minimal_size (void *vself __attribute__((unused)),
			       unsigned *width, unsigned *height)
{
  unsigned min_width = 0;
  unsigned min_height = 0;

  min_height += 2;
  min_width += 2;

  *width = 200;
  if (*width < min_width)
    *width = min_width;
  *height = 28;
  if (*height < min_height)
    *height = min_height;
}


static void
activity_bar_set_state (void *vself, int visible, int start __attribute__ ((unused)),
			int current, int end __attribute__ ((unused)))
{
  if (current != gui_activity_bar_advance_val)
    return;

  grub_gui_activity_bar_t self = vself;
  self->visible = visible;
  self->value = self->value + self->step;
  if (self->value >= (int)self->bounds.width - 2)
    self->value = self->step - self->highlight_width;
}



static void
progress_bar_set_state (void *vself, int visible, int start __attribute__ ((unused)),
			int current, int end)
{
  if (current == gui_activity_bar_advance_val)
    return;

  grub_gui_activity_bar_t self = vself;

  self->visible = visible;
  self->value = current;
  self->end = end;
}

static void
activity_bar_finish(void *vself)
{
  grub_gui_activity_bar_t self = vself;

  while (self->value > 0)
  {
	 grub_millisleep (miray_progress_tick_throttle_ms);
    activity_bar_set_state(vself, 1, 0, gui_activity_bar_advance_val, 0);
    miray_screen_draw_activity();
  }

  while (self->value < 0)
  {
    grub_millisleep (miray_progress_tick_throttle_ms);
    activity_bar_set_state(vself, 1, 0, gui_activity_bar_advance_val, 0);
    miray_screen_draw_activity();
  }

  while (self->highlight_width < self->bounds.width)
  {
    grub_millisleep (miray_progress_tick_throttle_ms);
    self->highlight_width += self->step;
    miray_screen_draw_activity();
  }

  grub_millisleep (miray_progress_tick_throttle_ms);
}

static grub_err_t
activity_bar_set_property (void *vself, const char *name, const char *value)
{
  grub_gui_activity_bar_t self = vself;
  if (grub_strcmp (name, "border_color") == 0)
    {
       grub_video_parse_color (value, &self->border_color);
    }
  else if (grub_strcmp (name, "bg_color") == 0)
    {
       grub_video_parse_color (value, &self->bg_color);
    }
  else if (grub_strcmp (name, "fg_color") == 0)
    {
      grub_video_parse_color (value, &self->fg_color);
    }
  else if (grub_strcmp (name, "bar_style") == 0)
    {
      self->need_to_recreate_pixmaps = 1;
      self->pixmapbar_available = 1;
      grub_free (self->bar_pattern);
      self->bar_pattern = value ? grub_strdup (value) : 0;
    }
  else if (grub_strcmp (name, "highlight_style") == 0)
    {
      self->need_to_recreate_pixmaps = 1;
      self->pixmapbar_available = 1;
      grub_free (self->highlight_pattern);
      self->highlight_pattern = value ? grub_strdup (value) : 0;
    }
  else if (grub_strcmp (name, "highlight_overlay") == 0)
    {
      self->highlight_overlay = grub_strcmp (value, "true") == 0;
    }
  else if (grub_strcmp (name, "theme_dir") == 0)
    {
      self->need_to_recreate_pixmaps = 1;
      grub_free (self->theme_dir);
      self->theme_dir = value ? grub_strdup (value) : 0;
    }
  else if (grub_strcmp (name, "id") == 0)
    {
      grub_gfxmenu_timeout_unregister ((grub_gui_component_t) self);
      grub_free (self->id);
      if (value)
        self->id = grub_strdup (value);
      else
        self->id = 0;
    }
  else if (grub_strcmp (name, "activity_finish") == 0)
  {
    if (self->step != 0) activity_bar_finish(self);
  }
  return grub_errno;
}


static struct grub_gui_component_ops activity_bar_ops =
{
  .destroy = activity_bar_destroy,
  .get_id = activity_bar_get_id,
  .is_instance = activity_bar_is_instance,
  .paint = activity_bar_paint,
  .set_parent = activity_bar_set_parent,
  .get_parent = activity_bar_get_parent,
  .set_bounds = activity_bar_set_bounds,
  .get_bounds = activity_bar_get_bounds,
  .get_minimal_size = activity_bar_get_minimal_size,
  .set_property = activity_bar_set_property
};


static struct grub_gui_component_ops progress_bar_ops =
{
  .destroy = activity_bar_destroy,
  .get_id = activity_bar_get_id,
  .is_instance = activity_bar_is_instance,
  .paint = progress_bar_paint,
  .set_parent = activity_bar_set_parent,
  .get_parent = activity_bar_get_parent,
  .set_bounds = activity_bar_set_bounds,
  .get_bounds = activity_bar_get_bounds,
  .get_minimal_size = activity_bar_get_minimal_size,
  .set_property = activity_bar_set_property
};


static struct grub_gui_progress_ops activity_bar_pg_ops =
{
  .set_state = activity_bar_set_state
};

static struct grub_gui_progress_ops progress_bar_pg_ops =
{
  .set_state = progress_bar_set_state
};


static grub_video_rgba_color_t black = { .red = 0, .green = 0, .blue = 0, .alpha = 255 };
static grub_video_rgba_color_t gray = { .red = 128, .green = 128, .blue = 128, .alpha = 255 };
static grub_video_rgba_color_t lightgray = { .red = 200, .green = 200, .blue = 200, .alpha = 255 };


grub_gui_component_t
grub_miray_gui_activity_bar_new (void)
{
  grub_gui_activity_bar_t self;
  self = grub_zalloc (sizeof (*self));
  if (! self)
    return 0;

  self->activity.ops  = &activity_bar_pg_ops;
  self->activity.component.ops = &activity_bar_ops;
  self->visible = 1;
  self->step = bar_step;
  self->highlight_width = bar_highlight_width;
  self->value = bar_step - bar_highlight_width;
  self->border_color = black;
  self->bg_color = gray;
  self->fg_color = lightgray;
  self->highlight_overlay = 0;

  return (grub_gui_component_t) self;
}


grub_gui_component_t
grub_miray_gui_progress_bar_new (void)
{
  grub_gui_activity_bar_t self;
  self = grub_zalloc (sizeof (*self));
  if (! self)
    return 0;

  self->activity.ops  = &progress_bar_pg_ops;
  self->activity.component.ops = &progress_bar_ops;
  self->visible = 1;
  self->step = 0;
  self->highlight_width = 0;
  self->value = 0;
  self->end = 0;
  self->border_color = black;
  self->bg_color = gray;
  self->fg_color = lightgray;
  self->highlight_overlay = 0;

  return (grub_gui_component_t) self;
}
