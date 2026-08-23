#include "iris_kmd_backend.h"
#include "iris/iris_bufmgr.h"

static uint32_t
i915_gem_create(struct iris_bufmgr *bufmgr,
                 const struct intel_memory_class_instance **regions,
                 uint16_t regions_count, uint64_t size,
                 enum iris_heap heap_flags, enum bo_alloc_flags alloc_flags)
{
   UNREACHABLE("i915/xe backend not implemented");
}

static uint32_t
i915_gem_create_userptr(struct iris_bufmgr *bufmgr, void *ptr, uint64_t size)
{
   UNREACHABLE("i915/xe backend not implemented");
}

static int
i915_gem_close(struct iris_bufmgr *bufmgr, struct iris_bo *bo)
{
   UNREACHABLE("i915/xe backend not implemented");
}

static bool
i915_bo_madvise(struct iris_bo *bo, enum iris_madvice state)
{
   UNREACHABLE("i915/xe backend not implemented");
}

static int
i915_bo_set_caching(struct iris_bo *bo, bool cached)
{
   UNREACHABLE("i915/xe backend not implemented");
}

static void *
i915_gem_mmap(struct iris_bufmgr *bufmgr, struct iris_bo *bo)
{
   UNREACHABLE("i915/xe backend not implemented");
}

static enum pipe_reset_status
i915_batch_check_for_reset(struct iris_batch *batch)
{
   UNREACHABLE("i915/xe backend not implemented");
}

static int
i915_batch_submit(struct iris_batch *batch)
{
   UNREACHABLE("i915/xe backend not implemented");
}

static bool
i915_gem_vm_bind(struct iris_bo *bo, enum bo_alloc_flags flags)
{
   UNREACHABLE("i915/xe backend not implemented");
}

static bool
i915_gem_vm_unbind(struct iris_bo *bo)
{
   UNREACHABLE("i915/xe backend not implemented");
}

static const struct iris_kmd_backend i915_backend = {
   .gem_create             = i915_gem_create,
   .gem_create_userptr      = i915_gem_create_userptr,
   .gem_close               = i915_gem_close,
   .bo_madvise              = i915_bo_madvise,
   .bo_set_caching          = i915_bo_set_caching,
   .gem_mmap                = i915_gem_mmap,
   .batch_check_for_reset   = i915_batch_check_for_reset,
   .batch_submit            = i915_batch_submit,
   .gem_vm_bind             = i915_gem_vm_bind,
   .gem_vm_unbind           = i915_gem_vm_unbind,
};

const struct iris_kmd_backend *
i915_get_backend(void)
{
   return &i915_backend;
}