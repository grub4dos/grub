/*
 *  Extension for GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2010-2019 Miray Software <oss@miray.de>
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

#include "miray_cmd_boothelper.h"

#include <grub/dl.h>
#include <grub/misc.h>
#include <grub/command.h>
#include <grub/i18n.h>
#include <grub/env.h>
#include <grub/time.h>
#include <grub/mm.h>
//#include <grub/i386/pc/kernel.h>
#include <grub/device.h>
#include <grub/extcmd.h>
#include <grub/acpi.h>
#include <grub/fs.h>
#include <grub/file.h>
#include <grub/video.h>

GRUB_MOD_LICENSE ("GPLv3+");


static const struct grub_arg_option options_bootdev[] =
{
   {"set",             's', 0,
      ("Set a variable to return value."), "VAR", ARG_TYPE_STRING},
};

static grub_err_t
miray_cmd_bootdev (grub_extcmd_context_t ctxt,
                   int argc __attribute__ ((unused)),
                   char **args __attribute__ ((unused)))
{
   struct grub_arg_list *state = ctxt->state;
   const char * env_key = "bootdev";
   char * dev = NULL;

   if (state[0].set)
      env_key = state[0].arg;

   dev = miray_machine_bootdev();
   if (!dev)
      return grub_errno;

   grub_env_set(env_key, dev);

   grub_free(dev);

   return GRUB_ERR_NONE;
}

static const struct grub_arg_option options_tolower[] =
{
   {"set",             's', 0,
      ("Set a variable to return value."), "VAR", ARG_TYPE_STRING},
};

static grub_err_t
miray_cmd_asciitolower (grub_extcmd_context_t ctxt, int argc, char **args)
{
   struct grub_arg_list *state = ctxt->state;
   char * out;
   char * p;

   if (argc < 1)
      return grub_error(GRUB_ERR_BAD_ARGUMENT, "Needs argument");

   out = grub_strdup(args[0]);
   if (!out)
      return grub_error(GRUB_ERR_BAD_ARGUMENT, "Empty string");

   for (p = out; *p != '\0'; ++p)
      *p = (char)grub_tolower(*p);

   if (state[0].set)
   {
      grub_env_set(state[0].arg, out);
   }
   else
   {
      grub_printf("%s", out);
   }

   grub_free(out);

   return GRUB_ERR_NONE;
}


static const struct grub_arg_option options_acpi2[] =
{
   {"set",             's', 0,
      ("Set a variable to return value."), "VAR", ARG_TYPE_STRING},
};

#define ACPI2_BUFFER_SIZE 40

static grub_err_t
miray_cmd_acpi2 (grub_extcmd_context_t ctxt,
                   int argc __attribute__ ((unused)),
                   char **args __attribute__ ((unused)))
{
   struct grub_arg_list *state = ctxt->state;
   const char * env_key = "acpi2_start";

   char buffer[ACPI2_BUFFER_SIZE + 1];

   struct grub_acpi_rsdp_v20 *a = 0;

   if (state[0].set)
      env_key = state[0].arg;

   a = grub_machine_acpi_get_rsdpv2 ();

   if (!a)
      return grub_error(GRUB_ERR_TEST_FAILURE, "ACPI 2 not supported");

   grub_snprintf(buffer, ACPI2_BUFFER_SIZE, "%p", (void *)a);

   grub_env_set(env_key, buffer);

   return GRUB_ERR_NONE;
}


static const struct grub_arg_option options_smbios[] =
{
   {"set",             's', 0,
      ("Set a variable to return value."), "VAR", ARG_TYPE_STRING},
};

#define SMBIOS_BUFFER_SIZE 40

static grub_err_t
miray_cmd_smbios (grub_extcmd_context_t ctxt,
                   int argc __attribute__ ((unused)),
                   char **args __attribute__ ((unused)))
{
   struct grub_arg_list *state = ctxt->state;
   const char * env_key = "smbios_start";

   char buffer[SMBIOS_BUFFER_SIZE + 1];

   void * ptr = 0;

   if (state[0].set)
      env_key = state[0].arg;

   ptr = miray_machine_smbios_ptr();

   if (!ptr)
      return grub_error(GRUB_ERR_TEST_FAILURE, "SMBIOS not supported");

   grub_snprintf(buffer, SMBIOS_BUFFER_SIZE, "%p", ptr);

   grub_env_set(env_key, buffer);

   return GRUB_ERR_NONE;
}

static grub_err_t
miray_cmd_smbios3 (grub_extcmd_context_t ctxt,
                   int argc __attribute__ ((unused)),
                   char **args __attribute__ ((unused)))
{
   struct grub_arg_list *state = ctxt->state;
   const char * env_key = "smbios3_start";

   char buffer[SMBIOS_BUFFER_SIZE + 1];

   void * ptr = 0;

   if (state[0].set)
      env_key = state[0].arg;

   ptr = miray_machine_smbios3_ptr();

   if (!ptr)
      return grub_error(GRUB_ERR_TEST_FAILURE, "SMBIOS3 not supported");

   grub_snprintf(buffer, SMBIOS_BUFFER_SIZE, "%p", ptr);

   grub_env_set(env_key, buffer);

   return GRUB_ERR_NONE;
}



static grub_err_t
miray_cmd_check_fbaddr(grub_extcmd_context_t ctxt __attribute__ ((unused)),
                     int argc __attribute__ ((unused)),
                     char **args __attribute__ ((unused)) )
{
   /* Check if the current frame buffer address fits into a 32bit pointer
    * We use this to print an error message when booting a 32bit kernel
    */

   #if GRUB_CPU_SIZEOF_VOID_P == 8
   if ((grub_uint64_t) grub_get_fb() > 0xFFFFFFFFu)
   {
      return grub_error(GRUB_ERR_TEST_FAILURE, N_("false"));
   }
   #endif

   return GRUB_ERR_NONE;
}

static grub_err_t
miray_cmd_test_file(struct grub_command *cmd __attribute__ ((unused)),
                    int argc, char **args)
{
   // This method allows to test for files even if the directory does not support iterating over a directory

   grub_file_t f;

   if (argc < 1)
      return grub_error(GRUB_ERR_BAD_ARGUMENT, "Argument missing");

   // Don't try to decompress or check signatures
   // We only want to check if the file exists
   f = grub_file_open(args[0], GRUB_FILE_TYPE_NONE | GRUB_FILE_TYPE_NO_DECOMPRESS | GRUB_FILE_TYPE_SKIP_SIGNATURE);

   if (f != NULL)
   {
      grub_file_close(f);
      return GRUB_ERR_NONE;
   }
   else
   {
      return grub_error(GRUB_ERR_TEST_FAILURE, "Could not open");
   }
}


static grub_extcmd_t cmd_bootdev, cmd_tolower, cmd_acpi2, cmd_smbios, cmd_smbios3, cmd_check_fbaddr;

static grub_command_t cmd_test_file;

GRUB_MOD_INIT(boothelper)
{
   cmd_bootdev = grub_register_extcmd ("miray_bootdev", miray_cmd_bootdev,
				     0, 0, N_("Set bootdev variable"), options_bootdev);

   cmd_tolower = grub_register_extcmd ("miray_tolower", miray_cmd_asciitolower,
				     0, 0, N_("Change ascii characters to lower"), options_tolower);

   cmd_acpi2 = grub_register_extcmd ("miray_acpi2", miray_cmd_acpi2,
				   0, 0, N_("Set acpi2 addres to variable"), options_acpi2);

   cmd_smbios = grub_register_extcmd ("miray_smbios", miray_cmd_smbios,
				    0, 0, N_("Set smbios addres to variable"), options_smbios);

   cmd_smbios3 = grub_register_extcmd ("miray_smbios3", miray_cmd_smbios3,
				    0, 0, N_("Set smbios addres to variable"), options_smbios);


   cmd_check_fbaddr = grub_register_extcmd ("miray_check_fbaddr", miray_cmd_check_fbaddr,
				    0, 0, N_("Check if framebuffer is addressable with 32 bit"), 0);

   cmd_test_file = grub_register_command("miray_test_file",
				      miray_cmd_test_file,
				      0, N_("Test if a file exists"));

}

GRUB_MOD_FINI(boothelper)
{
   grub_unregister_extcmd(cmd_bootdev);
   grub_unregister_extcmd(cmd_tolower);
   grub_unregister_extcmd(cmd_acpi2);
   grub_unregister_extcmd(cmd_smbios);
   grub_unregister_extcmd(cmd_smbios3);
   grub_unregister_extcmd(cmd_check_fbaddr);

   grub_unregister_command(cmd_test_file);
}

// GRUB_MOD_DEP(drivemap);
// GRUB_MOD_DEP(boot);
