/*
 * Copyright © 2026 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */
#include "lucifer/intel_engine.h"

#include <stdlib.h>

#include "common/intel_gem.h"
#include "lucifer/intel_device_query.h"

#include "vespera/dev/lucifer_drm.h"

static enum intel_engine_class
lucifer_engine_class_to_intel(uint16_t lucifer)
{
   switch (lucifer) {
   case LUCIFER_ENGINE_CLASS_RENDER:
      return INTEL_ENGINE_CLASS_RENDER;
   case LUCIFER_ENGINE_CLASS_COPY:
      return INTEL_ENGINE_CLASS_COPY;
   default:
      return INTEL_ENGINE_CLASS_INVALID;
   }
}

uint16_t
intel_engine_class_to_lucifer(enum intel_engine_class intel)
{
   switch (intel) {
   case INTEL_ENGINE_CLASS_RENDER:
      return LUCIFER_ENGINE_CLASS_RENDER;
   case INTEL_ENGINE_CLASS_COPY:
      return LUCIFER_ENGINE_CLASS_COPY;
   default:
      /* Gen9.5 (the only target so far) has no video, video-enhance, or
       * compute engine class -- unlike Xe's equivalent, there is no valid
       * lucifer_engine_class to map these to yet, so this is a genuine
       * error case rather than an unmapped-but-real hardware engine.
       */
      return UINT16_MAX;
   }
}

struct intel_query_engine_info *
lucifer_engine_get_info(int fd)
{
   struct lucifer_query_engines *lucifer_engines;

   lucifer_engines = lucifer_device_query_alloc_fetch(fd, LUCIFER_QUERY_ENGINES, NULL);
   if (!lucifer_engines)
      return NULL;

   struct intel_query_engine_info *intel_engines_info;
   intel_engines_info = calloc(1, sizeof(*intel_engines_info) +
                               sizeof(*intel_engines_info->engines) *
                               lucifer_engines->num_engines);
   if (!intel_engines_info) {
      goto error_free_lucifer_engines;
      return NULL;
   }

   for (uint32_t i = 0; i < lucifer_engines->num_engines; i++) {
      struct lucifer_engine_class_instance *lucifer_engine = &lucifer_engines->engines[i];
      struct intel_engine_class_instance *intel_engine = &intel_engines_info->engines[i];

      intel_engine->engine_class = lucifer_engine_class_to_intel(lucifer_engine->engine_class);
      intel_engine->engine_instance = lucifer_engine->engine_instance;
      /* Gen9.5 is single-GT -- there is no gt_id equivalent in
       * lucifer_engine_class_instance (unlike Xe's drm_xe_engine_class_instance,
       * which is multi-GT aware). Always report GT 0. */
      intel_engine->gt_id = 0;
   }

   intel_engines_info->num_engines = lucifer_engines->num_engines;
   free(lucifer_engines);
   return intel_engines_info;

error_free_lucifer_engines:
   free(lucifer_engines);
   return NULL;
}

bool
lucifer_engines_is_guc_semaphore_functional(int fd, const struct intel_device_info *info)
{
   /* Gen9.5 bring-up has no GuC submission path (execlist submission only,
    * see IntelPpgtt / execlist work in the kernel driver) -- there is no
    * GuC firmware version to query, so compute-class support (the only
    * caller of this function, see intel_engines_supported_count()) is
    * unconditionally unavailable for now. Revisit if/when GuC submission
    * is implemented.
    */
   return false;
}
