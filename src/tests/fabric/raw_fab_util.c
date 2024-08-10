#include <pthread.h>
#include <rdma/fabric.h>
#include <rdma/fi_cm.h>
#include <rdma/fi_endpoint.h>
#include <rdma/fi_errno.h>
#include <rdma/fi_rma.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FIVER FI_VERSION(1, 1)

static int print_long_info(struct fi_info* cur) {
    char buf[8192];

    fi_tostr_r(buf, sizeof(buf), cur, FI_TYPE_INFO);
    printf("%s", buf);
    return EXIT_SUCCESS;
}

struct fi_info* fi;
struct fi_info* hints;
struct fid_fabric* fabric;
struct fid_eq* eq;
struct fid_cq* cq;
struct fid_domain* domain;
struct fid_mr* mr;

struct fid_pep* pep;
struct fid_ep* ep;

void* buff;
size_t buff_size = 32 * 1024 * 1024;
pthread_t thread;
int run;

struct keys {
    uint64_t rkey;
    uint64_t addr;
};

struct ctx {
    pthread_t thread;
    pthread_mutex_t lock;
    pthread_cond_t cond;
    int ready;
    int count;
    int size;
};

struct ctx* ctx;
struct keys keys;

int common_init(const char* addr, uint64_t flags) {
    int ret;

    ret = fi_getinfo(FIVER, addr, "1234", flags, hints, &fi);
    if (ret) {
        perror("fi_getinfo");
        return ret;
    }

    print_long_info(fi);
    ret = fi_fabric(fi->fabric_attr, &fabric, NULL);
    if (ret) {
        perror("fi_fabric");
        return ret;
    }

    struct fi_eq_attr eq_attr = {
        .size = 0,
        .flags = 0,
        .wait_obj = FI_WAIT_UNSPEC,
        .signaling_vector = 0,
        .wait_set = NULL,
    };

    ret = fi_eq_open(fabric, &eq_attr, &eq, NULL);
    if (ret) {
        perror("fi_eq_open");
        return ret;
    }

    ret = fi_domain(fabric, fi, &domain, NULL);
    if (ret) {
        perror("fi_domain");
        return ret;
    }

    struct fi_cq_attr cq_attr = {
        .size = 0,
        .flags = 0,
        .format = FI_CQ_FORMAT_MSG,
        .wait_obj = FI_WAIT_UNSPEC,
        .signaling_vector = 0,
        .wait_cond = FI_CQ_COND_NONE,
        .wait_set = NULL,
    };

    ret = fi_cq_open(domain, &cq_attr, &cq, NULL);
    if (ret) {
        perror("fi_cq_open");
        return ret;
    }

    ret = fi_mr_reg(
        domain,
        buff,
        buff_size,
        FI_REMOTE_READ | FI_REMOTE_WRITE | FI_SEND | FI_RECV,
        0,
        0,
        0,
        &mr,
        NULL
    );
    if (ret) {
        perror("fi_mr_reg");
        return -1;
    }

    return 0;
}

int server_init(void) {
    int ret;

    keys.rkey = fi_mr_key(mr);

    ret = fi_passive_ep(fabric, fi, &pep, NULL);
    if (ret) {
        printf("ret : %d\n", ret);
        perror("fi_passive_ep");
        return ret;
    }

    ret = fi_pep_bind(pep, &eq->fid, 0);
    if (ret) {
        perror("fi_pep_bind(eq)");
        return ret;
    }

    ret = fi_listen(pep);
    if (ret) {
        perror("fi_listen");
        return ret;
    }

    struct fi_eq_cm_entry entry;
    uint32_t event;
    ssize_t rret;

    while (1) {
        printf("listening\n");

        rret = fi_eq_sread(eq, &event, &entry, sizeof(entry), -1, 0);
        if (rret != sizeof(entry)) {
            perror("fi_eq_sread");
            return (int)rret;
        }

        if (event != FI_CONNREQ) {
            fprintf(stderr, "invalid event %u\n", event);
            return -1;
        }

        printf("connection request\n");

        print_long_info(entry.info);
        ret = fi_endpoint(domain, entry.info, &ep, NULL);
        if (ret) {
            perror("fi_endpoint");
            return ret;
        }

        ret = fi_ep_bind(ep, &eq->fid, 0);
        if (ret) {
            perror("fi_ep_bind(eq)");
            return ret;
        }

        ret = fi_ep_bind(ep, &cq->fid, FI_TRANSMIT | FI_RECV);
        if (ret) {
            perror("fi_ep_bind(cq)");
            return ret;
        }

        ret = fi_accept(ep, NULL, 0);
        if (ret) {
            perror("fi_accept");
            return ret;
        }

        rret = fi_eq_sread(eq, &event, &entry, sizeof(entry), -1, 0);
        if (rret != sizeof(entry)) {
            perror("fi_eq_sread");
            return (int)rret;
        }

        if (event != FI_CONNECTED) {
            fprintf(stderr, "invalid event %u\n", event);
            return -1;
        }

        memcpy(buff, &keys, sizeof(keys));

        printf("server_init: %p\n", server_init);
        rret = fi_send(
            ep,
            buff,
            sizeof(keys),
            fi_mr_desc(mr),
            0,
            (void*)server_init
        );
        if (rret) {
            perror("fi_send");
            return (int)rret;
        }

        struct fi_cq_msg_entry comp;
        ret = fi_cq_sread(cq, &comp, 1, NULL, -1);
        printf("cb data: %p\n", comp.op_context);
        if (ret != 1) {
            perror("fi_cq_sread");
            return ret;
        }

        printf("connected\n");

        rret = fi_eq_sread(eq, &event, &entry, sizeof(entry), -1, 0);
        if (rret != sizeof(entry)) {
            perror("fi_eq_sread");
            return (int)rret;
        }

        if (event != FI_SHUTDOWN) {
            fprintf(stderr, "invalid event %u\n", event);
            return -1;
        }

        fi_close(&ep->fid);
    }

    return 0;
}

int server(void) {
    hints = fi_allocinfo();
    if (!hints) {
        perror("fi_allocinfo");
        return -1;
    }

    hints->caps = FI_MSG | FI_RMA;

    hints->addr_format = FI_SOCKADDR_IN;
    hints->ep_attr->type = FI_EP_MSG;
    hints->domain_attr->mr_mode = FI_MR_BASIC;
    hints->domain_attr->name = "mlx5_bond_0";
    hints->mode = FI_CONTEXT | FI_LOCAL_MR | FI_RX_CQ_DATA;

    int ret = common_init(NULL, FI_SOURCE);
    if (ret)
        return ret;

    ret = server_init();

    return ret;
}

void* cq_thread(void* arg) {
    struct fi_cq_msg_entry comp;
    ssize_t ret;
    struct fi_cq_err_entry err;
    const char* err_str;
    struct fi_eq_entry eq_entry;
    uint32_t event;

    while (run) {
        ret = fi_cq_sread(cq, &comp, 1, NULL, -1);
        if (!run)
            break;
        if (ret == -FI_EAGAIN)
            continue;

        if (ret != 1) {
            perror("fi_cq_sread");
            break;
        }

        if (comp.flags & FI_READ) {
            struct ctx* ctx = (struct ctx*)comp.op_context;
            pthread_mutex_lock(&ctx->lock);
            ctx->ready = 1;
            printf("receice ready\n");
            pthread_cond_signal(&ctx->cond);
            pthread_mutex_unlock(&ctx->lock);
        }
    }

    return NULL;
}

int client_init(void) {
    int ret;

    ret = fi_endpoint(domain, fi, &ep, NULL);
    if (ret) {
        perror("fi_endpoint");
        return ret;
    }

    ret = fi_ep_bind(ep, &eq->fid, 0);
    if (ret) {
        perror("fi_ep_bind(eq)");
        return ret;
    }

    ret = fi_ep_bind(ep, &cq->fid, FI_TRANSMIT | FI_RECV);
    if (ret) {
        perror("fi_ep_bind(cq)");
        return ret;
    }

    ret = fi_enable(ep);
    if (ret) {
        perror("fi_enable");
        return ret;
    }

    ssize_t rret;
    rret = fi_recv(ep, buff, sizeof(keys), fi_mr_desc(mr), 0, NULL);
    if (rret) {
        perror("fi_recv");
        return (int)rret;
    }

    ret = fi_connect(ep, fi->dest_addr, NULL, 0);
    if (ret) {
        perror("fi_connect");
        return ret;
    }

    struct fi_eq_cm_entry entry;
    uint32_t event;

    rret = fi_eq_sread(eq, &event, &entry, sizeof(entry), -1, 0);
    if (rret != sizeof(entry)) {
        perror("fi_eq_sread");
        return (int)rret;
    }

    if (event != FI_CONNECTED) {
        fprintf(stderr, "invalid event %u\n", event);
        return -1;
    }

    struct fi_cq_msg_entry comp;
    ret = fi_cq_sread(cq, &comp, 1, NULL, -1);
    if (ret != 1) {
        perror("fi_cq_sread");
        return ret;
    }

    memcpy(&keys, buff, sizeof(keys));

    run = 1;
    ret = pthread_create(&thread, NULL, cq_thread, NULL);
    if (ret) {
        perror("pthread_create");
        return ret;
    }

    printf("connected\n");

    return 0;
}

void* client_thread(void* arg) {
    struct ctx* ctx = (struct ctx*)arg;
    int i;
    ssize_t ret;
    for (i = 0; i < ctx->count; i++) {
        printf("region: %#lx\n", keys.addr);
        ret = fi_read(
            ep,
            buff,
            ctx->size,
            fi_mr_desc(mr),
            0,
            keys.addr,
            keys.rkey,
            ctx
        );
        if (ret) {
            perror("fi_read");
            break;
        }

        pthread_mutex_lock(&ctx->lock);
        while (!ctx->ready)
            pthread_cond_wait(&ctx->cond, &ctx->lock);
        ctx->ready = 0;
        pthread_mutex_unlock(&ctx->lock);
    }
}

int client(char* addr, int threads, int size, int count) {
    hints = fi_allocinfo();
    if (!hints) {
        perror("fi_allocinfo");
        return -1;
    }

    hints->addr_format = FI_SOCKADDR_IN;
    hints->ep_attr->type = FI_EP_MSG;
    hints->domain_attr->mr_mode = FI_MR_BASIC;
    hints->caps = FI_MSG | FI_RMA;
    hints->mode = FI_MR_LOCAL | FI_CONTEXT
        | FI_RX_CQ_DATA; //  | FI_MR_ENDPOINT | FI_MR_ALLOCATED | FI_MR_PROV_KEY | FI_MR_VIRT_ADDR | FI_MR_RAW;

    int ret = common_init(addr, 0);
    if (ret)
        return ret;

    ret = client_init();
    if (ret)
        return ret;

    int i;
    for (i = 0; i < threads; i++) {
        ret = pthread_create(&ctx[i].thread, NULL, client_thread, &ctx[i]);
        if (ret) {
            perror("pthread_create");
            return ret;
        }
    }

    for (i = 0; i < threads; i++) {
        pthread_join(ctx[i].thread, NULL);
    }

    run = 0;
    pthread_join(thread, NULL);

    fi_shutdown(ep, 0);
    fi_close(&ep->fid);
    fi_close(&mr->fid);
    fi_close(&cq->fid);
    fi_close(&eq->fid);
    fi_close(&domain->fid);
    fi_close(&fabric->fid);
    fi_freeinfo(hints);
    fi_freeinfo(fi);
}

int main(int argc, char* argv[]) {
    buff = malloc(buff_size);
    if (!buff) {
        perror("malloc");
        return -1;
    }

    if (argc == 1) {
        keys.addr = (uint64_t)buff;
        return server();
    }

    // if (argc != 5) {
    // 	fprintf(stderr, "usage: %s addr threads size count rkey addr\n", argv[0]);
    // 	return -1;
    // }

    char* addr = argv[1];
    int threads = 1; // atoi(argv[2]);
    int size = 1; // atoi(argv[3]);
    int count = 1; // atoi(argv[4]);

    ctx = (struct ctx*)calloc(threads, sizeof(*ctx));
    if (!ctx) {
        perror("calloc");
        return -1;
    }

    int i;
    for (i = 0; i < threads; i++) {
        pthread_mutex_init(&ctx[i].lock, NULL);
        pthread_cond_init(&ctx[i].cond, NULL);
        ctx[i].count = count;
        ctx[i].size = size;
    }

    return client(addr, threads, size, count);
}
