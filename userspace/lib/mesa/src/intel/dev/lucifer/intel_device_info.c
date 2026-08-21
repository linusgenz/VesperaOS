// intel_device_info.c
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 17.08.26.
//
// This file is part of VesperaOS.
//
// VesperaOS is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// VesperaOS is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with VesperaOS. If not, see <https://www.gnu.org/licenses/>.

// TODO implement intel device info

#include "lucifer/intel_device_info.h"

#include "common/intel_gem.h"
#include "dev/intel_device_info.h"
#include "util/bitscan.h"
#include "util/log.h"

#include "vespera/dev/lucifer_drm.h"


static void *
lucifer_query_alloc_fetch(int fd, uint32_t query_id, int32_t *out_len)
{
   struct lucifer_query q = { .query = query_id };

   if (intel_ioctl(fd, LUCIFER_IOCTL_QUERY, &q))
      return NULL;

   void *data = calloc(1, q.size);
   if (!data)
      return NULL;

   q.data = (uintptr_t)data;
   if (intel_ioctl(fd, LUCIFER_IOCTL_QUERY, &q))
      goto fetch_failed;

   if (out_len)
      *out_len = q.size;
   return data;

fetch_failed:
   free(data);
   return NULL;
}

static bool
lucifer_query_config(int fd, struct intel_device_info *devinfo)
{
   struct lucifer_query_config *cfg =
      lucifer_query_alloc_fetch(fd, LUCIFER_QUERY_CONFIG, NULL);
   if (!cfg)
      return false;

   devinfo->pci_device_id       = cfg->device_id;
   devinfo->pci_revision_id     = cfg->revision;
   devinfo->revision            = cfg->revision;
   devinfo->gtt_size            = cfg->gtt_size;
   devinfo->timestamp_frequency = cfg->timestamp_frequency;
   devinfo->mem_alignment       = cfg->mem_alignment;

   free(cfg);
   return true;
}

bool
intel_device_info_lucifer_query_regions(struct intel_device_info *devinfo,
                                         int fd, bool update)
{
   struct lucifer_query_mem_regions *regions =
      lucifer_query_alloc_fetch(fd, LUCIFER_QUERY_MEM_REGIONS, NULL);
   if (!regions)
      return false;

   if (!update) {
      devinfo->mem.sram.mem.klass     = 0;
      devinfo->mem.sram.mem.instance  = 0;
      devinfo->mem.sram.mappable.size = regions->total_size;
   } else {
      assert(devinfo->mem.sram.mappable.size == regions->total_size);
   }
   devinfo->mem.sram.mappable.free = regions->total_size - regions->used;

   /* Kein VRAM auf Gen9.5 -- immer false, es gibt nur eine Memory-Klasse. */
   devinfo->mem.use_class_instance = false;

   free(regions);
   return true;
}

/*
 * Uebersetzt die vom Kernel gelieferten Fuse-Masken (1 Slice, N Subslices,
 * gleiche EU-Maske je Subslice bei Gen9.5) in die generischen
 * intel_device_info Topologie-Felder. Bewusst generisch ueber
 * max_subslices_per_slice/max_eus_per_subslice statt Gen9.5-Werte
 * hartzukodieren, damit spaetere SKUs mit abweichendem Fusing (GT1 vs GT2
 * vs GT3) ohne Codeaenderung funktionieren -- die tatsaechlichen Grenzen
 * kommen aus der Anzahl gesetzter Bits in den Masken, nicht aus Konstanten.
 */
bool
intel_device_info_lucifer_update_from_masks(struct intel_device_info *devinfo,
                                             uint32_t slice_mask,
                                             uint32_t subslice_mask,
                                             uint32_t n_eus)
{
   intel_device_info_topology_reset_masks(devinfo);

   devinfo->max_slices = util_last_bit(slice_mask);
   devinfo->max_subslices_per_slice = util_last_bit(subslice_mask);
   devinfo->max_eus_per_subslice = util_bitcount(n_eus);

   assert(devinfo->max_slices <= INTEL_DEVICE_MAX_SLICES);
   assert(devinfo->max_subslices_per_slice <= INTEL_DEVICE_MAX_SUBSLICES);
   assert(devinfo->max_eus_per_subslice <= INTEL_DEVICE_MAX_EUS_PER_SUBSLICE);

   devinfo->subslice_slice_stride =
      DIV_ROUND_UP(devinfo->max_slices, 8);
   devinfo->eu_slice_stride =
      DIV_ROUND_UP(devinfo->max_eus_per_subslice * devinfo->max_subslices_per_slice, 8);
   devinfo->eu_subslice_stride =
      DIV_ROUND_UP(devinfo->max_eus_per_subslice, 8);

   /* Gen9.5: genau eine Slice, slice_mask ist entweder 0x1 (vorhanden) oder
    * 0x0 (kaputt/deaktiviert -- sollte praktisch nie vorkommen). */
   devinfo->slice_masks = slice_mask & 0xFF;

   for (unsigned s = 0; s < devinfo->max_slices; s++) {
      if (!(slice_mask & (1u << s)))
         continue;

      for (unsigned ss = 0; ss < devinfo->max_subslices_per_slice; ss++) {
         if (!(subslice_mask & (1u << ss)))
            continue;

         devinfo->subslice_masks[s * devinfo->subslice_slice_stride + ss / 8] |=
            (1u << (ss % 8));

         /* Gen9.5 fuselt EUs uniform pro Subslice (keine per-DSS-
          * individuellen Masken wie bei spaeteren Generationen), daher
          * dieselbe n_eus-Maske fuer jede aktive Subslice. */
         for (unsigned eu = 0; eu < devinfo->max_eus_per_subslice; eu++) {
            if (!(n_eus & (1u << eu)))
               continue;

            devinfo->eu_masks[s * devinfo->eu_slice_stride +
                               ss * devinfo->eu_subslice_stride +
                               eu / 8] |= (1u << (eu % 8));
         }
      }
   }

   intel_device_info_topology_update_counts(devinfo);
   intel_device_info_update_pixel_pipes(devinfo, devinfo->subslice_masks);
   return true;
}

static bool
lucifer_query_topology(int fd, struct intel_device_info *devinfo)
{
   struct lucifer_query_topology *topo =
      lucifer_query_alloc_fetch(fd, LUCIFER_QUERY_TOPOLOGY, NULL);
   if (!topo)
      return false;

   devinfo->l3_banks = topo->l3_banks;

   bool ret = intel_device_info_lucifer_update_from_masks(
      devinfo, topo->slice_mask, topo->subslice_mask, topo->eu_mask);

   free(topo);
   return ret;
}

void *
intel_device_info_lucifer_query_hwconfig(int fd, int32_t *len)
{

   if (len)
      *len = 0;
   return NULL;
}

bool
intel_device_info_lucifer_get_info_from_fd(int fd,
                                            struct intel_device_info *devinfo)
{
   if (!lucifer_query_config(fd, devinfo))
      return false;

   if (!intel_device_info_lucifer_query_regions(devinfo, fd, false))
      return false;

   if (!lucifer_query_topology(fd, devinfo))
      return false;

   devinfo->has_context_isolation = true;
   devinfo->has_mmap_offset       = true;
   devinfo->has_partial_mmap_offset = true;
   devinfo->has_llc               = true; /* Gen9.5 hat LLC (Last Level Cache) */

   return true;
}
