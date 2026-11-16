//
// Created by linus on 13.10.26.
//

#ifndef _VESPERA_DRM_BUF_H_
#define _VESPERA_DRM_BUF_H_

struct dma_buf_sync {
    /**
     * @flags: Set of access flags
     *
     * DMA_BUF_SYNC_START:
     *     Indicates the start of a map access session.
     *
     * DMA_BUF_SYNC_END:
     *     Indicates the end of a map access session.
     *
     * DMA_BUF_SYNC_READ:
     *     Indicates that the mapped DMA buffer will be read by the
     *     client via the CPU map.
     *
     * DMA_BUF_SYNC_WRITE:
     *     Indicates that the mapped DMA buffer will be written by the
     *     client via the CPU map.
     *
     * DMA_BUF_SYNC_RW:
     *     An alias for DMA_BUF_SYNC_READ | DMA_BUF_SYNC_WRITE.
     */
    u64 flags;
};

struct dma_buf_export_sync_file {
    /**
     * @flags: Read/write flags
     *
     * Must be DMA_BUF_SYNC_READ, DMA_BUF_SYNC_WRITE, or both.
     *
     * If DMA_BUF_SYNC_READ is set and DMA_BUF_SYNC_WRITE is not set,
     * the returned sync file waits on any writers of the dma-buf to
     * complete.  Waiting on the returned sync file is equivalent to
     * poll() with POLLIN.
     *
     * If DMA_BUF_SYNC_WRITE is set, the returned sync file waits on
     * any users of the dma-buf (read or write) to complete.  Waiting
     * on the returned sync file is equivalent to poll() with POLLOUT.
     * If both DMA_BUF_SYNC_WRITE and DMA_BUF_SYNC_READ are set, this
     * is equivalent to just DMA_BUF_SYNC_WRITE.
     */
    u32 flags;
    /** @fd: Returned sync file descriptor */
    i32 fd;
};

struct dma_buf_import_sync_file {
    /**
     * @flags: Read/write flags
     *
     * Must be DMA_BUF_SYNC_READ, DMA_BUF_SYNC_WRITE, or both.
     *
     * If DMA_BUF_SYNC_READ is set and DMA_BUF_SYNC_WRITE is not set,
     * this inserts the sync_file as a read-only fence.  Any subsequent
     * implicitly synchronized writes to this dma-buf will wait on this
     * fence but reads will not.
     *
     * If DMA_BUF_SYNC_WRITE is set, this inserts the sync_file as a
     * write fence.  All subsequent implicitly synchronized access to
     * this dma-buf will wait on this fence.
     */
    u32 flags;
    /** @fd: Sync file descriptor */
    i32 fd;
};

#define DMA_BUF_SYNC_READ      (1 << 0)
#define DMA_BUF_SYNC_WRITE     (2 << 0)
#define DMA_BUF_SYNC_RW        (DMA_BUF_SYNC_READ | DMA_BUF_SYNC_WRITE)
#define DMA_BUF_SYNC_START     (0 << 2)
#define DMA_BUF_SYNC_END       (1 << 2)
#define DMA_BUF_SYNC_VALID_FLAGS_MASK \
	(DMA_BUF_SYNC_RW | DMA_BUF_SYNC_END)

#define DMA_BUF_NAME_LEN	32

#define DMA_BUF_BASE		'b'
#define DMA_BUF_IOCTL_SYNC	_IOW(DMA_BUF_BASE, 0, struct dma_buf_sync)

#define DMA_BUF_SET_NAME	_IOW(DMA_BUF_BASE, 1, const char *)
#define DMA_BUF_SET_NAME_A	_IOW(DMA_BUF_BASE, 1, u32)
#define DMA_BUF_SET_NAME_B	_IOW(DMA_BUF_BASE, 1, u64)
#define DMA_BUF_IOCTL_EXPORT_SYNC_FILE	_IOWR(DMA_BUF_BASE, 2, struct dma_buf_export_sync_file)
#define DMA_BUF_IOCTL_IMPORT_SYNC_FILE	_IOW(DMA_BUF_BASE, 3, struct dma_buf_import_sync_file)

#endif //_VESPERA_DRM_BUF_H_
