/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) Miray Software 2016
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
#include <grub/command.h>
#include <grub/extcmd.h>
#include <grub/datetime.h>
#include <grub/env.h>
#include <grub/err.h>

GRUB_MOD_LICENSE ("GPLv3+");

static const char * default_env = "last_boot";


static int
get_current_timestamp(void)
{
   struct grub_datetime now;
   grub_get_datetime(&now);

   return (((int)now.day * 24 + now.hour) * 60 + now.minute) * 60 + now.second;
}

static const struct grub_arg_option options_boottime_check[] =
{
   {"env",             'e', 0,
      ("Set a variable to return value."), "VAR", ARG_TYPE_STRING},
};

static grub_err_t
miray_boottime_check(grub_extcmd_context_t ctxt,
                   int argc,
                   char **args)
{
   struct grub_arg_list *state = ctxt->state;
   const char * env_key = default_env;
   const char * envval;
   int span = 5 * 60; // Default to 5 minutes
   int val;
   int now;

   if (state[0].set)
      env_key = state[0].arg;

   if (argc > 0)
   {
      int s = (int) grub_strtoul (args[0], 0, 0);
      if (s)
         span = s;
   }

   envval = grub_env_get(env_key);
   if (!envval)
      return GRUB_ERR_NONE;

   val = (int) grub_strtoul (envval, 0, 0);
   now = get_current_timestamp();

   /* We base the timestamp second within a month,
    * so the timestamp rolls over every month.
    * The simplified rollover handling has a chance to get the 
    * timout wrong if the computer is rebooted during the first 5 minutes of a month.
    * For our use case this is good enough and if avoids complicated date handling.
    */
   if (val == 0 || (val > now && now < span) || now - val < span)
      return GRUB_ERR_NONE;

   return grub_error(GRUB_ERR_TIMEOUT, "Boottime timed out");
}

static const struct grub_arg_option options_boottime_set[] =
{
   {"env",             'e', 0,
      ("Set a variable to return value."), "VAR", ARG_TYPE_STRING},
};

static grub_err_t
miray_boottime_set(grub_extcmd_context_t ctxt,
                   int argc __attribute__ ((unused)),
                   char **args __attribute__ ((unused)))
{
#define SET_BUFFER_SIZE 40
   struct grub_arg_list *state = ctxt->state;
   const char * env_key = default_env;

   char buffer[SET_BUFFER_SIZE + 1];

   if (state[0].set)
      env_key = state[0].arg;

   grub_snprintf(buffer, SET_BUFFER_SIZE, "%d", get_current_timestamp());

   grub_env_set(env_key, buffer);

   return GRUB_ERR_NONE;
}

static grub_extcmd_t cmd_boottime_check, cmd_boottime_set;


GRUB_MOD_INIT(boot_native)
{
  cmd_boottime_check = grub_register_extcmd("miray_boottime_check", miray_boottime_check,
				       0, 0, N_("Check timestamp against defined elapsetime"), options_boottime_check);

  cmd_boottime_set = grub_register_extcmd ("miray_boottime_set", miray_boottime_set,
				       0, 0, N_(""), options_boottime_set);
}

GRUB_MOD_FINI(boot_native)
{
   grub_unregister_extcmd(cmd_boottime_check);
   grub_unregister_extcmd(cmd_boottime_set);
}
