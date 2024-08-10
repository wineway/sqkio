#include <unistd.h>

#include "fabric.hpp"
using namespace sqk::net::fab;

bool send_first = 0;

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
    MemoryRegion& mr,
    Info& hint) {
    printf("run~~~\n");
    S_INFO("run!!!");

    Endpoint ep(domain, info, eq, cq);
    uint64_t cnt {};

    Address dst_addr;
    if (send_first) {
        auto node = "127.0.0.1";
        auto flag = 0;
    }

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

    for (;;) {
        if (send_first) {
            S_INFO("before send...");
            co_await ep.send(buf, sizeof(keys), mr, dst_addr);
            S_INFO("send() end");
            S_INFO("before recv {}/{}", keys.addr, keys.rkey);
            auto addr = co_await ep.recv(buf, sizeof(keys), mr);
            S_INFO("recv {}/{} from {}", keys.addr, keys.rkey, *addr());

        }

        else {
            S_INFO("before recv {}/{}", keys.addr, keys.rkey);
            auto addr = co_await ep.recv(buf, sizeof(keys), mr);
            S_INFO("recv {}/{} from {}", keys.addr, keys.rkey, *addr());
            co_await ep.send(buf, sizeof(keys), mr, addr);
            S_INFO("send() end");
        }
        co_await ep.wait_disconn();

        S_INFO("evt, cnt: {}", cnt);
    }
}

int main(int argc, char* argv[]) {
    S_LOGGER_SETUP;
    if (argc == 2) {
        S_INFO("I'm client; {}", argc);
        send_first = 1;
    }
    sqk::scheduler = new sqk::SQKScheduler;
    keys keys;
    Info hint =
        Info()
            .with_caps(FI_MSG | FI_RMA)
            .with_ep_attr([](auto ea) { ea->type = FI_EP_MSG; })
            .with_mode(FI_RX_CQ_DATA)
            .with_fabric_attr([](auto fa) { fa->prov_name = strdup("verbs"); })
            .with_domain_attr([](auto da) {
                da->mr_mode = FI_MR_LOCAL | FI_MR_VIRT_ADDR | FI_MR_ALLOCATED
                    | FI_MR_PROV_KEY;
            });

    hint.print();

    std::optional<std::string> node = std::nullopt;
    auto flag = FI_SOURCE;
    std::optional<std::string> port = "1234";

    if (send_first) {
        flag = 0;
        node = "127.0.0.1";
    }

    auto info = Info::get_info(node, port, flag, hint);

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

    S_INFO("wawwawawa");
    sqk::scheduler->run(run(fabric, info, eq, cq, domain, keys, buf, mr, hint));
    return 0;
}
