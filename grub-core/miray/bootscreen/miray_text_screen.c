/*
 *  Extension for GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2010-2017 Miray Software <oss@miray.de>
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
#include <grub/normal.h>
#include <grub/menu.h>
#include <grub/charset.h>
#include <grub/time.h>

#include "miray_bootscreen.h"
#include "miray_screen.h"
#include "text_progress_bar.h"


/* Constants for bootscreen */
static const unsigned int miray_pos_msg_starting_top = 13;
static const unsigned int miray_pos_activity_bar_top = 15;
static const unsigned int miray_size_activity_bar = 24;

struct miray_text_screen_data {
   struct text_bar *bar1;
   struct text_bar *bar2;
   char * menutext;
   char * warning;
   int show_bar;
   int show_activity;
};
typedef struct miray_text_screen_data mts_data_t;

struct miray_screen * miray_text_screen_new(struct grub_term_output *);


/* Returns the offset necessary to center a element */
static inline grub_uint32_t offset_center(grub_uint32_t size, struct grub_term_output *term)
{
   if (size >= grub_term_width(term))
      return 0;

   return ((grub_term_width(term) - size) / 2);
}

static inline grub_size_t grub_utf8len(const char *str)
{
   if (!str)
      return 0;
   
   grub_uint32_t * tmp = 0;
   grub_ssize_t ret = grub_utf8_to_ucs4_alloc(str, &tmp, 0);
   if (ret < 0)
      ret = grub_strlen(str);
   if (tmp != 0)
      grub_free(tmp);

   return ret;
}


static char * menu_line(unsigned term_width, grub_menu_t menu )
{
   const char * spacer = "";
   grub_size_t len = 0;

   char * ret = grub_strdup("");

   int i;
   grub_menu_entry_t entry;
   for (i = 0, entry = menu->entry_list;
        i < menu->size && entry != 0; i++, entry = entry->next)
   {
      char * entry_title;
      char * tmp;

      entry_title = miray_screen_entry_name(entry);
      if (!entry_title)
         continue;
      
      tmp = grub_xasprintf("%s%s%s", ret, spacer, entry_title);
      grub_free(entry_title);
      
      if (!tmp)
         break;

      spacer = "   ";
      grub_free(ret);
      ret = tmp;

      len = grub_strlen(ret);
      if (term_width > 0 && len >= term_width)
         break;
   }

   if (len == 0)
   {
      grub_free(ret);
      return NULL;
   }

   // Cap string to term_width - 1
   if (term_width > 0 && len >= term_width)
      ret[term_width - 1] = '\0';

   return ret;
}

static grub_err_t miray_text_screen_destroy(struct miray_screen *scr)
{
   mts_data_t * data = scr->data;

   text_bar_destroy(data->bar1);
   text_bar_destroy(data->bar2);
   grub_free(data->menutext);
   if (data->warning)
      grub_free(data->warning);
   grub_free (data);
   grub_free (scr);

   return GRUB_ERR_NONE;
}

static void miray_text_screen_reset (struct miray_screen *scr)
{
   mts_data_t * data = scr->data;

   data->show_bar = -1;
   data->show_activity = 0;
}

inline static void miray_text_screen_put_line(grub_term_output_t term, int y, const char *message)
{
   unsigned int offset = offset_center(grub_strlen(message), term);
   unsigned int i;
   struct grub_term_coordinate pos = { .x = 0, .y = y};

   grub_term_gotoxy(term, pos);
   for (i= 0; i < offset; i++)
      grub_putcode(' ', term);

   grub_puts_terminal(message, term);

   for (i = offset + grub_strlen(message); i < grub_term_width(term) - 1; i++)
      grub_putcode(' ', term);
}

#pragma GCC diagnostic ignored "-Wformat-nonliteral"

static void miray_text_screen_put_text(struct miray_screen *scr)
{
   mts_data_t * data = scr->data;

   int timeout = grub_menu_get_timeout();
   if (data->show_activity && screen_data.activity_descr != 0 && screen_data.activity_descr[0] != '\0')
   {
      miray_text_screen_put_line (scr->term, miray_pos_msg_starting_top, screen_data.activity_descr);
   }
   else if (timeout >= 0 && screen_data.timeout_message != 0)
   {
      miray_text_screen_put_line (scr->term, miray_pos_msg_starting_top, screen_data.timeout_message);

      grub_uint8_t normal_color_save    = grub_term_normal_color;
      grub_uint8_t highlight_color_save = grub_term_highlight_color;
      grub_uint8_t normal_color;
      grub_parse_color_name_pair (&normal_color, "dark-gray/black");
      grub_term_normal_color = normal_color;
      grub_term_setcolorstate (scr->term, GRUB_TERM_COLOR_NORMAL);

      if (screen_data.timeout_format != 0 && grub_printf_fmt_check(screen_data.timeout_format, "%d") == GRUB_ERR_NONE)
      {
         char *timeout_str = 0;
         timeout_str = grub_xasprintf(screen_data.timeout_format, timeout);
         miray_text_screen_put_line (scr->term, miray_pos_msg_starting_top + 2, timeout_str);
         grub_free(timeout_str);
      }

      if (screen_data.timeout_stop != 0)
         miray_text_screen_put_line (scr->term, miray_pos_msg_starting_top + 4, screen_data.timeout_stop);
      grub_term_normal_color    = normal_color_save;
      grub_term_highlight_color = highlight_color_save;
      grub_term_setcolorstate (scr->term, GRUB_TERM_COLOR_NORMAL);
   }
   else if (screen_data.default_message != 0)
   {
      miray_text_screen_put_line (scr->term, miray_pos_msg_starting_top, screen_data.default_message);
   }

   if (data->show_bar > 0)
   {
      text_bar_draw(data->bar1);
      if (screen_data.stage_max > 1)
      {
         text_bar_draw(data->bar2);
      }
   }

   if (data->show_bar < 0 && data->menutext!= 0)
   {
      miray_text_screen_put_line (scr->term, grub_term_height(scr->term) - 1, data->menutext);
   }
   {
      struct grub_term_coordinate pos = { .x = 0, .y = miray_pos_msg_starting_top + 2 };
      grub_term_gotoxy(scr->term, pos);
   }

   if (data->warning != 0)
   {
      static const char * warning_color = "yellow/black";

      grub_uint8_t old_normal_color    = grub_term_normal_color;
      grub_uint8_t this_normal_color;

      grub_parse_color_name_pair(&this_normal_color, warning_color);
      grub_term_normal_color = this_normal_color;
      grub_term_setcolorstate(scr->term, GRUB_TERM_COLOR_NORMAL);

      miray_text_screen_put_line (scr->term, grub_term_height(scr->term) - 7, data->warning);

      grub_term_normal_color = old_normal_color;
      grub_term_setcolorstate(scr->term, GRUB_TERM_COLOR_NORMAL);
   }
}

#pragma GCC diagnostic error "-Wformat-nonliteral"

static void miray_text_screen_clear(struct miray_screen *scr)
{
   if (scr->term == 0)
      return;

   grub_term_cls(scr->term);
}


static void miray_text_screen_redraw (struct miray_screen *scr)
{
   if (scr->term == 0)
      return;

   grub_term_setcursor (scr->term, 0);
   grub_term_cls (scr->term);

   miray_text_screen_put_text(scr);
}

static void miray_text_screen_redraw_text (struct miray_screen *scr, int clear)
{
  if (scr->term == 0)
     return;

  if (clear)
  {
    // clear message region,
    unsigned int y;
    for (y = miray_pos_msg_starting_top; y < grub_term_height(scr->term) -1; y++)
    {
      struct grub_term_coordinate pos = { .x = 0, .y = y };
      grub_term_gotoxy(scr->term, pos);
      unsigned int x;
      for(x = grub_term_width(scr->term); x > 0; x--)
      {
        grub_putcode(' ', scr->term);
      }
    }

    // don't overwrite the character in the bottom right corner to
    // avoid accidential scrolling
    struct grub_term_coordinate pos = { .x = 0, .y = grub_term_height(scr->term) - 1 };
    grub_term_gotoxy(scr->term, pos);
    int x;
    for(x = grub_term_width(scr->term); x > 1; x--)
    {
      grub_putcode(' ', scr->term);
    }
  }

  miray_text_screen_put_text(scr);
}

static void miray_text_screen_set_splash_menu(struct miray_screen *scr, grub_menu_t menu)
{
   mts_data_t * data = scr->data;

   if (data->menutext != 0)
   {
      grub_free(data->menutext);
      data->menutext = 0;
   }

   if (menu == 0) return;

   data->menutext = menu_line(grub_term_width(scr->term), menu);
}

static int miray_text_screen_run_submenu(struct miray_screen *scr __attribute__ ((unused)), grub_menu_t menu)
{
   return miray_run_menu(menu);
}

static grub_err_t miray_text_screen_property(struct miray_screen *scr, const char * name, const char * value)
{
   mts_data_t * data = scr->data;

   if (grub_strcmp(name, "activity") == 0)
   {
      if (value != 0)
      {
         data->show_bar = 0;
         data->show_activity = 1;
      }
      else
      {
         data->show_bar = -1;
         data->show_activity = 0;
      }
   }
   else if (grub_strcmp(name, "stage") == 0)
   {
      if (screen_data.stage > 1)
      {
         text_bar_set_progress(data->bar1, 1, 1);
         //text_bar_set_progress(data->bar2, 0, 1);
      }
   }
   return GRUB_ERR_NONE;
}

static void miray_text_screen_set_progress(struct miray_screen *scr, grub_uint64_t cur, grub_uint64_t max)
{
   struct miray_text_screen_data *data = scr->data;

   if (data && data->show_bar >= 0)
   {
      data->show_bar = 1;
      if (screen_data.stage <= screen_data.stage_max)
      {
         switch(screen_data.stage)
         {
         case 0:
         case 1:
            text_bar_set_progress(data->bar1, cur, max);
            text_bar_set_progress(data->bar2, 0, 1);
            break;
         case 2:
            text_bar_set_progress(data->bar1, 1, 1);
            text_bar_set_progress(data->bar2, cur, max);
            break;
         }
      }

      text_bar_draw(data->bar1);
      if (screen_data.stage_max > 1)
      {
         text_bar_draw(data->bar2);
      }
   }
}


static void miray_text_screen_finish(struct miray_screen *scr)
{
   if (scr->term == 0)
      return;
   struct miray_text_screen_data * data = scr->data;

   if (data->show_bar > 0)
   {
      text_bar_finish(data->bar1);
      if (screen_data.stage_max > 1)
         text_bar_finish(data->bar2);
   }
}


static void miray_text_screen_warning(struct miray_screen * scr, const char * warning)
{
   mts_data_t * data = scr->data;

   if (data->warning)
     grub_free(data->warning);

   if (warning)
      data->warning = grub_strdup(warning);
   else
      data->warning = 0;

   miray_text_screen_redraw_text(scr, 1);
}


static void miray_text_screen_message_box(struct miray_screen *scr, const char ** message, const char * color)
{
   if (!scr->term)
      return;

   static const grub_uint32_t c_ul = GRUB_UNICODE_CORNER_UL;
   static const grub_uint32_t c_ur = GRUB_UNICODE_CORNER_UR;
   static const grub_uint32_t c_bl = GRUB_UNICODE_CORNER_LL;
   static const grub_uint32_t c_br = GRUB_UNICODE_CORNER_LR;
   static const grub_uint32_t c_h  = GRUB_UNICODE_HLINE;
   static const grub_uint32_t c_v  = GRUB_UNICODE_VLINE;
   static const grub_uint32_t c_sp = ' ';

   static const char * box_default_color = "white/red";

   if (color == 0) color = box_default_color;

   static const int v_spaces = 2;

   int lines = 0;
   grub_size_t max_len = 0;

   grub_uint8_t old_normal_color    = grub_term_normal_color;
   grub_uint8_t old_highlight_color = grub_term_highlight_color;
   grub_uint8_t this_normal_color;

   grub_parse_color_name_pair(&this_normal_color, color);
   grub_term_normal_color = this_normal_color;
   grub_term_setcolorstate(scr->term, GRUB_TERM_COLOR_NORMAL);

   while (message[lines] != 0)
   {
      // grub_strlen does not count utf8 characters correctly
      grub_size_t this_len = grub_utf8len(message[lines]);
      max_len = grub_max(max_len, this_len);
      lines++;
   }

   if ((max_len % 2) != 0)
      max_len++;

   const int b_lines = lines + 4;
   const int b_columns = max_len + 2 + 2 * v_spaces;


   const grub_uint32_t y_offset = miray_pos_activity_bar_top;
   const grub_uint32_t x_offset = offset_center(b_columns, scr->term);

   {
      int x;
      struct grub_term_coordinate pos = { .x = x_offset, .y = y_offset };
      grub_term_gotoxy(scr->term, pos);
      grub_putcode(c_ul, scr->term);
      for (x = 1; x < b_columns - 1; x++)
         grub_putcode(c_h, scr->term);
      grub_putcode(c_ur, scr->term);
   }

   {
      int x;
      struct grub_term_coordinate pos = { .x = x_offset, .y = y_offset + 1 };
      grub_term_gotoxy(scr->term, pos);
      grub_putcode(c_v, scr->term);
      for (x = 1; x < b_columns - 1; x++)
         grub_putcode(c_sp, scr->term);
      grub_putcode(c_v, scr->term);
   }
   const grub_uint32_t y_text_offset = y_offset + 2;

   unsigned int y;
   for (y = y_text_offset; y < y_text_offset + lines; y++)
   {
      struct grub_term_coordinate pos = { .x = x_offset, .y = y };
      const char * str = message[y - y_text_offset];
      grub_term_gotoxy(scr->term, pos);
      grub_putcode(c_v, scr->term);
      int x;
      for (x = 1; x < 1 + v_spaces; x++)
         grub_putcode(c_sp, scr->term);
      grub_puts_terminal(str, scr->term);

      for (x = grub_utf8len(str) + 1 + v_spaces; x < b_columns - 1; x++)
      {
        grub_putcode(c_sp, scr->term);
      }
      grub_putcode(c_v, scr->term);
   }

   {
      int x;
      struct grub_term_coordinate pos = { .x = x_offset, .y = y_offset + b_lines - 2 };
      grub_term_gotoxy(scr->term, pos);
      grub_putcode(c_v, scr->term);
      for (x = 1; x < b_columns - 1; x++)
         grub_putcode(c_sp, scr->term);
      grub_putcode(c_v, scr->term);
   }
   {
      int x;
      struct grub_term_coordinate pos = { .x = x_offset, .y = y_offset + b_lines - 1 };
      grub_term_gotoxy(scr->term, pos);
      grub_putcode(c_bl, scr->term);
      for (x = 1; x < b_columns - 1; x++)
         grub_putcode(c_h, scr->term);
      grub_putcode(c_br, scr->term);
   }

   grub_term_normal_color    = old_normal_color;
   grub_term_highlight_color = old_highlight_color;
   grub_term_setcolorstate(scr->term, GRUB_TERM_COLOR_NORMAL);
}


static char *
miray_text_screen_submenu_center_string(struct miray_screen *scr __attribute__ ((unused)), const char * string)
{
   int len = grub_strlen(string);
   int pad = (grub_term_width(grub_term_outputs) - len -5) /2;

   char * new_string = grub_malloc(sizeof(char) * (len + pad + 1));
   grub_memset(new_string, ' ', len + pad);
   new_string[len + pad] = '\0';

   grub_memcpy(&new_string[pad], string, len);

   return new_string;
}


struct miray_screen *
miray_text_screen_new(struct grub_term_output *term)
{
   struct miray_screen *ret = grub_zalloc(sizeof(struct miray_screen));
   if (ret == NULL)
   {
      grub_dprintf("miray", "%s: Out of memory\n", __FUNCTION__);
      grub_error(GRUB_ERR_OUT_OF_MEMORY, "Out of Memory");
      return NULL;
   }

   grub_dprintf("miray", "%s\n", __FUNCTION__);

   ret->destroy = miray_text_screen_destroy;
   ret->reset   = miray_text_screen_reset;
   ret->clear   = miray_text_screen_clear;
   ret->redraw  = miray_text_screen_redraw;
   ret->redraw_text = miray_text_screen_redraw_text;

   ret->set_splash_menu = miray_text_screen_set_splash_menu;
   ret->run_submenu = miray_text_screen_run_submenu;

   ret->property = miray_text_screen_property;
   ret->set_progress = miray_text_screen_set_progress;

   ret->warning = miray_text_screen_warning;

   ret->message_box = miray_text_screen_message_box;
   ret->submenu_center_entry = miray_text_screen_submenu_center_string;

   ret->finish = miray_text_screen_finish;

   struct miray_text_screen_data * data = grub_zalloc(sizeof(struct miray_text_screen_data));
   if (data == NULL)
   {
      grub_free(ret);
      return NULL;
   }
   ret->data = data;
   ret->term = term;

   data->bar1 = text_progress_bar_new(term, offset_center(miray_size_activity_bar, term), miray_pos_activity_bar_top, miray_size_activity_bar);
   data->bar2 = text_progress_bar_new(term, offset_center(miray_size_activity_bar, term), miray_pos_activity_bar_top + 1, miray_size_activity_bar);

   /* Initialize Values */
   miray_text_screen_reset(ret);

   return ret;
}


