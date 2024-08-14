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
    S_INFO("run!!!");

    AddressVectorAttr av_attr {};
    av_attr->type = info.get_domain_attr()->av_type;
    av_attr->count = send_first ? 1 : 100;
    AddressVector av(domain, av_attr);
    Address srv_addr {};
    if (send_first)
        srv_addr = av.insert(info.dst_addr());

    uint64_t cnt {};

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

    Endpoint* ep;
    if (!send_first)
        ep = new Endpoint(domain, info, eq, cq, av);
    for (;;) {
        if (send_first) {
            auto endpoint = Endpoint(domain, info, eq, cq, av);
            ep = &endpoint;
            IOVector iov(1024);
            ep->get_name(iov);
            memcpy(buf.buf_, iov.buf_, iov.size_);
            S_INFO("before send...{}/{}", keys.rkey, keys.addr);
            co_await ep->send(buf, iov.size_, mr);
            S_INFO("send() end");
            S_INFO("before recv {}/{}", keys.addr, keys.rkey);
            auto addr = co_await ep->recv(buf, sizeof(keys), mr);
            memcpy(&keys, buf.buf_, sizeof(keys));
            S_INFO("recv {}/{} from {}", keys.addr, keys.rkey, *addr());
            co_await ep
                ->write(buf, buf.capicity_, mr, keys.addr, keys.rkey, srv_addr);
            S_INFO("write() end");
            S_INFO("end conn");
        } else {
            IOVector iov(1024);
            S_INFO("before recv {}/{}", keys.addr, keys.rkey);
            auto addr = co_await ep->recv(buf, 1024, mr);
            memcpy(iov.buf_, buf.buf_, iov.size_);
            auto address = av.insert(iov.buf_);
            keys.rkey = mr.key();
            keys.addr = reinterpret_cast<uint64_t>(buf.buf_);
            memcpy(buf.buf_, &keys, sizeof(keys));
            S_INFO("before send {}/{}", keys.addr, keys.rkey);
            co_await ep->send(buf, sizeof(keys), mr, address);
            S_INFO("send() end");
        }
        S_INFO("evt, cnt: {}", cnt);
    }
    if (!send_first)
        delete ep;
}

int main(int argc, char* argv[]) {
    S_LOGGER_SETUP;
    if (argc == 2) {
        S_INFO("I'm client; {}", argc);
        send_first = 1;
    }
    auto da_name = getenv("DA_NAME");
    sqk::scheduler = new sqk::SQKScheduler;
    keys keys;
    Info hint =
        Info()
            .with_caps(FI_MSG | FI_RMA)
            .with_ep_attr([](auto ea) { ea->type = FI_EP_RDM; })
            // .with_mode(FI_CONTEXT)
            .with_fabric_attr([](auto fa) { fa->prov_name = strdup("verbs"); })
            .with_domain_attr([&](auto da) {
                if (da_name) {
                    da->name = strdup(da_name);
                }
                da->mr_mode = FI_MR_LOCAL | FI_MR_RAW | FI_MR_VIRT_ADDR
                    | FI_MR_ALLOCATED | FI_MR_PROV_KEY | FI_MR_ENDPOINT;
            });

    hint.print();

    std::optional<std::string> node = std::nullopt;
    auto flag = FI_SOURCE;
    std::optional<std::string> port = "1234";

    if (send_first) {
        flag = 0;
        node = argv[1];
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
        FI_REMOTE_READ | FI_REMOTE_WRITE | FI_SEND | FI_RECV | FI_WRITE
            | FI_READ
    );

    keys.rkey = mr.key();
    keys.addr = (uint64_t)buf.buf_;

    S_INFO("mr key: {}, addr: {}", keys.rkey, keys.addr);

    S_INFO("wawwawawa");
    sqk::scheduler->enqueue(run(fabric, info, eq, cq, domain, keys, buf, mr, hint));
    sqk::scheduler->run();
    return 0;
}
