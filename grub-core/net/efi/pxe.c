
#include <grub/efi/api.h>
#include <grub/efi/efi.h>
#include <grub/misc.h>
#include <grub/net/efi.h>
#include <grub/charset.h>

static grub_efi_guid_t pxe_callback_guid = GRUB_EFI_PXE_CALLBACK_GUID;

grub_file_t current_pxe_file = NULL;
grub_uint32_t current_progress = 0;

static void send_current_progress (void)
{
  // Limit number of grub_file_progress_hook calls
  // to avoid slowing down the transfer too much
  current_pxe_file->device->net->offset += current_progress;

  if (grub_file_progress_hook)
  {
    grub_file_progress_hook (0, 0, current_progress, current_pxe_file);
  }
  current_progress = 0;
}

static grub_efi_base_code_callback_status_t GRUB_EFI_EFIAPI
grub_efi_net_pxe_callback (__attribute__((unused)) struct grub_efi_pxe_base_code_callback *this, 
                           __attribute__((unused)) grub_efi_pxe_base_code_function_t function,
                           grub_efi_boolean_t received,
                           grub_uint32_t packet_len,
                           void * packet)
{
  // We ignore the function value because of a bug in TianoCore.
  // Due to the bug function was never set to EFI_PXE_BASE_CODE_FUNCTION_MTFTP but to EFI_PXE_BASE_CODE_FUNCTION_DHCP
  // We set pxe_file while reading and we hope that this is enough....
  if (current_pxe_file != 0 && received && packet_len > 4)
  {
    grub_uint16_t * header = (grub_uint16_t*)packet;
    if (grub_be_to_cpu16(header[0]) == 3) // Data
    {
      current_progress += packet_len - 4;

      if (current_progress >= 16 * 1024)
        send_current_progress();
    }
  }

  return EFI_PXE_BASE_CODE_CALLBACK_STATUS_CONTINUE;
}

void grub_efi_net_register_pxe_callback(struct grub_efi_net_device *dev)
{
  if (dev->pxe_callback.callback != 0)
    return;

  if (!dev->ip4_pxe && !dev->ip6_pxe)
    return;

  {
    grub_efi_status_t status;
    grub_efi_boot_services_t *b = grub_efi_system_table->boot_services;

    dev->pxe_callback.rev = GRUB_EFI_PXE_CALLBACK_VERSION;
    dev->pxe_callback.callback = grub_efi_net_pxe_callback;

    status = efi_call_4 (b->install_protocol_interface,
                  &dev->handle,
                  &pxe_callback_guid,
                  GRUB_EFI_NATIVE_INTERFACE,
                  &dev->pxe_callback);

    if (status != GRUB_EFI_SUCCESS)
    {
      dev->pxe_callback.callback = NULL;
      grub_printf("Could not install callback interface: %d\n", (int)status);
      return;
    }
  }

  if (dev->ip4_pxe && dev->ip4_pxe->mode->started)
  {
    grub_efi_boolean_t enable = 1;
    grub_efi_status_t status;

    status = efi_call_6(dev->ip4_pxe->setparams, dev->ip4_pxe, 0,0,0,0, &enable);    
    if (status != GRUB_EFI_SUCCESS)
    {
      grub_printf("Could not enable pxe callback for ipv4: %d\n", (int)status);            
    }
  }

  if (dev->ip6_pxe && dev->ip6_pxe->mode->started)
  {
    grub_efi_boolean_t enable = 1;
    grub_efi_status_t status;

    status = efi_call_6(dev->ip6_pxe->setparams, dev->ip6_pxe, 0,0,0,0, &enable);    
    if (status != GRUB_EFI_SUCCESS)
    {
      grub_printf("Could not enable pxe callback for ipv6: %d\n", (int)status);            
    }
  }
}

void grub_efi_net_unregister_pxe_callback(struct grub_efi_net_device *dev)
{
  if (dev->pxe_callback.callback == 0)
    return;

  if (dev->ip4_pxe && dev->ip4_pxe->mode->started)
  {
    grub_efi_boolean_t enable = 0;
    grub_efi_status_t status;

    status = efi_call_6(dev->ip4_pxe->setparams, dev->ip4_pxe, 0,0,0,0, &enable);    
    if (status != GRUB_EFI_SUCCESS)
    {
      grub_printf("Could not disable pxe callback for ipv4: %d\n", (int)status);            
    }
  }

  if (dev->ip6_pxe && dev->ip6_pxe->mode->started)
  {
    grub_efi_boolean_t enable = 0;
    grub_efi_status_t status;

    status = efi_call_6(dev->ip6_pxe->setparams, dev->ip6_pxe, 0,0,0,0, &enable);    
    if (status != GRUB_EFI_SUCCESS)
    {
      grub_printf("Could not disable pxe callback for ipv6: %d\n", (int)status);            
    }
  }

  {
    grub_efi_status_t status;
    grub_efi_boot_services_t *b = grub_efi_system_table->boot_services;

    status = efi_call_3 (b->uninstall_protocol_interface,
                dev->handle,
                &pxe_callback_guid,
                &dev->pxe_callback);
    if (status == GRUB_EFI_SUCCESS)
    {
       dev->pxe_callback.callback = NULL;
    }
    else
    {
      grub_printf("Could not remove pxe callback interface: %d\n", (int)status);
    }
  }
}

static grub_efi_ip6_config_manual_address_t *
efi_ip6_config_manual_address (grub_efi_ip6_config_protocol_t *ip6_config)
{
  grub_efi_uintn_t sz;
  grub_efi_status_t status;
  grub_efi_ip6_config_manual_address_t *manual_address;

  sz = sizeof (*manual_address);
  manual_address = grub_malloc (sz);
  if (!manual_address)
    return NULL;

  status = efi_call_4 (ip6_config->get_data, ip6_config,
		    GRUB_EFI_IP6_CONFIG_DATA_TYPE_MANUAL_ADDRESS,
		    &sz, manual_address);

  if (status != GRUB_EFI_SUCCESS)
    {
      grub_free (manual_address);
      return NULL;
    }

  return manual_address;
}

static grub_efi_ip4_config2_manual_address_t *
efi_ip4_config_manual_address (grub_efi_ip4_config2_protocol_t *ip4_config)
{
  grub_efi_uintn_t sz;
  grub_efi_status_t status;
  grub_efi_ip4_config2_manual_address_t *manual_address;

  sz = sizeof (*manual_address);
  manual_address = grub_malloc (sz);
  if (!manual_address)
    return NULL;

  status = efi_call_4 (ip4_config->get_data, ip4_config,
		    GRUB_EFI_IP4_CONFIG2_DATA_TYPE_MANUAL_ADDRESS,
		    &sz, manual_address);

  if (status != GRUB_EFI_SUCCESS)
    {
      grub_free (manual_address);
      return NULL;
    }

  return manual_address;
}

static void
pxe_configure (struct grub_efi_net_device *dev, int prefer_ip6)
{
  grub_efi_pxe_t *pxe = (prefer_ip6) ? dev->ip6_pxe : dev->ip4_pxe;

  grub_efi_pxe_mode_t *mode = pxe->mode;

  if (!mode->started)
    {
      grub_efi_status_t status;
      status = efi_call_2 (pxe->start, pxe, prefer_ip6);

      if (status != GRUB_EFI_SUCCESS)
	  grub_printf ("Couldn't start PXE\n");
    }

#if 0
  grub_printf ("PXE STARTED: %u\n", mode->started);
  grub_printf ("PXE USING IPV6: %u\n", mode->using_ipv6);
#endif

  if (mode->using_ipv6)
    {
      grub_efi_ip6_config_manual_address_t *manual_address;
      manual_address = efi_ip6_config_manual_address (dev->ip6_config);

      if (manual_address &&
	  grub_memcmp (manual_address->address, mode->station_ip.v6.addr, sizeof (manual_address->address)) != 0)
	{
	  grub_efi_status_t status;
	  grub_efi_pxe_ip_address_t station_ip;

	  grub_memcpy (station_ip.v6.addr, manual_address->address, sizeof (station_ip.v6.addr));
	  status = efi_call_3 (pxe->set_station_ip, pxe, &station_ip, NULL);

	  if (status != GRUB_EFI_SUCCESS)
	      grub_printf ("Couldn't set station ip\n");

	  grub_free (manual_address);
	}
    }
  else
    {
      grub_efi_ip4_config2_manual_address_t *manual_address;
      manual_address = efi_ip4_config_manual_address (dev->ip4_config);

      if (manual_address &&
	  grub_memcmp (manual_address->address, mode->station_ip.v4.addr, sizeof (manual_address->address)) != 0)
	{
	  grub_efi_status_t status;
	  grub_efi_pxe_ip_address_t station_ip;
	  grub_efi_pxe_ip_address_t subnet_mask;

	  grub_memcpy (station_ip.v4.addr, manual_address->address, sizeof (station_ip.v4.addr));
	  grub_memcpy (subnet_mask.v4.addr, manual_address->subnet_mask, sizeof (subnet_mask.v4.addr));

	  status = efi_call_3 (pxe->set_station_ip, pxe, &station_ip, &subnet_mask);

	  if (status != GRUB_EFI_SUCCESS)
	      grub_printf ("Couldn't set station ip\n");

	  grub_free (manual_address);
	}
    }

#if 0
  if (mode->using_ipv6)
    {
      grub_printf ("PXE STATION IP: %02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x\n",
	mode->station_ip.v6.addr[0],
	mode->station_ip.v6.addr[1],
	mode->station_ip.v6.addr[2],
	mode->station_ip.v6.addr[3],
	mode->station_ip.v6.addr[4],
	mode->station_ip.v6.addr[5],
	mode->station_ip.v6.addr[6],
	mode->station_ip.v6.addr[7],
	mode->station_ip.v6.addr[8],
	mode->station_ip.v6.addr[9],
	mode->station_ip.v6.addr[10],
	mode->station_ip.v6.addr[11],
	mode->station_ip.v6.addr[12],
	mode->station_ip.v6.addr[13],
	mode->station_ip.v6.addr[14],
	mode->station_ip.v6.addr[15]);
    }
  else
    {
      grub_printf ("PXE STATION IP: %d.%d.%d.%d\n",
	mode->station_ip.v4.addr[0],
	mode->station_ip.v4.addr[1],
	mode->station_ip.v4.addr[2],
	mode->station_ip.v4.addr[3]);
      grub_printf ("PXE SUBNET MASK: %d.%d.%d.%d\n",
	mode->subnet_mask.v4.addr[0],
	mode->subnet_mask.v4.addr[1],
	mode->subnet_mask.v4.addr[2],
	mode->subnet_mask.v4.addr[3]);
    }
#endif

  /* TODO: Set The Station IP to the IP2 Config */
}

static int
parse_ip6 (const char *val, grub_uint64_t *ip, const char **rest)
{
  grub_uint16_t newip[8];
  const char *ptr = val;
  int word, quaddot = -1;
  int bracketed = 0;

  if (ptr[0] == '[') {
    bracketed = 1;
    ptr++;
  }

  if (ptr[0] == ':' && ptr[1] != ':')
    return 0;
  if (ptr[0] == ':')
    ptr++;

  for (word = 0; word < 8; word++)
    {
      unsigned long t;
      if (*ptr == ':')
	{
	  quaddot = word;
	  word--;
	  ptr++;
	  continue;
	}
      t = grub_strtoul (ptr, &ptr, 16);
      if (grub_errno)
	{
	  grub_errno = GRUB_ERR_NONE;
	  break;
	}
      if (t & ~0xffff)
	return 0;
      newip[word] = grub_cpu_to_be16 (t);
      if (*ptr != ':')
	break;
      ptr++;
    }
  if (quaddot == -1 && word < 7)
    return 0;
  if (quaddot != -1)
    {
      grub_memmove (&newip[quaddot + 7 - word], &newip[quaddot],
		    (word - quaddot + 1) * sizeof (newip[0]));
      grub_memset (&newip[quaddot], 0, (7 - word) * sizeof (newip[0]));
    }
  grub_memcpy (ip, newip, 16);
  if (bracketed && *ptr == ']') {
    ptr++;
  }
  if (rest)
    *rest = ptr;
  return 1;
}

static grub_err_t
pxe_open (struct grub_efi_net_device *dev,
	  int prefer_ip6,
	  grub_file_t file,
	  const char *filename,
	  int type __attribute__((unused)))
{
  int i;
  const char *p;
  grub_efi_status_t status;
  grub_efi_pxe_ip_address_t server_ip;
  grub_efi_uint64_t file_size = 0;
  grub_efi_pxe_t *pxe = (prefer_ip6) ? dev->ip6_pxe : dev->ip4_pxe;

  if (pxe->mode->using_ipv6)
    {
      const char *rest;
      grub_uint64_t ip6[2];
      if (parse_ip6 (file->device->net->server, ip6, &rest) && *rest == 0)
	grub_memcpy (server_ip.v6.addr, ip6, sizeof (server_ip.v6.addr));
      /* TODO: ERROR Handling Here */
#if 0
      grub_printf ("PXE SERVER IP: %02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x\n",
	server_ip.v6.addr[0],
	server_ip.v6.addr[1],
	server_ip.v6.addr[2],
	server_ip.v6.addr[3],
	server_ip.v6.addr[4],
	server_ip.v6.addr[5],
	server_ip.v6.addr[6],
	server_ip.v6.addr[7],
	server_ip.v6.addr[8],
	server_ip.v6.addr[9],
	server_ip.v6.addr[10],
	server_ip.v6.addr[11],
	server_ip.v6.addr[12],
	server_ip.v6.addr[13],
	server_ip.v6.addr[14],
	server_ip.v6.addr[15]);
#endif
    }
  else
    {
      for (i = 0, p = file->device->net->server; i < 4; ++i, ++p)
	server_ip.v4.addr[i] = grub_strtoul (p, &p, 10);
    }

  status = efi_call_10 (pxe->mtftp,
	    pxe,
	    GRUB_EFI_PXE_BASE_CODE_TFTP_GET_FILE_SIZE,
	    NULL,
	    0,
	    &file_size,
	    NULL,
	    &server_ip,
	    (grub_efi_char8_t *)filename,
	    NULL,
	    0);

  if (status != GRUB_EFI_SUCCESS)
    return grub_error (GRUB_ERR_IO, "Couldn't get file size");

  file->size = (grub_off_t)file_size;
  file->not_easily_seekable = 0;
  file->data = 0;
  file->device->net->offset = 0;

  return GRUB_ERR_NONE;
}

static grub_err_t
pxe_close (struct grub_efi_net_device *dev __attribute__((unused)),
	  int prefer_ip6 __attribute__((unused)),
	  grub_file_t file __attribute__((unused)))
{
  file->offset = 0;
  file->size = 0;
  file->device->net->offset = 0;

  if (file->data)
    {
      grub_free (file->data);
      file->data = NULL;
    }

  return GRUB_ERR_NONE;
}

#if 0
static grub_ssize_t
pxe_read (struct grub_efi_net_device *dev,
	  int prefer_ip6,
	  grub_file_t file,
	  char *buf,
	  grub_size_t len)
{
  int i;
  const char *p;
  grub_efi_status_t status;
  grub_efi_pxe_t *pxe = (prefer_ip6) ? dev->ip6_pxe : dev->ip4_pxe;
  grub_efi_uint64_t bufsz = len;
  grub_efi_pxe_ip_address_t server_ip;
  char *buf2 = NULL;

  if (file->data)
    {
      /* TODO: RANGE Check for offset and file size */
      grub_memcpy (buf, (char*)file->data + file->device->net->offset, len);
      file->device->net->offset += len;
      return len;
    }

  if (file->device->net->offset)
    {
      grub_error (GRUB_ERR_BUG, "No Offet Read Possible");
      grub_print_error ();
      return 0;
    }

  if (pxe->mode->using_ipv6)
    {
      const char *rest;
      grub_uint64_t ip6[2];
      if (parse_ip6 (file->device->net->server, ip6, &rest) && *rest == 0)
	grub_memcpy (server_ip.v6.addr, ip6, sizeof (server_ip.v6.addr));
      /* TODO: ERROR Handling Here */
    }
  else
    {
      for (i = 0, p = file->device->net->server; i < 4; ++i, ++p)
	server_ip.v4.addr[i] = grub_strtoul (p, &p, 10);
    }

  status = efi_call_10 (pxe->mtftp,
	    pxe,
	    GRUB_EFI_PXE_BASE_CODE_TFTP_READ_FILE,
	    buf,
	    0,
	    &bufsz,
	    NULL,
	    &server_ip,
	    (grub_efi_char8_t *)file->device->net->name,
	    NULL,
	    0);

  if (bufsz != file->size)
    {
      grub_error (GRUB_ERR_BUG, "Short read should not happen here");
      grub_print_error ();
      return 0;
    }

  if (status == GRUB_EFI_BUFFER_TOO_SMALL)
    {

      buf2 = grub_malloc (bufsz);

      if (!buf2)
	{
	  grub_error (GRUB_ERR_OUT_OF_MEMORY, "ERROR OUT OF MEMORY");
	  grub_print_error ();
	  return 0;
	}

      status = efi_call_10 (pxe->mtftp,
		pxe,
		GRUB_EFI_PXE_BASE_CODE_TFTP_READ_FILE,
		buf2,
		0,
		&bufsz,
		NULL,
		&server_ip,
		(grub_efi_char8_t *)file->device->net->name,
		NULL,
		0);
    }

  if (status != GRUB_EFI_SUCCESS)
    {
      if (buf2)
	grub_free (buf2);

      grub_error (GRUB_ERR_IO, "Failed to Read File");
      grub_print_error ();
      return 0;
    }

  if (buf2)
    grub_memcpy (buf, buf2, len);

  file->device->net->offset = len;

  if (buf2)
    file->data = buf2;

  return len;
}
#endif

static grub_ssize_t
pxe_read (struct grub_efi_net_device *dev,
	  int prefer_ip6,
	  grub_file_t file,
	  char *buf,
	  grub_size_t len)
{
  int i;
  const char *p;
  grub_efi_status_t status;
  grub_efi_pxe_t *pxe = (prefer_ip6) ? dev->ip6_pxe : dev->ip4_pxe;
  grub_efi_uint64_t bufsz = len;
  grub_uint64_t oldOffset = file->device->net->offset;
  grub_efi_pxe_ip_address_t server_ip;
  char *buf2 = NULL;

  if (!file->data)
  {
    if (pxe->mode->using_ipv6)
      {
        const char *rest;
        grub_uint64_t ip6[2];
        if (parse_ip6 (file->device->net->server, ip6, &rest) && *rest == 0)
	  grub_memcpy (server_ip.v6.addr, ip6, sizeof (server_ip.v6.addr));
        /* TODO: ERROR Handling Here */
      }
    else
      {
        for (i = 0, p = file->device->net->server; i < 4; ++i, ++p)
	  server_ip.v4.addr[i] = grub_strtoul (p, &p, 10);
      }

    if (file->device->net->offset == 0 && file->size != 0 && len >= file->size)
      {
        current_pxe_file = file;
        current_progress = 0;

        status = efi_call_10 (pxe->mtftp,
		pxe,
		GRUB_EFI_PXE_BASE_CODE_TFTP_READ_FILE,
		buf,
		0,
		&bufsz,
		NULL,
		&server_ip,
		(grub_efi_char8_t *)file->device->net->name,
		NULL,
		0);

        if (current_progress > 0)
          send_current_progress();
        current_pxe_file = 0;

        if (bufsz != file->size)
          {
            grub_error (GRUB_ERR_BUG, "Short read should not happen here");
            grub_print_error ();
            return 0;
          }

	// Ignore buffer too small when we were reading at the start of the file
	// We do need to read the complete file again if we want more data,
	// but we can do that at the time when we actually need it
	// This avoids reading the whole file if we just want to check a header
	if (status != GRUB_EFI_SUCCESS && status != GRUB_EFI_BUFFER_TOO_SMALL)
	{
	  if (buf2)
	    grub_free (buf2);

          grub_error (GRUB_ERR_IO, "Failed to Read File");
          grub_print_error ();
          return 0;
        }

        file->device->net->offset = oldOffset + len;
        return len;
      }
    else
      {
        bufsz = file->size;
        buf2 = grub_malloc (bufsz);

	if (!buf2)
	  {
	    grub_error (GRUB_ERR_OUT_OF_MEMORY, "ERROR OUT OF MEMORY");
	    grub_print_error ();
	    return 0;
	  }

        current_pxe_file = file;

	status = efi_call_10 (pxe->mtftp,
		pxe,
		GRUB_EFI_PXE_BASE_CODE_TFTP_READ_FILE,
		buf2,
		0,
		&bufsz,
		NULL,
		&server_ip,
		(grub_efi_char8_t *)file->device->net->name,
		NULL,
		0);

        if (current_progress > 0)
          send_current_progress();
        current_pxe_file = 0;
      }

    if (status != GRUB_EFI_SUCCESS)
      {
	if (buf2)
	  grub_free (buf2);

	grub_error (GRUB_ERR_IO, "Failed to Read File");
	grub_print_error ();
	return 0;
      }

    if (buf2)
      file->data = buf2;
  }

  if (file->data)
    {
      /* TODO: RANGE Check for offset and file size */
      grub_memcpy (buf, (char*)file->data + oldOffset, len);
      file->device->net->offset = oldOffset + len;
      return len;
    }

  grub_error (GRUB_ERR_IO, "Failed to Read File");
  grub_print_error ();
  return 0;
}


struct grub_efi_net_io io_pxe =
  {
    .configure = pxe_configure,
    .open = pxe_open,
    .read = pxe_read,
    .close = pxe_close
  };
