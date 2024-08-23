#ifndef SQK_IO_BLOB_BLOB_HPP_
#define SQK_IO_BLOB_BLOB_HPP_

#include "core.hpp"
#include "spdk/bdev.h"
#include "spdk/blob.h"
#include "spdk/blob_bdev.h"
#include "spdk/env.h"
#include "spdk/init.h"
#include "spdk/log.h"
#include "spdk/thread.h"

namespace sqk::io::blob {
using std::enable_shared_from_this;

struct BlobOptions: spdk_env_opts {
    std::string conf_file;

    BlobOptions() {
        spdk_env_opts_init(this);
    }
};

inline void common_cb(void* cb_arg, int bserrno) {
    auto awaker = static_cast<Awaker<int>*>(cb_arg);
    awaker->wake(std::move(bserrno));
}

struct Blob {
    Task<int> resize(size_t size) {
        Awaker<int> awaker;
        spdk_blob_resize(blob_, size, common_cb, &awaker);
        int rc = co_await awaker;
        co_return rc;
    }

    size_t get_num_clusters() {
        return spdk_blob_get_num_clusters(blob_);
    }

    Task<int> sync_meta_data() {
        Awaker<int> awaker;
        spdk_blob_sync_md(blob_, common_cb, &awaker);
        int rc = co_await awaker;
        co_return rc;
    }

    Task<int> write(uint8_t* buf, size_t offset, size_t length) {
        Awaker<int> awaker;
        spdk_blob_io_write(
            blob_,
            channel_,
            buf,
            offset,
            length,
            common_cb,
            &awaker
        );
        int rc = co_await awaker;
        co_return rc;
    }

    Task<int> read(uint8_t* buf, size_t offset, size_t length) {
        Awaker<int> awaker;
        spdk_blob_io_read(
            blob_,
            channel_,
            buf,
            offset,
            length,
            common_cb,
            &awaker
        );
        int rc = co_await awaker;
        co_return rc;
    }

    Blob() {}

  private:
    friend class BlobStore;

    Blob(spdk_blob* blob, spdk_io_channel* channel) :
        blob_(blob),
        channel_(channel) {}

    spdk_io_channel* channel_;
    spdk_blob* blob_;
};

struct BlobStore {
    size_t free_cluster_count() {
        return spdk_bs_free_cluster_count(bs_);
    }

    size_t get_io_unit_size() {
        return spdk_bs_get_io_unit_size(bs_);
    }

    Task<Blob> make_blob() {
        Awaker<spdk_blob_id> awaker;
        spdk_bs_create_blob(
            bs_,
            [](void* cb_arg, spdk_blob_id blobid, int bserrno) {
                assert(bserrno == 0);
                auto waker = static_cast<Awaker<spdk_blob_id>*>(cb_arg);
                waker->wake(std::move(blobid));
            },
            &awaker
        );
        auto blob_id = co_await awaker;
        Awaker<spdk_blob*> blob_awaker;
        spdk_bs_open_blob(
            bs_,
            blob_id,
            [](void* cb_arg, struct spdk_blob* blob, int bserrno) {
                assert(bserrno == 0);
                auto waker = static_cast<Awaker<spdk_blob*>*>(cb_arg);
                waker->wake(std::move(blob));
            },
            &blob_awaker
        );
        auto blob = co_await blob_awaker;
        spdk_io_channel* channel = spdk_bs_alloc_io_channel(bs_);
        co_return Blob(blob, channel);
    }

    BlobStore() {}

  private:
    friend struct BlobDev;

    BlobStore(spdk_blob_store* bs) : bs_(bs) {}

    spdk_blob_store* bs_;
};

struct BlobDev {
    BlobDev(spdk_bs_dev* bs_dev) : bs_dev_(bs_dev) {}

    Task<BlobStore> init_blobstore() {
        Awaker<spdk_blob_store*> awaker;
        spdk_bs_init(
            bs_dev_,
            nullptr,
            [](void* cb_arg, struct spdk_blob_store* bs, int bserrno) {
                assert(bserrno == 0);
                auto awaker = static_cast<Awaker<spdk_blob_store*>*>(cb_arg);
                awaker->wake(std::move(bs));
            },
            &awaker
        );
        auto bs = co_await awaker;
        co_return BlobStore(bs);
    }

  private:
    friend class BlobEnv;
    spdk_bs_dev* bs_dev_;
};

struct BlobEventLoop {
    void poll() {
        spdk_thread_poll(thread_, 0, 0);
    }

  private:
    friend class BlobEnv;

    BlobEventLoop(spdk_thread* thread) : thread_(thread) {}

    spdk_thread* thread_;
};

struct BlobEnv: enable_shared_from_this<BlobEnv> {
    Task<int> setup(BlobOptions& opt) {
        int rc;
        rc = spdk_env_init(&opt);
        if (rc) {
            co_return rc;
        }
        rc = spdk_thread_lib_init(NULL, 0);
        if (rc) {
            co_return rc;
        }
        spdk_log_set_print_level(SPDK_LOG_DEBUG);
        spdk_log_set_level(SPDK_LOG_DEBUG);

        auto thread = spdk_thread_create("init_thread", nullptr);
        if (!thread) {
            co_return -1;
        }
        spdk_set_thread(thread);
        CheckableAwaker<int> waker;
        spdk_subsystem_init_from_json_config(
            opt.conf_file.c_str(),
            SPDK_DEFAULT_RPC_ADDR,
            [](int rc, void* cb_arg) {
                auto waker = static_cast<CheckableAwaker<int>*>(cb_arg);
                waker->wake(std::move(rc)) /*FIXME*/;
            },
            &waker,
            true
        );
        while (!waker.is_awaked()) {
            spdk_thread_poll(thread, 0, 0);
        }
        spdk_thread_exit(thread);
        spdk_set_thread(nullptr);
        while (!spdk_thread_is_exited(thread)) {
            spdk_thread_poll(thread, 0, 0);
        }
        spdk_thread_destroy(thread);
        rc = co_await waker;
        co_return rc;
    }

    BlobEventLoop make_event_loop(std::string name) {
        spdk_thread* thread = spdk_thread_create(name.c_str(), nullptr);
        if (!thread) {
            throw std::bad_alloc {};
        }
        spdk_set_thread(thread);
        return BlobEventLoop(thread);
    }

    BlobDev make_blob_dev(std::string dev_name) {
        spdk_bs_dev* bs_dev;
        int rc = spdk_bdev_create_bs_dev_ext(
            dev_name.c_str(),
            [](enum spdk_bdev_event_type type,
               struct spdk_bdev* bdev,
               void* cb_arg) { S_DBUG("create bdev: {}", fmt::ptr(bdev)); },
            NULL,
            &bs_dev
        );
        return BlobDev(bs_dev);
    }
};

inline uint8_t* dma_alloc(
    size_t size,
    size_t align,
    int socket_id = SPDK_ENV_SOCKET_ID_ANY,
    uint32_t flags = SPDK_MALLOC_DMA
) {
    return static_cast<uint8_t*>(
        spdk_malloc(size, align, nullptr, socket_id, flags)
    );
}

} // namespace sqk::io::blob

#endif // !SQK_IO_BLOB_BLOB_HPP_
