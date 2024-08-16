#include "blob.hpp"
using namespace sqk::io::blob;

const char* dpdk_cli_override_opts =
    "--log-level=lib.eal:4 "
    "--log-level=lib.malloc:4 "
    "--log-level=lib.ring:4 "
    "--log-level=lib.mempool:4 "
    "--log-level=lib.timer:4 "
    "--log-level=pmd:4 "
    "--log-level=lib.hash:4 "
    "--log-level=lib.lpm:4 "
    "--log-level=lib.kni:4 "
    "--log-level=lib.acl:4 "
    "--log-level=lib.power:4 "
    "--log-level=lib.meter:4 "
    "--log-level=lib.sched:4 "
    "--log-level=lib.port:4 "
    "--log-level=lib.table:4 "
    "--log-level=lib.pipeline:4 "
    "--log-level=lib.mbuf:4 "
    "--log-level=lib.cryptodev:4 "
    "--log-level=lib.efd:4 "
    "--log-level=lib.eventdev:4 "
    "--log-level=lib.gso:4 "
    "--log-level=user1:4 "
    "--log-level=user2:4 "
    "--log-level=user3:4 "
    "--log-level=user4:4 "
    "--log-level=user5:4 "
    "--log-level=user6:4 "
    "--log-level=user7:4 "
    "--log-level=user8:4 "
    "--no-telemetry";

int main(int argc, char* argv[]) {
    sqk::scheduler = new sqk::SQKScheduler;
    BlobEnv env;
    BlobOptions opts;
    opts.name = "blob_test";
    opts.env_context = const_cast<char*>(dpdk_cli_override_opts);
    opts.conf_file = "../src/tests/io/blob/hello_blob.json";

    auto loop_task = [](BlobEnv& env, BlobOptions& opts) -> sqk::Task<void> {
        co_await env.setup(opts);
        auto loop = env.make_event_loop("spdk_poll");
        auto task = [](BlobEnv& env) -> sqk::Task<void> {
            auto blob_dev = env.make_blob_dev("Malloc0");
            auto bs = co_await blob_dev.init_blobstore();
            auto unit_size = bs.get_io_unit_size();

            auto blob = co_await bs.make_blob();
            auto free = bs.free_cluster_count();
            co_await blob.resize(free);
            co_await blob.sync_meta_data();

            auto write_buf = dma_alloc(unit_size, 0x1000);
            memset(write_buf, 0x5a, unit_size);
            co_await blob.write(write_buf, 0, 1);

            auto read_buf = dma_alloc(unit_size, 0x1000);
            co_await blob.read(read_buf, 0, 1);

            auto cmp_res = memcmp(write_buf, read_buf, unit_size);
            S_INFO("blob test: {}", cmp_res);
            sqk::scheduler->stop();
        };
        sqk::scheduler->enqueue(task(env));
        for (;;) {
            loop.poll();
            co_yield nullptr;
        }
    };
    sqk::scheduler->enqueue(loop_task(env, opts));
    sqk::scheduler->run();
    delete sqk::scheduler;
    return 0;
}
