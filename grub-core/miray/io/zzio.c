/* zzio.c - miray header check */
/*
 *  Module for GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2010-2019 Miray Software <oss@miray.de>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <grub/types.h>
#include <grub/fs.h>
#include <grub/file.h>
#include <grub/mm.h>
#include <grub/misc.h>
#include <grub/dl.h>

GRUB_MOD_LICENSE ("GPLv3+");

static const grub_uint16_t zzMagic = 0x5A5A; /* 'ZZ' */

static const grub_uint8_t zzMaskMode = 0x0F;

#if 0 /* Values currently not needed */
static const grub_uint8_t zzModeNone = 0; /* invalid */
static const grub_uint8_t zzModeDeflate = 1;
#endif
static const grub_uint8_t zzModeLZMA = 2;

struct zzHeader
{
   grub_uint16_t magic;
   grub_uint8_t version; /**< 1 = lzw, 2 & 3 = deflate, 4 = use mode and flags */
   grub_uint8_t  minlzw; /**< only if version = 1 */
   grub_uint32_t ucompSize;
   grub_uint32_t compSize; /**< only if version >= 3 */
   grub_uint32_t res;
} __attribute__((packed));

struct grub_zzio {
   grub_file_t file;
};
typedef struct grub_zzio *grub_zzio_p;

static struct grub_fs grub_zzio_fs;


static grub_file_t
grub_zzio_open (grub_file_t io, enum grub_file_type type __attribute__ ((unused)))
{
   grub_file_t file;
   grub_zzio_p zzio;
   struct zzHeader header;

   /* We use that combination for our test file command */
   if (type == (GRUB_FILE_TYPE_NONE | GRUB_FILE_TYPE_NO_DECOMPRESS | GRUB_FILE_TYPE_SKIP_SIGNATURE))
      return io;

   if (io->size != GRUB_FILE_SIZE_UNKNOWN && io->size < sizeof(header))
      return io;

   if (grub_file_tell(io) != 0)
      grub_file_seek(io, 0);
   if (grub_file_read(io, &header, sizeof(header)) < (grub_ssize_t)sizeof(header))
   {
      grub_file_seek(io, 0);

      if (io->size == 0)
         return io;
      else
         return 0;
   }

   if (header.magic != zzMagic)
   {
      grub_file_seek(io, 0);
      return io;
   }

   if (header.version != 4 || (header.minlzw & zzMaskMode) != zzModeLZMA)
   {
      grub_error(GRUB_ERR_READ_ERROR, "INVALID FILE");
      return 0;
   }

   file = (grub_file_t) grub_zalloc (sizeof (*file));
   if (! file)
   {
      grub_error (GRUB_ERR_OUT_OF_MEMORY, "Out of memory");
      return 0;
   }

   zzio = grub_zalloc (sizeof(*zzio));
   if (! zzio)
   {
      grub_error (GRUB_ERR_OUT_OF_MEMORY, "Out of memory");
      grub_free (file);
      return 0;
   }

   zzio->file = io;

   file->device = io->device;
   if (io->size == GRUB_FILE_SIZE_UNKNOWN)
      file->size = GRUB_FILE_SIZE_UNKNOWN;
   else
      file->size = io->size - sizeof(struct zzHeader);
   file->offset = 0;
   file->data = zzio;
   file->read_hook = 0;
   file->fs = &grub_zzio_fs;
   file->not_easily_seekable = io->not_easily_seekable;

   return file;
}


static grub_ssize_t
grub_zzio_read(struct grub_file *file, char *buf, grub_size_t len)
{
   grub_zzio_p zzio = file->data;
   grub_ssize_t ret = 0;

   if (zzio->file->offset != file->offset + sizeof(struct zzHeader))
      if (grub_file_seek(zzio->file, file->offset + sizeof(struct zzHeader)) == (grub_off_t) -1)
         return -1;

   ret = grub_file_read(zzio->file, buf, len);

   return ret;
}


static grub_err_t
grub_zzio_close(struct grub_file *file)
{
   grub_zzio_p zzio = file->data;
   file->data = 0;

   grub_file_close (zzio->file);
   grub_free(zzio);

   /* Don't close device twice */
   file->device = 0;

   return 0;
}


static struct grub_fs grub_zzio_fs =
{
   .name = "zzio",
   .fs_dir = 0,
   .fs_open = 0,
   .fs_read = grub_zzio_read,
   .fs_close = grub_zzio_close,
   .fs_label = 0,
   .next = 0
};


GRUB_MOD_INIT (lzmaio)
{
   grub_file_filter_register (GRUB_FILE_FILTER_ZZIO, grub_zzio_open);
}

GRUB_MOD_FINI (lzmaio)
{
   grub_file_filter_unregister (GRUB_FILE_FILTER_ZZIO);
}
