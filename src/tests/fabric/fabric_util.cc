#include <unistd.h>

#include "fabric.hpp"
using namespace sqk::net::fab;

struct keys {
    uint64_t rkey;
    uint64_t addr;
};

int main() {
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

    PassiveEndpoint pep(fabric, info);
    pep.bind(eq);
    pep.listen();

    uint64_t cnt {};
    for (;;) {
        Event event;
        int rc;
        EventQueueCompleteEntry ent;

        while (true) {
            rc = eq.read(event, ent, 0);
            if (rc == -EAGAIN) {
                cnt++;
                continue;
            }
            break;
        }

        printf("evt: %d, cnt: %lu\n", event, cnt);
        Info in2 = Info::from_raw(ent->info);
        Endpoint ep(domain, in2);
        ep.bind(eq, 0);
        ep.bind(cq, FI_TRANSMIT | FI_RECV);
        ep.accept();

        while (true) {
            rc = eq.read(event, ent, 0);
            if (rc == -EAGAIN) {
                cnt++;
                continue;
            }
            break;
        }

        printf("evt: %d, cnt: %lu\n", event, cnt);
        memcpy(buf.buf_, &keys, sizeof(keys));
        ep.send(buf, sizeof(keys), mr, nullptr);

        CompletionQueueMsgEntry cqe;
        while (true) {
            rc = cq.read(cqe);
            if (rc == -EAGAIN) {
                cnt++;
                continue;
            }
            printf("rc: %d, cnt: %lu\n", rc, cnt);
            break;
        }

        while (true) {
            rc = eq.read(event, ent, 0);
            if (rc == -EAGAIN) {
                cnt++;
                continue;
            }
            break;
        }

        printf("evt: %d, cnt: %lu\n", event, cnt);
    }
}
