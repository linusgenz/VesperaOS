//
// Created by linus on 20.11.26.
//

#ifndef VESPERAOS_DMABUF_H
#define VESPERAOS_DMABUF_H

#include <uapi/vespera/dev/dma-buf.h>
#include <vespera/mm/vm_backing.h>
#include <vespera/types.h>
#include <vespera_errno.h>

#include <uapi/vespera/ioctl.h>

namespace kernel::vm {

    class DmaBuf : public VmBackingObject {
    public:
        DmaBuf() = default;
        ~DmaBuf() override = default;

        DmaBuf(const DmaBuf&) = delete;
        DmaBuf& operator=(const DmaBuf&) = delete;

        void ref() {
            __atomic_add_fetch(&refs_, 1, __ATOMIC_RELAXED);
        }

        void unref() {
            if (__atomic_sub_fetch(&refs_, 1, __ATOMIC_ACQ_REL) == 0) {
                delete this;
            }
        }

        void add_mapping() override { ref(); }
        void remove_mapping() override { unref(); }

        /// Generischer ioctl-Handler für DMA_BUF_IOCTL_SYNC
        virtual int ioctl(const u32 request, void* arg) {
            if (request != DMA_BUF_IOCTL_SYNC) return -ENOTTY;

            const auto* s = static_cast<const dma_buf_sync*>(arg);
            if (!s) return -EINVAL;
            if (s->flags & ~DMA_BUF_SYNC_VALID_FLAGS_MASK) return -EINVAL;
            if ((s->flags & DMA_BUF_SYNC_RW) == 0) return -EINVAL;

            return sync(s->flags);
        }

        static void handle_acquire(void* resource) {
            static_cast<DmaBuf*>(resource)->ref();
        }

        static void handle_destroy(void* resource) {
            static_cast<DmaBuf*>(resource)->unref();
        }

    protected:
        virtual int sync(u64 flags) {
            (void)flags;
            return 0;
        }

    private:
        u32 refs_ = 1;
    };

} // namespace kernel::vm

#endif // VESPERAOS_DMABUF_H