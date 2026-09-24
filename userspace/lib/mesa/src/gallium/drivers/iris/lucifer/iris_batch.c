// iris_batch.c
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 10.09.26.
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

#include "lucifer/iris_batch.h"

#include "iris_batch.h"
#include "iris_context.h"
#include "iris_screen.h"

#include "common/intel_gem.h"
#include "common/intel_engine.h"

#include "vespera/dev/lucifer_drm.h"

/* TODO(lucifer): once LUCIFER_KMD gains exec-queue semantics (priority,
 * per-context isolation, ban/robustness tracking), mirror iris_xe_batch.c:
 *   - iris_lucifer_init_batch(): LUCIFER_IOCTL_EXEC_QUEUE_CREATE, honoring
 *     ice->priority and ice->protected the way iris_xe_init_batch() does
 *     with DRM_XE_EXEC_QUEUE_SET_PROPERTY_PRIORITY / _PXP_TYPE.
 *   - iris_lucifer_destroy_batch(): wait-idle + LUCIFER_IOCTL_EXEC_QUEUE_DESTROY,
 *     mirroring iris_xe_wait_exec_queue_idle()/iris_xe_destroy_exec_queue().
 *   - iris_lucifer_replace_batch(): re-create on the new queue and call
 *     iris_lost_context_state(), mirroring iris_xe_replace_batch().
 * Until then, engine_class is the only per-batch state and there is
 * nothing to destroy/replace on the kernel side.
 */

static void
iris_lucifer_map_intel_engine_class(struct iris_bufmgr *bufmgr,
                                    enum intel_engine_class *engine_classes)
{
   engine_classes[IRIS_BATCH_RENDER] = INTEL_ENGINE_CLASS_RENDER;
   engine_classes[IRIS_BATCH_COMPUTE] = INTEL_ENGINE_CLASS_RENDER;
   engine_classes[IRIS_BATCH_BLITTER] = INTEL_ENGINE_CLASS_COPY;
   STATIC_ASSERT(IRIS_BATCH_COUNT == 3);

   /* TODO(lucifer): no compute-engine query yet; LUCIFER_QUERY_ENGINES only
    * reports LUCIFER_ENGINE_CLASS_RENDER/_COPY today. Revisit once/if a
    * dedicated compute class is added to enum lucifer_engine_class, the
    * way iris_xe_map_intel_engine_class() checks
    * iris_bufmgr_compute_engine_supported().
    */
}

static enum lucifer_engine_class
intel_engine_class_to_lucifer(enum intel_engine_class engine_class)
{
   switch (engine_class) {
   case INTEL_ENGINE_CLASS_COPY:
      return LUCIFER_ENGINE_CLASS_COPY;
   case INTEL_ENGINE_CLASS_RENDER:
   default:
      return LUCIFER_ENGINE_CLASS_RENDER;
   }
}

void iris_lucifer_init_batches(struct iris_context *ice)
{
   struct iris_screen *screen = (struct iris_screen *)ice->ctx.screen;
   struct iris_bufmgr *bufmgr = screen->bufmgr;
   enum intel_engine_class engine_classes[IRIS_BATCH_COUNT];

   iris_lucifer_map_intel_engine_class(bufmgr, engine_classes);

   iris_foreach_batch(ice, batch) {
      const enum iris_batch_name name = batch - &ice->batches[0];

      /* TODO(lucifer): once exec-queue creation lands, this is where
       * iris_lucifer_init_batch() would run and populate
       * batch->lucifer.exec_queue_id, mirroring iris_xe_init_batches().
       */
      batch->lucifer.engine_class =
         intel_engine_class_to_lucifer(engine_classes[name]);
   }
}

void iris_lucifer_destroy_batch(struct iris_batch *batch)
{
   /* LUCIFER_KMD has no exec-queue object to wait-idle/destroy yet
    * lucifer_batch_submit() can touch this batch's BOs after we return --
    * otherwise a submission racing with iris_bufmgr teardown can
    * double-unreference a BO (mirrors iris_xe_wait_exec_queue_idle(), just
    * keyed off the batch's own signalling syncobj instead of an
    * exec_queue_id, since we don't have queue semantics yet). */

   if (util_dynarray_num_elements(&batch->syncobjs, struct iris_syncobj *) == 0)
      return;

   struct iris_bufmgr *bufmgr = batch->screen->bufmgr;
   int fd = iris_bufmgr_get_fd(bufmgr);
   struct iris_syncobj *syncobj = iris_batch_get_signal_syncobj(batch);

   struct drm_syncobj_wait wait = {
      .handles = (uintptr_t)&syncobj->handle,
      .count_handles = 1,
      .timeout_nsec = INT64_MAX,
   };
   intel_ioctl(fd, DRM_IOCTL_SYNCOBJ_WAIT, &wait);
}

bool iris_lucifer_replace_batch(struct iris_batch *batch)
{
   /* TODO(lucifer): with no exec-queue object, there is nothing to
    * "replace" yet — a hung/banned batch has no per-queue state to
    * recreate. Once exec-queue semantics land, mirror
    * iris_xe_replace_batch(): create a new queue, swap it in, and call
    * iris_lost_context_state(batch).
    */
   return false;
}