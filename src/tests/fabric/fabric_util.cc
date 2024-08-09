#include <unistd.h>

#include "fabric.hpp"
using namespace sqk::net::fab;

struct keys {
    uint64_t rkey;
    uint64_t addr;
};

sqk::Task<int>
run(Fabric& fabric,
    Info& info,
    EventQueue& eq,
    CompletionQueue& cq,
    Domain& domain,
    keys& keys,
    MemoryBuffer& buf,
    MemoryRegion& mr) {
    PassiveEndpoint pep(fabric, info, eq);
    pep.listen();

    uint64_t cnt {};
    for (;;) {
        Event event;
        int rc;
        EventQueueCompleteEntry ent;

        while (true) {
            rc = eq.sread(event, ent, -1, 0);
            if (rc == -EAGAIN) {
                cnt++;
                continue;
            }
            break;
        }

        spdlog::info("evt: %d, cnt: %lu", event, cnt);
        Info in2 = Info::from_raw(ent->info);
        Endpoint ep(domain, in2, eq, cq);
        co_await ep.accept();

        spdlog::info("evt: %d, cnt: %lu\n", event, cnt);
        memcpy(buf.buf_, &keys, sizeof(keys));
        std::cout << "send() before" << std::endl;
        co_await ep.send(buf, sizeof(keys), mr);

        std::cout << "send() end" << std::endl;
        while (true) {
            rc = eq.poll(event, 0);
            if (rc == -EAGAIN) {
                cnt++;
                continue;
            }
            break;
        }

        spdlog::info("evt: %d, cnt: %lu\n", event, cnt);
    }
}

int main() {
    S_LOGGER_SETUP;
    S_INFO("S_INFO");
    SPDLOG_INFO("info");
    SPDLOG_DEBUG("debug");
    printf("SPDLOG_ACTIVE_LEVEL: %d\n", SPDLOG_ACTIVE_LEVEL);
    sqk::scheduler = new sqk::SQKScheduler;
    keys keys;
    Info hint = Info()
                    .with_caps(FI_MSG | FI_RMA)
                    .with_addr_format(FI_SOCKADDR_IN)
                    .with_ep_attr([](auto ea) { ea->type = FI_EP_MSG; })
                    .with_mode(FI_MR_LOCAL | FI_CONTEXT | FI_RX_CQ_DATA)
                    .with_domain_attr([](auto da) {
                        da->mr_mode =
                            FI_MR_BASIC; // | FI_MR_ENDPOINT | FI_MR_ALLOCATED |
                        // FI_MR_PROV_KEY | FI_MR_VIRT_ADDR | FI_MR_RAW;
                    });

    hint.print();

    auto info =
        Info::get_info(std::nullopt, "1234", FI_SOURCE, std::move(hint));
    info.print();

    Fabric fabric(info.get_fabric_attr());

    EventQueueAttr eq_attr = EventQueueAttr().with_wait_obj(FI_WAIT_UNSPEC);
    EventQueue eq(fabric, eq_attr);
    Domain domain(fabric, info);
    CompletionQueueAttr cq_attr = CompletionQueueAttr()
                                      .with_wait_obj(FI_WAIT_UNSPEC)
                                      .with_format(FI_CQ_FORMAT_MSG)
                                      .with_wait_cond(FI_CQ_COND_NONE);
    CompletionQueue cq(domain, cq_attr);
    MemoryBuffer buf(32 * 1024 * 1024);
    memset(buf.buf_, 0, buf.capicity_);
    MemoryRegion mr(
        domain,
        buf,
        FI_REMOTE_READ | FI_REMOTE_WRITE | FI_SEND | FI_RECV
    );

    keys.rkey = mr.key();
    keys.addr = (uint64_t)buf.buf_;

    // spdlog::info("1. eq: %p", &eq);
    sqk::scheduler->enqueue([](EventQueue& eq) -> sqk::Task<void> {
        Event event;
        int rc;
        EventQueueCompleteEntry ent;
        for (;;) {
            rc = eq.poll(event, 0);
            co_yield nullptr;
        }
        co_return;
    }(eq));

    sqk::scheduler->enqueue([](CompletionQueue& cq) -> sqk::Task<void> {
        CompletionQueueMsgEntry cqe;
        for (;;) {
            int rc;
            rc = cq.read(cqe);
            if (rc == 1) {
                auto awaker = static_cast<sqk::Awaker*>(cqe->op_context);
                awaker->wake();
                std::cout << "awaker: " << awaker << std::endl;
            }
            if (rc != -EAGAIN) {
                std::cout << "rc: " << rc << std::endl;
            }
            co_yield nullptr;
        }
        co_return;
    }(cq));

    sqk::scheduler->run(run(fabric, info, eq, cq, domain, keys, buf, mr));
    return 0;
}
