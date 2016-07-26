/*
 *  Extention for GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2010,2011 Miray Software <oss@miray.de>
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


#include <grub/normal.h>
#include <grub/types.h>
#include <grub/mm.h>
#include <grub/time.h>

#include "miray_bootscreen.h"
#include "text_progress_bar.h"

# define PBAR_CHAR GRUB_UNICODE_LIGHT_HLINE
# define PBAR_CHAR_HIGHLIGHT GRUB_UNICODE_LIGHT_HLINE

static const grub_uint32_t miray_progress_tick_throttle_ms = 5;

struct text_bar {
   void (*set_progress) (struct text_bar * bar, grub_uint64_t cur, grub_uint64_t max);
   void (*draw) (struct text_bar * bar);
   void (*finish) (struct text_bar * bar);

   grub_uint8_t  color_normal;
   grub_uint8_t  color_highlight;
   grub_uint32_t char_normal;
   grub_uint32_t char_highlight;
   grub_term_output_t term;
   struct grub_term_coordinate pos;
   int bar_len;
   int current_pos;
};


/* highlight_start and highlight_end: start and end of highlighted bar,
 * relative to textbar */
static void draw_textbar(struct text_bar *bar, int highlight_start, int highlight_end)
{
   if (bar->term == 0)
      return;

   int x;
   grub_uint32_t c;
   struct grub_term_coordinate pos_save;
   grub_uint8_t normal_color_save;
   grub_uint8_t highlight_color_save;

   pos_save = grub_term_getxy (bar->term);
   normal_color_save    = grub_term_normal_color;
   highlight_color_save = grub_term_highlight_color;

   grub_term_gotoxy (bar->term, bar->pos);
   grub_term_normal_color    = bar->color_normal;
   grub_term_highlight_color = bar->color_highlight;
   if (highlight_start > 0) {
      grub_term_setcolorstate (bar->term, GRUB_TERM_COLOR_NORMAL);
      c = bar->char_normal;
   } else {
      grub_term_setcolorstate (bar->term, GRUB_TERM_COLOR_HIGHLIGHT);
      c = bar->char_highlight;
   }

   for (x = 0; x < bar->bar_len; x++)
   {
      if (x == highlight_start)
      {
         grub_term_setcolorstate (bar->term, GRUB_TERM_COLOR_HIGHLIGHT);
         c = bar->char_highlight;
      }
      if (x == (highlight_end))
      {
         grub_term_setcolorstate (bar->term, GRUB_TERM_COLOR_NORMAL);
         c = bar->char_normal;
      }
      grub_putcode (c, bar->term);
   }

   /* see call in grub_term_restore_pos() */
   grub_term_gotoxy (bar->term, pos_save);

   grub_term_normal_color    = normal_color_save;
   grub_term_highlight_color = highlight_color_save;
   grub_term_setcolorstate (bar->term, GRUB_TERM_COLOR_NORMAL);
}


void text_bar_destroy(struct text_bar * bar)
{
   grub_free(bar);
}

void text_bar_set_progress(struct text_bar * bar, grub_uint64_t cur, grub_uint64_t max)
{
   if (!bar)
      return;
   
   if (bar->set_progress != 0)
      bar->set_progress(bar, cur, max);
}

void text_bar_draw(struct text_bar * bar)
{
   if (!bar)
      return;
   
   if (bar->draw != 0)
      bar->draw(bar);
}

void text_bar_set_term(struct text_bar * bar, grub_term_output_t term)
{
   if (!bar)
      return;
   
   bar->term = term;
   text_bar_draw(bar);
}

void text_bar_finish(struct text_bar * bar)
{
   if (!bar)
      return;
   
   if (bar->finish != 0)
      bar->finish(bar);
}


/*
 * Progress bar
 */


static void text_progress_bar_draw(struct text_bar * bar)
{
   draw_textbar (bar, 0, bar->current_pos);
}

static void text_progress_bar_set_progress(struct text_bar * bar, grub_uint64_t cur, grub_uint64_t max)
{
   if (max == 0 || cur > max)
      return;

   bar->current_pos = grub_divmod64(cur * bar->bar_len, max, 0);
}


static void text_progress_bar_finish(struct text_bar * bar)
{
   // This might not be necessary any more...

   for (; bar->current_pos <= bar->bar_len; bar->current_pos++)
   {
      grub_millisleep (miray_progress_tick_throttle_ms);
      draw_textbar(bar, 0, bar->current_pos);
   }
}


struct text_bar *
text_progress_bar_new_verbose(grub_term_output_t term, int offset_left, int offset_top, int len, grub_uint32_t char_normal, grub_uint32_t char_highlight, const char * color_normal, const char * color_highlight)
{
	struct text_bar *ret;

	ret = grub_malloc(sizeof(struct text_bar));
   if (!ret)
      return NULL;

   ret->set_progress = text_progress_bar_set_progress;
   ret->draw = text_progress_bar_draw;
   ret->finish = text_progress_bar_finish;

   ret->term = term;
	ret->pos.x = offset_left;
	ret->pos.y = offset_top;
	ret->bar_len = len;
	ret->char_normal = char_normal;
   ret->char_highlight = char_highlight;
	grub_parse_color_name_pair (&ret->color_normal, color_normal);
	grub_parse_color_name_pair (&ret->color_highlight, color_highlight);

	ret->current_pos = 0;

	return ret;

}

struct text_bar *
text_progress_bar_new (grub_term_output_t term, int offset_left, int offset_top, int len)
{
   return text_progress_bar_new_verbose(term, offset_left, offset_top, len, PBAR_CHAR, PBAR_CHAR_HIGHLIGHT, "dark-gray/black", "white/black");
}


/*
 * Activity bar
 */

static const int activity_bar_highlight_width = 3;


static void text_activity_bar_draw(struct text_bar * bar)
{
	draw_textbar (bar, bar->current_pos, bar->current_pos + activity_bar_highlight_width);
}

static void text_activity_bar_set_progress(struct text_bar * bar, grub_uint64_t cur __attribute__((unused)), grub_uint64_t max __attribute__((unused)))
{
	bar->current_pos++;
	if (bar->current_pos >= bar->bar_len)
		bar->current_pos = 1 - activity_bar_highlight_width;

	text_activity_bar_draw(bar);
}


static void text_activity_bar_finish(struct text_bar * bar)
{
	// Finish current run
   while (bar->current_pos > 0)
   {
      grub_millisleep (miray_progress_tick_throttle_ms * 10);
      text_activity_bar_set_progress(bar, 0, 0);
   }

   if (bar->current_pos <= 0)
      bar->current_pos = activity_bar_highlight_width;

   for (; bar->current_pos <= bar->bar_len; bar->current_pos++)
   {
      grub_millisleep (miray_progress_tick_throttle_ms);
      draw_textbar(bar, 0, bar->current_pos);
   }
}


struct text_bar *
text_activity_bar_new_verbose(grub_term_output_t term, int offset_left, int offset_top, int len, grub_uint32_t char_normal, grub_uint32_t char_highlight, const char * color_normal, const char * color_highlight)
{
   struct text_bar *ret;

   ret = grub_malloc(sizeof(struct text_bar));
   if (!ret)
      return NULL;

   ret->set_progress = text_activity_bar_set_progress;
   ret->draw = text_activity_bar_draw;
   ret->finish = text_activity_bar_finish;

   ret->term = term;
   ret->pos.x = offset_left;
   ret->pos.y = offset_top;
   ret->bar_len = len;
   ret->char_normal = char_normal;
   ret->char_highlight = char_highlight;
   grub_parse_color_name_pair (&ret->color_normal, color_normal);
   grub_parse_color_name_pair (&ret->color_highlight, color_highlight);

   ret->current_pos = 1 - activity_bar_highlight_width;

   return ret;
}

struct text_bar *
text_activity_bar_new (grub_term_output_t term, int offset_left, int offset_top, int len)
{
   return text_activity_bar_new_verbose(term, offset_left, offset_top, len, PBAR_CHAR, PBAR_CHAR_HIGHLIGHT, "dark-gray/black", "white/black");
}

