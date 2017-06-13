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

#include "miray_cmd_boothelper.h"

#include <grub/misc.h>
#include <grub/mm.h>

#include <grub/efi/efi.h>
#include <grub/efi/disk.h>


// Remove the last section of a device path
// We use when booting from cd to get the cd device from the boot entry
// WARNING: modifies supplied path
static grub_efi_device_path_t * chomp_device_path(grub_efi_device_path_t *dp)
{
   grub_efi_device_path_t * last = grub_efi_find_last_device_path(dp);
   if (!last)
      return NULL;
   
   last->type      = (last->type & 0x80) | 0x7f;
   last->subtype   = GRUB_EFI_END_ENTIRE_DEVICE_PATH_SUBTYPE;
   last->length    = 4;

   return dp;
}


char *
miray_machine_bootdev(void)
{
   char * ret = NULL;
   grub_efi_loaded_image_t *image = NULL;
   char *device = NULL;

   image = grub_efi_get_loaded_image (grub_efi_image_handle);
   if (image == NULL)
   {
      grub_dprintf("miray", "No Image\n");
      grub_error(GRUB_ERR_UNKNOWN_DEVICE, "No loaded image");
      return NULL;
   }

   device = grub_efidisk_get_device_name (image->device_handle);

   if (device == NULL && grub_efi_net_config != NULL)
   {
      char * path = NULL;

      grub_efi_net_config(image->device_handle, &device, &path);
      if (device != NULL)
      {
         grub_dprintf("miray", "NET DEVICE (%s), <%s>", device, path);
      }

      grub_free(path);
   }

   if (device == NULL)
   {
      // Try to find a boot device in the path,
      // This works for cd image boot
      grub_efi_device_path_t *dp;
      grub_efi_device_path_t *dp_copy;

      dp = grub_efi_get_device_path (image->device_handle);
      if (!dp)
      {
        grub_dprintf("miray", "No device path\n");
        grub_error(GRUB_ERR_UNKNOWN_DEVICE, "No device path found");
        return NULL;
      }

      dp_copy = grub_efi_duplicate_device_path(dp);
      if (dp_copy)
      {  
         grub_efi_device_path_t * subpath = dp_copy;
   
         while ((subpath = chomp_device_path(subpath)) != 0)
         {
            device = miray_efi_get_diskname_from_path(subpath);
            if (device != 0)
               break;
         }
   
         grub_free(dp_copy);
      }
   }

   if (device == NULL)
   {
      grub_dprintf("miray", "No device\n");
      grub_error(GRUB_ERR_UNKNOWN_DEVICE, "Could not find boot device");
      return NULL;
   }

   ret = grub_xasprintf("(%s)", device);

   grub_free (device);

   return ret;
}


static grub_efi_packed_guid_t smbios_guid = GRUB_EFI_SMBIOS_TABLE_GUID;
static grub_efi_packed_guid_t smbios3_guid = GRUB_EFI_SMBIOS3_TABLE_GUID;

void *
miray_machine_smbios_ptr(void)
{
   /* this method is based on loadbios.c */

   void * smbios = 0;
   unsigned i;

   for (i = 0; i < grub_efi_system_table->num_table_entries; i++)
   {
      grub_efi_packed_guid_t *guid = &grub_efi_system_table->configuration_table[i].vendor_guid;

      if (! grub_memcmp (guid, &smbios_guid, sizeof (grub_efi_guid_t)))
      {
         smbios = grub_efi_system_table->configuration_table[i].vendor_table;
         grub_dprintf ("efi", "SMBIOS: %p\n", smbios);
      }
   }

   return smbios;
}


void *
miray_machine_smbios3_ptr(void)
{
   /* this method is based on loadbios.c */

   void * smbios = 0;
   unsigned i;

   for (i = 0; i < grub_efi_system_table->num_table_entries; i++)
   {
      grub_efi_packed_guid_t *guid = &grub_efi_system_table->configuration_table[i].vendor_guid;

      if (! grub_memcmp (guid, &smbios3_guid, sizeof (grub_efi_guid_t)))
      {
         smbios = grub_efi_system_table->configuration_table[i].vendor_table;
         grub_dprintf ("efi", "SMBIOS3: %p\n", smbios);
      }
   }

   return smbios;
}
