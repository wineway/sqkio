#include "core.hpp"
#include "spdk/bdev.h"
#include "spdk/env.h"
#include "spdk/event.h"
#include "spdk/blob_bdev.h"
#include "spdk/blob.h"
#include "spdk/log.h"
#include "spdk/string.h"



namespace sqk::io::blob {
using std::enable_shared_from_this;
struct BlobOptions : spdk_env_opts {
    std::string conf_file;
};

struct BlobEnv : enable_shared_from_this<BlobEnv> {
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

        Awaker<int> waker;
        spdk_subsystem_init_from_json_config(
            opt.conf_file.c_str(),
            SPDK_DEFAULT_RPC_ADDR,
            [](int rc, void *cb_arg){
                auto waker = static_cast<Awaker<int>*>(cb_arg);
                waker->wake(std::move(rc))/*FIXME*/;
            },
            &waker,
            true
        );
        rc = co_await waker;
        co_return rc;
    }
};

}
