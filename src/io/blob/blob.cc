#include "spdk/bdev.h"
#include "spdk/env.h"
#include "spdk/event.h"
#include "spdk/blob_bdev.h"
#include "spdk/blob.h"
#include "spdk/log.h"
#include "core.hpp"

using namespace sqk;

void bdev_create_wakeup_fn(enum spdk_bdev_event_type type,
                           struct spdk_bdev *bdev, void *waker) {
    S_INFO("bdev_create_wakeup_fn type: {}", (int)type);
}
void init_waker_fn(void *cb_arg, struct spdk_blob_store *bs,
		int bserrno) {
    assert(bserrno == 0);
    auto waker = static_cast<Awaker<spdk_blob_store*>*>(cb_arg);
    waker->wake(std::move(bs));
}
void blob_wake_fn(void *cb_arg, spdk_blob_id blobid, int bserrno) {
    assert(bserrno == 0);
    auto waker = static_cast<Awaker<spdk_blob_id>*>(cb_arg);
    waker->wake(std::move(blobid));
}

static void
open_cb(void *cb_arg, struct spdk_blob *blob, int bserrno)
{

    assert(bserrno == 0);
    auto waker = static_cast<Awaker<spdk_blob*>*>(cb_arg);
    waker->wake(std::move(blob));
}
static void
resize_cb(void *cb_arg, int bserrno)
{
    assert(bserrno == 0);
    auto waker = static_cast<Awaker<void>*>(cb_arg);
    waker->wake();
}

void wakeup_fn1(int rc, void *cb_arg) {
    S_INFO("wakeup_fn1");
}
void wakeup_fn(int rc, void *cb_arg) {
    assert(!rc);
    auto waker = static_cast<Awaker<void>*>(cb_arg);
    waker->wake();
}

const char *
dpdk_cli_override_opts = "--log-level=lib.eal:4 "
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

char *conf;

Task<int> run() {
    S_INFO("run");
  int rc;
  spdk_bs_dev *bs_dev;
  Awaker<void> waker{};

  spdk_env_opts opts{};
  spdk_env_opts_init(&opts);
  opts.env_context = (char *)dpdk_cli_override_opts;
  opts.name = "hello";
  rc = spdk_env_init(&opts);
  assert(!rc);

  rc = spdk_thread_lib_init(NULL, 0);
  assert(!rc);
  spdk_log_set_print_level(SPDK_LOG_DEBUG);
  spdk_log_set_level(SPDK_LOG_DEBUG);

  S_INFO("before start");
  spdk_thread *thread = spdk_thread_create("spdk_thread", NULL);
  assert(thread);
  spdk_set_thread(thread);

  spdk_subsystem_init_from_json_config(
conf, SPDK_DEFAULT_RPC_ADDR, wakeup_fn, &waker, true);


  scheduler->enqueue([](spdk_thread *thr) -> Task<void> {
    while (true) {
      spdk_thread_poll(thr, 0, 0);
      co_yield nullptr;
    }
    co_return;
  }(thread));

  co_await waker;
  S_INFO("after start");

  S_INFO("before create");
  rc = spdk_bdev_create_bs_dev_ext("Malloc0", bdev_create_wakeup_fn, NULL,
                                   &bs_dev);
  assert(!rc);
  S_INFO("after create");

  S_INFO("before init");
  Awaker<spdk_blob_store *> waker2{};
  spdk_bs_init(bs_dev, NULL, init_waker_fn, &waker2);
  auto bs = co_await waker2;
  int io_unit_size = spdk_bs_get_io_unit_size(bs);
  S_INFO("after init");

  S_INFO("before blob");
  Awaker<spdk_blob_id> waker3{};
  spdk_bs_create_blob(bs, blob_wake_fn, &waker3);
  spdk_blob_id id = co_await waker3;
  S_INFO("after blob");

  Awaker<spdk_blob *> waker4;
  spdk_bs_open_blob(bs, id, open_cb, &waker4);
  auto blob = co_await waker4;

  uint64_t free = spdk_bs_free_cluster_count(bs);
  spdk_blob_resize(blob, free, resize_cb, &waker);
  uint64_t total = spdk_blob_get_num_clusters(blob);
    co_await waker;

  spdk_blob_sync_md(blob, resize_cb, &waker);
    co_await waker;
  uint8_t *write_buff = (uint8_t *)spdk_malloc(
      io_unit_size, 0x1000, NULL, SPDK_ENV_LCORE_ID_ANY, SPDK_MALLOC_DMA);
  assert(write_buff != nullptr);
  memset(write_buff, 0x5a, io_unit_size);
  spdk_io_channel *channel = spdk_bs_alloc_io_channel(bs);

  spdk_blob_io_write(blob, channel, write_buff, 0, 1, resize_cb, &waker);
  co_await waker;

  uint8_t *read_buff = (uint8_t *)spdk_malloc(
      io_unit_size, 0x1000, NULL, SPDK_ENV_LCORE_ID_ANY, SPDK_MALLOC_DMA);

  spdk_blob_io_read(blob, channel, read_buff, 0, 1, resize_cb, &waker);
  co_await waker;

  int match_res = memcmp(write_buff, read_buff, io_unit_size);

  if (match_res) {
    S_WARN("data not matches!!!");
  } else {
    S_INFO("data read complete and matches!!!");
  }
  co_return 0;
}

int main (int argc, char *argv[]) {
    S_LOGGER_SETUP;
    conf = argv[1];
    S_INFO("start.... {}", conf);

    scheduler = new SQKScheduler;
    scheduler->enqueue(run());
    scheduler->run();

}
