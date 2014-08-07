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

#include <grub/dl.h>
#include <grub/err.h>
#include <grub/command.h>
#include <grub/normal.h>
#include <grub/time.h>

#include "miray_screen.h"
#include <grub/miray_debug.h>

extern char ** miray_clone_stringlist(char ** list);
extern void miray_free_stringlist(char ** list);


GRUB_MOD_LICENSE ("GPLv3+");

extern struct miray_screen * miray_gfx_screen_new(struct grub_term_output *term);
static int active = 0;

static char ** _gfx_error_msg = 0;


static grub_err_t
miray_gfx_error_msg(struct grub_command *cmd __attribute__ ((unused)),
   int argc , char *argv[])
{
  if (argc == 0)
     return grub_error(GRUB_ERR_BAD_ARGUMENT, "Missing argument");

  if (_gfx_error_msg != 0)
     miray_free_stringlist(_gfx_error_msg);
  _gfx_error_msg = miray_clone_stringlist(argv);

  return GRUB_ERR_NONE;
}

static int getkey_norefresh(void)
{
   while (1)
   {
      int ret = grub_getkey_noblock ();
      if (ret != GRUB_TERM_NO_KEY)
         return ret;
      grub_cpu_idle ();
    }
}

static grub_err_t
cmd_gfx_check(struct grub_command *cmd __attribute__ ((unused)),
              int argc __attribute__ ((unused)), char *argv[] __attribute__ ((unused)))
{
   if (active)
      return GRUB_ERR_NONE;
   else if (_gfx_error_msg == 0)
      return grub_error(GRUB_ERR_MENU, "GFX menu not active");
   else
   {
      miray_screen_message_box((const char **)_gfx_error_msg, 0);
      if (grub_term_inputs)
      {
         getkey_norefresh ();
      }

      grub_reboot();
   }
}


static grub_command_t _cmd_gfx_error_msg, _cmd_gfx_check;

GRUB_MOD_INIT(miray_gfx_bootscreen)
{
   _cmd_gfx_error_msg = grub_register_command("miray_gfx_error_msg",
                                              miray_gfx_error_msg,
                                              0, N_("set message for gfx errors"));

   _cmd_gfx_check = grub_register_command("miray_gfx_check",
                                          cmd_gfx_check,
                                          0, N_("Check if gfx menu is active"));

   struct miray_screen * scr = miray_gfx_screen_new(grub_term_outputs);
   if (scr != 0)
   {
      if (miray_screen_set_screen(scr) == GRUB_ERR_NONE)
      {
         active = 1;
      }
      else
      {
         if (miray_debugmode())
         {
            grub_error(GRUB_ERR_MENU, "Could not set gfx menu");
            grub_wait_after_message();
         }
         miray_screen_destroy(scr);
         scr = NULL;
      }
   }
   else
   {
      if (miray_debugmode())
      {
         grub_error(GRUB_ERR_MENU, "Could not create gfx screen");
         grub_wait_after_message();
      }
   }

   while (grub_error_pop()) {};
}

GRUB_MOD_FINI(miray_gfX_bootscreen)
{
   if (active)
      miray_screen_set_screen(0);

   if (_gfx_error_msg != NULL)
   {
      miray_free_stringlist(_gfx_error_msg);
      _gfx_error_msg = NULL;
   }

   grub_unregister_command (_cmd_gfx_error_msg);
   grub_unregister_command (_cmd_gfx_check);
}
