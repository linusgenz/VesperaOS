#include "iris_kmd_backend.h"
#include "iris/iris_bufmgr.h"

static uint32_t
xe_gem_create(struct iris_bufmgr *bufmgr,
              const struct intel_memory_class_instance **regions,
              uint16_t regions_count, uint64_t size,
              enum iris_heap heap_flags, enum bo_alloc_flags alloc_flags)
{
   UNREACHABLE("i915/xe backend not implemented");
}

static uint32_t
xe_gem_create_userptr(struct iris_bufmgr *bufmgr, void *ptr, uint64_t size)
{
   UNREACHABLE("i915/xe backend not implemented");
}

static int
xe_gem_close(struct iris_bufmgr *bufmgr, struct iris_bo *bo)
{
   UNREACHABLE("i915/xe backend not implemented");
}

static bool
xe_bo_madvise(struct iris_bo *bo, enum iris_madvice state)
{
   UNREACHABLE("i915/xe backend not implemented");
}

static int
xe_bo_set_caching(struct iris_bo *bo, bool cached)
{
   UNREACHABLE("i915/xe backend not implemented");
}

static void *
xe_gem_mmap(struct iris_bufmgr *bufmgr, struct iris_bo *bo)
{
   UNREACHABLE("i915/xe backend not implemented");
}

static enum pipe_reset_status
xe_batch_check_for_reset(struct iris_batch *batch)
{
   UNREACHABLE("i915/xe backend not implemented");
}

static int
xe_batch_submit(struct iris_batch *batch)
{
   UNREACHABLE("i915/xe backend not implemented");
}

static bool
xe_gem_vm_bind(struct iris_bo *bo, enum bo_alloc_flags flags)
{
   UNREACHABLE("i915/xe backend not implemented");
}

static bool
xe_gem_vm_unbind(struct iris_bo *bo)
{
   UNREACHABLE("i915/xe backend not implemented");
}

static const struct iris_kmd_backend xe_backend = {
   .gem_create              = xe_gem_create,
   .gem_create_userptr      = xe_gem_create_userptr,
   .gem_close               = xe_gem_close,
   .bo_madvise              = xe_bo_madvise,
   .bo_set_caching          = xe_bo_set_caching,
   .gem_mmap                = xe_gem_mmap,
   .batch_check_for_reset   = xe_batch_check_for_reset,
   .batch_submit            = xe_batch_submit,
   .gem_vm_bind             = xe_gem_vm_bind,
   .gem_vm_unbind           = xe_gem_vm_unbind,
};

const struct iris_kmd_backend *
xe_get_backend(void)
{
   return &xe_backend;
}