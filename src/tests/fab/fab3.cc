#include <pthread.h>
#include <rdma/fabric.h>
#include <rdma/fi_cm.h>
#include <rdma/fi_endpoint.h>
#include <rdma/fi_errno.h>
#include <rdma/fi_rma.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cassert>

#define FIVER FI_VERSION(1, 1)

static int print_long_info(struct fi_info* cur) {
    char buf[8192];

    fi_tostr_r(buf, sizeof(buf), cur, FI_TYPE_INFO);
    printf("%s", buf);
    return EXIT_SUCCESS;
}

int main() {
    int rc;
    fi_info* hints;
    fi_info* info;
    hints = fi_allocinfo();

    // 1. get info
    rc = fi_getinfo(FIVER, "127.0.0.1", NULL, 0, hints, &info);
    assert(!rc);
    print_long_info(info);

    // 2. get domain
    fid_fabric* fabric;
    fid_eq* eq;
    fi_eq_attr eq_attr = {.wait_obj = FI_WAIT_UNSPEC};
    fid_domain* domain;

    rc = fi_fabric(info->fabric_attr, &fabric, NULL);
    assert(!rc);
    rc = fi_eq_open(fabric, &eq_attr, &eq, NULL);
    assert(!rc);

    rc = fi_domain(fabric, info, &domain, NULL);
    assert(!rc);

    // rc = fi_domain_bind(domain, &eq->fid, 0);
    // assert(!rc);

    fi_cq_attr cq_attr = {.wait_obj = FI_WAIT_NONE};
    cq_attr.size = info->tx_attr->size;

    struct fid_cq* new_txcq;
    struct fid_cq* new_rxcq;
    rc = fi_cq_open(domain, &cq_attr, &new_txcq, &new_txcq);
    assert(!rc);
    rc = fi_cq_open(domain, &cq_attr, &new_rxcq, &new_rxcq);
    assert(!rc);

    fid_av* new_av;
    fi_av_attr av_attr = {.type = FI_AV_MAP, .count = 1};
    rc = fi_av_open(domain, &av_attr, &new_av, NULL);
    assert(!rc);
    fid_ep* ep;
    rc = fi_endpoint(domain, info, &ep, NULL);
    assert(!rc);

    rc = fi_ep_bind(ep, &new_av->fid, 0);
    assert(!rc);
    rc = fi_ep_bind(ep, &new_txcq->fid, 0);
    assert(!rc);
    rc = fi_ep_bind(ep, &new_rxcq->fid, 0);
    assert(!rc);
    rc = fi_enable(ep);
    assert(!rc);

    // int buf_size = 131264;
    // int tx_size = 1024;

    // void* dev_host_buf = malloc(tx_size);
    // void* buf = malloc(buf_size);

    // fi_mr_reg(domain, buf, buf_size, uint64_t acs, uint64_t offset, uint64_t requested_key, uint64_t flags, struct fid_mr **mr, void *context)

    fi_addr_t fi_addr;
    rc = fi_av_insert(new_av, info->dest_addr, 1, &fi_addr, 0, NULL);
    assert(rc == 1);

#define FT_MAX_CTRL_MSG 1024
    size_t addrlen = FT_MAX_CTRL_MSG;
    char temp[FT_MAX_CTRL_MSG];
    rc = fi_getname(&ep->fid, temp, &addrlen);
    assert(!rc);

    printf("name: %s\n", temp);
}
