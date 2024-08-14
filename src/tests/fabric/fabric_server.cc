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
        int rc;
        Info in2 = co_await pep.next();

        S_INFO("evt, cnt: {}", cnt);
        Endpoint ep(domain, in2, eq, cq);
        co_await ep.accept();

        S_INFO("evt, cnt: {}", cnt);
        memcpy(buf.buf_, &keys, sizeof(keys));
        S_INFO("send() before");
        co_await ep.send(buf, sizeof(keys), mr);

        S_INFO("send() end");

        co_await ep.wait_disconn();

        S_INFO("evt, cnt: {}", cnt);
    }
}

int main() {
    S_LOGGER_SETUP;
    sqk::scheduler = new sqk::SQKScheduler;
    keys keys;
    Info hint =
        Info()
            .with_caps(FI_MSG | FI_RMA)
            .with_addr_format(FI_SOCKADDR_IN)
            .with_ep_attr([](auto ea) { ea->type = FI_EP_RDM; })
            .with_mode(FI_CONTEXT)
            .with_domain_attr([](auto da) { da->mr_mode = FI_RX_CQ_DATA; })
            .with_fabric_attr([](auto fa) {
                fa->name = strdup("verbs;ofi_rxm");
            });

    hint.print();

    auto info = Info::get_info(std::nullopt, "1234", FI_SOURCE, hint);
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
            rc = cq.poll();
            co_yield nullptr;
        }
        co_return;
    }(cq));

    sqk::scheduler->enqueue(run(fabric, info, eq, cq, domain, keys, buf, mr));
    sqk::scheduler->run();
    return 0;
}
