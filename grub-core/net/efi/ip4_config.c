
#include <grub/efi/api.h>
#include <grub/efi/efi.h>
#include <grub/misc.h>
#include <grub/net/efi.h>
#include <grub/charset.h>
#include <grub/safemath.h>

char *
grub_efi_hw_address_to_string (grub_efi_uint32_t hw_address_size, grub_efi_mac_address_t hw_address)
{
  char *hw_addr, *p;
  grub_size_t sz, s, i;

  if (grub_mul (hw_address_size, sizeof ("XX:") - 1, &sz) ||
      grub_add (sz, 1, &sz))
    {
      grub_errno = GRUB_ERR_OUT_OF_RANGE;
      return NULL;
    }

  hw_addr = grub_malloc (sz);
  if (!hw_addr)
    return NULL;

  p = hw_addr;
  s = sz;
  for (i = 0; i < hw_address_size; i++)
    {
      grub_snprintf (p, sz, "%02x:", hw_address[i]);
      p +=  sizeof ("XX:") - 1;
      s -=  sizeof ("XX:") - 1;
    }

  hw_addr[sz - 2] = '\0';
  return hw_addr;
}

char *
grub_efi_ip4_address_to_string (grub_efi_ipv4_address_t *address)
{
  char *addr;

  addr = grub_malloc (sizeof ("XXX.XXX.XXX.XXX"));
  if (!addr)
      return NULL;

  /* FIXME: Use grub_xasprintf ? */
  grub_snprintf (addr,
	  sizeof ("XXX.XXX.XXX.XXX"),
	  "%u.%u.%u.%u",
	  (*address)[0],
	  (*address)[1],
	  (*address)[2],
	  (*address)[3]);

  return addr;
}

int
grub_efi_string_to_ip4_address (const char *val, grub_efi_ipv4_address_t *address, const char **rest)
{
  grub_uint32_t newip = 0;
  int i;
  const char *ptr = val;

  for (i = 0; i < 4; i++)
    {
      unsigned long t;
      t = grub_strtoul (ptr, &ptr, 0);
      if (grub_errno)
	{
	  grub_errno = GRUB_ERR_NONE;
	  return 0;
	}
      if (*ptr != '.' && i == 0)
	{
	  /* XXX: t is in host byte order */
	  newip = t;
	  break;
	}
      if (t & ~0xff)
	return 0;
      newip <<= 8;
      newip |= t;
      if (i != 3 && *ptr != '.')
	return 0;
      ptr++;
    }

  newip =  grub_cpu_to_be32 (newip);

  grub_memcpy (address, &newip, sizeof(*address));

  if (rest)
    *rest = (ptr - 1);
  return 1;
}

static grub_efi_ip4_config2_interface_info_t *
efi_ip4_config_interface_info (grub_efi_ip4_config2_protocol_t *ip4_config)
{
  grub_efi_uintn_t sz;
  grub_efi_status_t status;
  grub_efi_ip4_config2_interface_info_t *interface_info;

  sz = sizeof (*interface_info) + sizeof (*interface_info->route_table);
  interface_info = grub_malloc (sz);
  if (!interface_info)
    return NULL;

  status = efi_call_4 (ip4_config->get_data, ip4_config,
		GRUB_EFI_IP4_CONFIG2_DATA_TYPE_INTERFACEINFO,
		&sz, interface_info);

  if (status == GRUB_EFI_BUFFER_TOO_SMALL)
    {
      grub_free (interface_info);
      interface_info = grub_malloc (sz);
      status = efi_call_4 (ip4_config->get_data, ip4_config,
		    GRUB_EFI_IP4_CONFIG2_DATA_TYPE_INTERFACEINFO,
		    &sz, interface_info);
    }

  if (status != GRUB_EFI_SUCCESS)
    {
      grub_free (interface_info);
      return NULL;
    }

  return interface_info;
}

static grub_efi_ip4_config_data_t *
efi_ip4_config1_data(grub_efi_ip4_config_protocol_t * ip4_config1)
{
  grub_efi_uintn_t sz;
  grub_efi_status_t status;
  grub_efi_ip4_config_data_t *config_data;

  if (!ip4_config1)
     return NULL;

  sz = sizeof (*config_data) + sizeof (*config_data->route_table);
  config_data = grub_malloc (sz);
  if (!config_data)
    return NULL;

  status = efi_call_3 (ip4_config1->get_data, ip4_config1,
		&sz, config_data);

  if (status == GRUB_EFI_BUFFER_TOO_SMALL)
    {
      grub_free (config_data);
      config_data = grub_malloc (sz);
      status = efi_call_3 (ip4_config1->get_data, ip4_config1,
		    &sz, config_data);
    }

  if (status != GRUB_EFI_SUCCESS)
    {
      grub_free (config_data);
      return NULL;
    }

  return config_data;
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

char *
grub_efi_ip4_interface_name (struct grub_efi_net_device *dev)
{
  if (dev->ip4_config)
  {
    grub_efi_ip4_config2_interface_info_t *interface_info;
    char *name;

    interface_info = efi_ip4_config_interface_info (dev->ip4_config);

    if (!interface_info)
      return NULL;

    name = grub_malloc (GRUB_EFI_IP4_CONFIG2_INTERFACE_INFO_NAME_SIZE
		        * GRUB_MAX_UTF8_PER_UTF16 + 1);
    *grub_utf16_to_utf8 ((grub_uint8_t *)name, interface_info->name,
		        GRUB_EFI_IP4_CONFIG2_INTERFACE_INFO_NAME_SIZE) = 0;
    grub_free (interface_info);
    return name;
  }
  else
  {
    // TODO: ip4_config1
    return grub_strdup("legacy");
  }
}

static char *
grub_efi_ip4_interface_hw_address (struct grub_efi_net_device *dev)
{
  char *hw_addr;

  if (dev->ip4_config)
  {
    grub_efi_ip4_config2_interface_info_t *interface_info;

    interface_info = efi_ip4_config_interface_info (dev->ip4_config);

    if (!interface_info)
      return NULL;

    hw_addr = grub_efi_hw_address_to_string (interface_info->hw_address_size, interface_info->hw_address);
    grub_free (interface_info);

  }
  else if (dev->simple_net)
  {
    if (dev->simple_net->mode && dev->simple_net->mode->hwaddr_size > 0)
    {
      hw_addr = grub_efi_hw_address_to_string(dev->simple_net->mode->hwaddr_size, dev->simple_net->mode->current_address);   
    }
    else
    {
      hw_addr = grub_strdup("ff:ff:ff:ff:ff:ff");
    }
  }
  else
  {
    // TODO!!
    hw_addr = grub_strdup("ff:ff:ff:ff:ff:ff");
  }

  return hw_addr;
}

static char *
grub_efi_ip4_interface_address (struct grub_efi_net_device *dev)
{
  char *addr = NULL;

  if (dev->ip4_config)
  {
    grub_efi_ip4_config2_manual_address_t *manual_address;
    manual_address = efi_ip4_config_manual_address (dev->ip4_config);

    if (!manual_address)
      return NULL;

    addr = grub_efi_ip4_address_to_string (&manual_address->address);
    grub_free (manual_address);
  }
  else
  {
    // CHECK: Should this use the pxe basecode protocol instead?
    grub_efi_ip4_config_data_t * config_data;

    config_data = efi_ip4_config1_data(dev->ip4_config1);
    if (config_data)
    {
      addr = grub_efi_ip4_address_to_string(&config_data->station_address);
      grub_free(config_data);
    }
    else if (dev->ip4_pxe)
    {
      addr =  grub_efi_ip4_address_to_string((grub_efi_ipv4_address_t*)&dev->ip4_pxe->mode->station_ip.v4);
    }
  }

  return addr;
}


static int
address_mask_size (grub_efi_ipv4_address_t *address)
{
  grub_uint8_t i;
  grub_uint32_t u32_addr = grub_be_to_cpu32 (grub_get_unaligned32 (address));

  if (u32_addr == 0)
    return 0;

  for (i = 0; i < 32 ; ++i)
    {
      if (u32_addr == ((0xffffffff >> i) << i))
	return (32 - i);
    }

  return -1;
}

static char **
grub_efi_ip4_interface_route_table (struct grub_efi_net_device *dev)
{
  char **ret;
  if (dev->ip4_config)
  {
    grub_efi_ip4_config2_interface_info_t *interface_info;
    int id;
    grub_size_t i, nmemb;

    interface_info = efi_ip4_config_interface_info (dev->ip4_config);
    if (!interface_info)
      return NULL;

    if (grub_add (interface_info->route_table_size, 1, &nmemb))
      {
        grub_errno = GRUB_ERR_OUT_OF_RANGE;
        return NULL;
      }
    
    ret = grub_calloc (nmemb, sizeof (*ret));
    if (!ret)
      {
        grub_free (interface_info);
        return NULL;
      }

    id = 0;
    for (i = 0; i < interface_info->route_table_size; i++)
      {
        char *subnet, *gateway, *mask;
        grub_uint32_t u32_subnet, u32_gateway;
        int mask_size;
        grub_efi_ip4_route_table_t *route_table = interface_info->route_table + i;
        grub_efi_net_interface_t *inf;
        char *interface_name = NULL;

        for (inf = dev->net_interfaces; inf; inf = inf->next)
	  if (!inf->prefer_ip6)
	    interface_name = inf->name;

        u32_gateway = grub_get_unaligned32 (&route_table->gateway_address);
        gateway = grub_efi_ip4_address_to_string (&route_table->gateway_address);
        u32_subnet = grub_get_unaligned32 (&route_table->subnet_address);
        subnet = grub_efi_ip4_address_to_string (&route_table->subnet_address);
        mask_size = address_mask_size (&route_table->subnet_mask);
        mask = grub_efi_ip4_address_to_string (&route_table->subnet_mask);
        if (u32_subnet && !u32_gateway && interface_name)
	  ret[id++] = grub_xasprintf ("%s:local %s/%d %s", dev->card_name, subnet, mask_size, interface_name);
        else if (u32_subnet && u32_gateway)
	  ret[id++] = grub_xasprintf ("%s:gw %s/%d gw %s", dev->card_name, subnet, mask_size, gateway);
        else if (!u32_subnet && u32_gateway)
	  ret[id++] = grub_xasprintf ("%s:default %s/%d gw %s", dev->card_name, subnet, mask_size, gateway);
        grub_free (subnet);
        grub_free (gateway);
        grub_free (mask);
      }

    ret[id] = NULL;
    grub_free (interface_info);
  }
  else
  {
    grub_efi_ip4_config_data_t * config_data;
    int id;
    grub_size_t i, nmemb;

    config_data = efi_ip4_config1_data(dev->ip4_config1);
    if (config_data)
    {
      if (grub_add (config_data->route_table_size, 1, &nmemb))
      {
        grub_errno = GRUB_ERR_OUT_OF_RANGE;
        return NULL;
      }

      ret = grub_calloc (nmemb, sizeof (*ret));
      if (!ret)
        {
          grub_free (config_data);
          return NULL;
        }

      id = 0;
      for (i = 0; i < config_data->route_table_size; i++)
        {
          char *subnet, *gateway, *mask;
          grub_uint32_t u32_subnet, u32_gateway;
          int mask_size;
          grub_efi_ip4_route_table_t *route_table = config_data->route_table + i;
          grub_efi_net_interface_t *inf;
          char *interface_name = NULL;

          for (inf = dev->net_interfaces; inf; inf = inf->next)
	    if (!inf->prefer_ip6)
	      interface_name = inf->name;

          u32_gateway = grub_get_unaligned32 (&route_table->gateway_address);
          gateway = grub_efi_ip4_address_to_string (&route_table->gateway_address);
          u32_subnet = grub_get_unaligned32 (&route_table->subnet_address);
          subnet = grub_efi_ip4_address_to_string (&route_table->subnet_address);
          mask_size = address_mask_size (&route_table->subnet_mask);
          mask = grub_efi_ip4_address_to_string (&route_table->subnet_mask);
          if (u32_subnet && !u32_gateway && interface_name)
	    ret[id++] = grub_xasprintf ("%s:local %s/%d %s", dev->card_name, subnet, mask_size, interface_name);
          else if (u32_subnet && u32_gateway)
	    ret[id++] = grub_xasprintf ("%s:gw %s/%d gw %s", dev->card_name, subnet, mask_size, gateway);
          else if (!u32_subnet && u32_gateway)
	    ret[id++] = grub_xasprintf ("%s:default %s/%d gw %s", dev->card_name, subnet, mask_size, gateway);
          grub_free (subnet);
          grub_free (gateway);
          grub_free (mask);
        }

      ret[id] = NULL;
      grub_free (config_data);
    }
#if 0
    else if (dev->ip4_pxe)
    {
      ret = grub_malloc (sizeof (*ret) * (dev->ip4_pxe->mode->route_table_entries + 1));
      if (!ret)
        {
          return NULL;
        }

      id = 0;
      for (i = 0; i < (int)dev->ip4_pxe->mode->route_table_entries; i++)
        {
          char *subnet, *gateway, *mask;
          grub_uint32_t u32_subnet, u32_gateway;
          int mask_size;
          grub_efi_pxe_route_entry_t *route_table = &dev->ip4_pxe->mode->route_table[i];
          grub_efi_net_interface_t *inf;
          char *interface_name = NULL;

          for (inf = dev->net_interfaces; inf; inf = inf->next)
	    if (!inf->prefer_ip6)
	      interface_name = inf->name;

          u32_gateway = grub_get_unaligned32 (&route_table->gw_addr.v4);
          gateway = grub_efi_ip4_address_to_string ((grub_efi_ipv4_address_t*)&route_table->gw_addr.v4);
          u32_subnet = grub_get_unaligned32 (&route_table->ip_addr.v4);
          subnet = grub_efi_ip4_address_to_string ((grub_efi_ipv4_address_t*)&route_table->ip_addr.v4);
          mask_size = address_mask_size ((grub_efi_ipv4_address_t*)&route_table->subnet_mask.v4);
          mask = grub_efi_ip4_address_to_string ((grub_efi_ipv4_address_t*)&route_table->subnet_mask.v4);
          if (u32_subnet && !u32_gateway && interface_name)
	    ret[id++] = grub_xasprintf ("%s:local %s/%d %s", dev->card_name, subnet, mask_size, interface_name);
          else if (u32_subnet && u32_gateway)
	    ret[id++] = grub_xasprintf ("%s:gw %s/%d gw %s", dev->card_name, subnet, mask_size, gateway);
          else if (!u32_subnet && u32_gateway)
	    ret[id++] = grub_xasprintf ("%s:default %s/%d gw %s", dev->card_name, subnet, mask_size, gateway);
          grub_free (subnet);
          grub_free (gateway);
          grub_free (mask);
        }

      ret[id] = NULL;
    }
#endif
    else
      return 0;
  }

  return ret;
}

static grub_efi_net_interface_t *
grub_efi_ip4_interface_match (struct grub_efi_net_device *dev, grub_efi_net_ip_address_t *ip_address)
{
  grub_efi_net_interface_t *inf;
  int i;
  grub_efi_ipv4_address_t *address = &ip_address->ip4;

  if (dev->ip4_config)
  {
    grub_efi_ip4_config2_interface_info_t *interface_info;

    interface_info = efi_ip4_config_interface_info (dev->ip4_config);
    if (!interface_info)
      return NULL;

    for (i = 0; i < (int)interface_info->route_table_size; i++)
      {
        grub_efi_ip4_route_table_t *route_table = interface_info->route_table + i;
        grub_uint32_t u32_address, u32_mask, u32_subnet;

        u32_address = grub_get_unaligned32 (address);
        u32_subnet = grub_get_unaligned32 (route_table->subnet_address);
        u32_mask = grub_get_unaligned32 (route_table->subnet_mask);

        /* SKIP Default GATEWAY */
        if (!u32_subnet && !u32_mask)
	  continue;

        if ((u32_address & u32_mask) == u32_subnet)
	  {
	    for (inf = dev->net_interfaces; inf; inf = inf->next)
	      if (!inf->prefer_ip6)
	        {
		  grub_free (interface_info);
		  return inf;
	        }
	}
      }

    grub_free (interface_info);
  }
  else
  {
    grub_efi_ip4_config_data_t * config_data;

    config_data = efi_ip4_config1_data (dev->ip4_config1);
    if (config_data)
    {
      for (i = 0; i < (int)config_data->route_table_size; i++)
        {
          grub_efi_ip4_route_table_t *route_table = config_data->route_table + i;
          grub_uint32_t u32_address, u32_mask, u32_subnet;

          u32_address = grub_get_unaligned32 (address);
          u32_subnet = grub_get_unaligned32 (route_table->subnet_address);
          u32_mask = grub_get_unaligned32 (route_table->subnet_mask);

          /* SKIP Default GATEWAY */
          if (!u32_subnet && !u32_mask)
	    continue;

          if ((u32_address & u32_mask) == u32_subnet)
	    {
	      for (inf = dev->net_interfaces; inf; inf = inf->next)
	        if (!inf->prefer_ip6)
	          {
		    grub_free (config_data);
		    return inf;
	          }
	  }
        }

      grub_free (config_data);
    }

    if (dev->ip4_pxe)
    {
      // Also check pxe if available
      for (i = 0; i < (int)dev->ip4_pxe->mode->route_table_entries; i++)
        {
          grub_efi_pxe_route_entry_t *route_table = &dev->ip4_pxe->mode->route_table[i];
          grub_uint32_t u32_address, u32_mask, u32_subnet;

          u32_address = grub_get_unaligned32 (address);
          u32_subnet = grub_get_unaligned32 (&route_table->ip_addr.v4);
          u32_mask = grub_get_unaligned32 (&route_table->subnet_mask.v4);

          /* SKIP Default GATEWAY */
          if (!u32_subnet && !u32_mask)
	    continue;

          if ((u32_address & u32_mask) == u32_subnet)
	    {
	      for (inf = dev->net_interfaces; inf; inf = inf->next)
	        if (!inf->prefer_ip6)
	          {
		    return inf;
	          }
	    }
        }

       {
          // Try to compare the current address with the requested address, it is not set in the routing table
          grub_uint32_t u32_address, u32_mask, u32_cardaddr;

          u32_address = grub_get_unaligned32 (address);
          u32_cardaddr = grub_get_unaligned32 (&dev->ip4_pxe->mode->station_ip.v4);
          u32_mask = grub_get_unaligned32 (&dev->ip4_pxe->mode->subnet_mask.v4);

          if (u32_mask != 0 && (u32_cardaddr & u32_mask) == (u32_address & u32_mask))
          {
	      for (inf = dev->net_interfaces; inf; inf = inf->next)
	        if (!inf->prefer_ip6)
	          {
		    return inf;
	          }
          }
       }
    }
  }

  return NULL;
}

static int
grub_efi_ip4_interface_set_manual_address (struct grub_efi_net_device *dev,
	    grub_efi_net_ip_manual_address_t *net_ip,
	    int with_subnet)
{
  grub_efi_status_t status;
  grub_efi_ip4_config2_manual_address_t *address = &net_ip->ip4;

  if (dev->ip4_config)
  {
    if (!with_subnet)
      {
        grub_efi_ip4_config2_manual_address_t *manual_address =
        efi_ip4_config_manual_address (dev->ip4_config);

        if (manual_address)
	  {
	    grub_memcpy (address->subnet_mask, manual_address->subnet_mask, sizeof(address->subnet_mask));
	    grub_free (manual_address);
	  }
        else
	  {
	    /* XXX: */
	    address->subnet_mask[0] = 0xff;
	    address->subnet_mask[1] = 0xff;
	    address->subnet_mask[2] = 0xff;
	    address->subnet_mask[3] = 0;
	  }
      }

    status = efi_call_4 (dev->ip4_config->set_data, dev->ip4_config,
		      GRUB_EFI_IP4_CONFIG2_DATA_TYPE_MANUAL_ADDRESS,
		      sizeof(*address), address);

    if (status != GRUB_EFI_SUCCESS)
      return 0;
  }
  else
  {
    // Not supported for old ip4 configuration
    // But for now return true, because the code expects this method to succeed

    // CHECK: PXE allows to set ip address. Should we use that instead of the ip_config protocol?

    grub_dprintf("efinetfs", "Manual Address not supported\n");

    return 1;
  }
  return 1;
}

static int
grub_efi_ip4_interface_set_gateway (struct grub_efi_net_device *dev,
	      grub_efi_net_ip_address_t *address)
{
  grub_efi_status_t status;

  if (dev->ip4_config)
  {
    status = efi_call_4 (dev->ip4_config->set_data, dev->ip4_config,
		  GRUB_EFI_IP4_CONFIG2_DATA_TYPE_GATEWAY,
		  sizeof (address->ip4), &address->ip4);

    if (status != GRUB_EFI_SUCCESS)
      return 0;
  }
  else
  {
    // Not supported for old ip4 configuration
    return 0;
  }

  return 1;
}

/* FIXME: Multiple DNS */
static int
grub_efi_ip4_interface_set_dns (struct grub_efi_net_device *dev,
	      grub_efi_net_ip_address_t *address)
{
  grub_efi_status_t status;

  if (dev->ip4_config)
  {
    status = efi_call_4 (dev->ip4_config->set_data, dev->ip4_config,
		  GRUB_EFI_IP4_CONFIG2_DATA_TYPE_DNSSERVER,
		  sizeof (address->ip4), &address->ip4);

    if (status != GRUB_EFI_SUCCESS)
      return 0;
  }
  else
  {
    // Not supported for old ip4 configuration
    return 0;
  }

  return 1;
}

grub_efi_net_ip_config_t *efi_net_ip4_config = &(grub_efi_net_ip_config_t)
  {
    .get_hw_address = grub_efi_ip4_interface_hw_address,
    .get_address = grub_efi_ip4_interface_address,
    .get_route_table = grub_efi_ip4_interface_route_table,
    .best_interface = grub_efi_ip4_interface_match,
    .set_address = grub_efi_ip4_interface_set_manual_address,
    .set_gateway = grub_efi_ip4_interface_set_gateway,
    .set_dns = grub_efi_ip4_interface_set_dns
  };
