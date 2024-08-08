#include <cstring>
#include <functional>
#include <new>
#include <optional>
#include <string>
#include <system_error>

#include "rdma/fabric.h"
#include "rdma/fi_cm.h"
#include "rdma/fi_domain.h"
#include "rdma/fi_endpoint.h"
#include "rdma/fi_eq.h"

namespace sqk {
namespace net {
    namespace fab {

#define HAS_FID_DTOR(type)                                                     \
    ~type() {                                                                  \
        fi_close(get_fid());                                                   \
    }
#define FI_VER FI_VERSION(FI_MAJOR_VERSION, FI_MINOR_VERSION)
#define MAYBE_THROW(fn, ...)                                                   \
    do {                                                                       \
        int rc = fn(__VA_ARGS__);                                              \
        if (rc)                                                                \
            throw std::system_error(errno, std::generic_category());           \
    } while (0)

        typedef fi_wait_obj WaitObj;
        typedef fi_cq_format CompletionQueueFormat;
        typedef fi_cq_wait_cond CompletionQueueWaitCond;
        using Mode = uint64_t;
        using Caps = uint64_t;
        using Flags = uint64_t;
        using Action = uint64_t;
        using Event = uint32_t;

        template<typename T>
        concept HasFid = requires(T a) { a.get_fid(); };

        class CompletionQueueAttr {
            fi_cq_attr attr_;
            using Self = CompletionQueueAttr;
            friend class CompletionQueue;

          public:
            CompletionQueueAttr() : attr_ {} {}

            Self with_format(CompletionQueueFormat format) {
                attr_.format = format;
                return *this;
            }

            Self with_wait_obj(WaitObj obj) {
                attr_.wait_obj = obj;
                return *this;
            }

            Self with_wait_cond(CompletionQueueWaitCond cond) {
                attr_.wait_cond = cond;
                return *this;
            }
        };

        class EventQueueAttr {
            fi_eq_attr attr_;
            friend class EventQueue;

          public:
            using Self = EventQueueAttr;

            EventQueueAttr() : attr_ {} {}

            Self with_wait_obj(WaitObj wait_obj) {
                attr_.wait_obj = wait_obj;
                return *this;
            }
        };

#define REDIRECT_INNER(type, member)                                           \
    const type* operator->() const noexcept {                                  \
        return &member;                                                        \
    }
#define REDIRECT_INNER_PTR(type, member)                                       \
    type* operator->() const noexcept {                                        \
        return member;                                                         \
    }

        class DomainAttr {
            fi_domain_attr* attr_;

          public:
            DomainAttr(fi_domain_attr* attr) : attr_(attr) {}

            REDIRECT_INNER_PTR(fi_domain_attr, attr_);
        };

        class EndpointAttr {
            fi_ep_attr* attr_;

          public:
            EndpointAttr(fi_ep_attr* attr) : attr_(attr) {}

            REDIRECT_INNER_PTR(fi_ep_attr, attr_);
        };

        class FabricAttr {
            fi_fabric_attr* attr_;
            friend class Fabric;

          public:
            FabricAttr(fi_fabric_attr* attr) : attr_(attr) {}
        };

        class Info {
            fi_info* info_;
            using Self = Info;
            friend class Domain;
            friend class PassiveEndpoint;
            friend class Endpoint;

            Info(fi_info* info) : info_(info) {}

          public:
            static Info from_raw(fi_info* info) {
                return Info(info);
            }

            Info() {
                info_ = fi_allocinfo();
                if (!info_) {
                    throw std::bad_alloc();
                }
            };

            ~Info() {
                if (info_)
                    fi_freeinfo(info_);
            }

            Info(Info&) = delete;

            Info(Info&& rhs) {
                if (this == &rhs) {
                    return;
                }
                info_ = rhs.info_;
                rhs.info_ = nullptr;
            };

            Info& operator=(const Info& info) = delete;

            Info&& operator=(Info&& rhs) {
                if (this == &rhs) {
                    return std::move(*this);
                }
                info_ = rhs.info_;
                rhs.info_ = nullptr;
                return std::move(*this);
            }

            Self with_caps(Caps caps) {
                info_->caps |= caps;
                return std::move(*this);
            }

            Self with_addr_format(uint32_t addr_format) {
                info_->addr_format = addr_format;
                return std::move(*this);
            }

            Self with_ep_attr(std::function<void(EndpointAttr)> fn) {
                fn(EndpointAttr(info_->ep_attr));
                return std::move(*this);
            }

            Self with_domain_attr(std::function<void(DomainAttr)> fn) {
                fn(DomainAttr(info_->domain_attr));
                return std::move(*this);
            }

            Self with_mode(Mode mode) {
                info_->mode |= mode;
                return std::move(*this);
            }

            void print() {
                char buf[8192];
                fi_tostr_r(buf, sizeof(buf), info_, FI_TYPE_INFO);
                printf("info: %s\n", buf);
            }

            FabricAttr get_fabric_attr() {
                return FabricAttr(info_->fabric_attr);
            }

            static Info get_info(
                std::optional<std::string> node,
                std::string service,
                Flags flags,
                Info hint
            ) {
                fi_info* info;
                MAYBE_THROW(
                    fi_getinfo,
                    FI_VER,
                    node ? node.value().c_str() : nullptr,
                    service.c_str(),
                    flags,
                    hint.info_,
                    &info
                );
                return Info(info);
            }
        };

        class Fabric {
            fid_fabric* fabric_;
            friend class EventQueue;
            friend class Domain;
            friend class PassiveEndpoint;

          public:
            fid* get_fid() {
                return &fabric_->fid;
            }

            Fabric(FabricAttr attr) {
                MAYBE_THROW(fi_fabric, attr.attr_, &fabric_, nullptr);
            };
            HAS_FID_DTOR(Fabric)
        };

        class EventQueueCompleteEntry {
            fi_eq_cm_entry ent_;
            friend class EventQueue;

          public:
            REDIRECT_INNER(fi_eq_cm_entry, ent_);
        };

        class CompletionQueueMsgEntry {
            fi_cq_msg_entry ent_;
            friend class CompletionQueue;

          public:
            REDIRECT_INNER(fi_cq_msg_entry, ent_);
        };

        class EventQueue {
            fid_eq* eq;

          public:
            fid* get_fid() {
                return &eq->fid;
            }

            EventQueue(Fabric& fabric, EventQueueAttr attr) {
                MAYBE_THROW(
                    fi_eq_open,
                    fabric.fabric_,
                    &attr.attr_,
                    &eq,
                    nullptr
                );
            }
            HAS_FID_DTOR(EventQueue)

            int sread(
                Event& event,
                EventQueueCompleteEntry& ent,
                int timeout,
                Flags flags
            ) {
                return fi_eq_sread(
                    eq,
                    &event,
                    &ent.ent_,
                    sizeof(ent.ent_),
                    timeout,
                    flags
                );
            }

            int read(Event& event, EventQueueCompleteEntry& ent, Flags flags) {
                return fi_eq_read(
                    eq,
                    &event,
                    &ent.ent_,
                    sizeof(ent.ent_),
                    flags
                );
            }
        };

        class Domain {
            fid_domain* domain_;
            friend class CompletionQueue;
            friend class MemoryRegion;
            friend class Endpoint;

          public:
            fid* get_fid() {
                return &domain_->fid;
            }

            Domain(Fabric& fabric, Info& info) {
                MAYBE_THROW(
                    fi_domain,
                    fabric.fabric_,
                    info.info_,
                    &domain_,
                    nullptr
                );
            }

            HAS_FID_DTOR(Domain);
        };

        class CompletionQueue {
            fid_cq* cq;

          public:
            fid* get_fid() {
                return &cq->fid;
            }

            CompletionQueue(Domain& domain, CompletionQueueAttr& attr) {
                MAYBE_THROW(
                    fi_cq_open,
                    domain.domain_,
                    &attr.attr_,
                    &cq,
                    nullptr
                );
            }
            HAS_FID_DTOR(CompletionQueue)

            int sread(CompletionQueueMsgEntry& ent, int timeout) {
                return fi_cq_sread(cq, &ent, 1, nullptr, timeout);
            }

            int read(CompletionQueueMsgEntry& ent) {
                return fi_cq_read(cq, &ent, 1);
            }
        };

        class MemoryBuffer {
            friend class MemoryRegion;
            friend class Endpoint;

          public:
            size_t capicity_;
            void* buf_;

            MemoryBuffer(size_t size) : capicity_(size) {
                buf_ = malloc(size);
                if (!buf_) {
                    throw std::bad_alloc();
                }
            }

            ~MemoryBuffer() {
                free(buf_);
            }
        };

        class MemoryRegion {
            fid_mr* mr_;

          public:
            fid* get_fid() {
                return &mr_->fid;
            }

            MemoryRegion(Domain& domain, MemoryBuffer& buf, Action acts) {
                MAYBE_THROW(
                    fi_mr_reg,
                    domain.domain_,
                    buf.buf_,
                    buf.capicity_,
                    acts,
                    0,
                    0,
                    0,
                    &mr_,
                    nullptr
                );
            }

            HAS_FID_DTOR(MemoryRegion)

            void* desc() {
                return fi_mr_desc(mr_);
            }

            uint64_t key() {
                return fi_mr_key(mr_);
            }
        };

        class PassiveEndpoint {
            fid_pep* pep_;

          public:
            fid* get_fid() {
                return &pep_->fid;
            }

            PassiveEndpoint(Fabric& fabric, Info& info) {
                MAYBE_THROW(
                    fi_passive_ep,
                    fabric.fabric_,
                    info.info_,
                    &pep_,
                    nullptr
                );
            }

            HAS_FID_DTOR(PassiveEndpoint)

            template<HasFid T>
            void bind(T& e) {
                MAYBE_THROW(fi_pep_bind, pep_, e.get_fid(), 0);
            }

            void listen() {
                MAYBE_THROW(fi_listen, pep_);
            }
        };

        class Endpoint {
            fid_ep* ep_;

            fid* get_fid() {
                return &ep_->fid;
            }

          public:
            Endpoint(Domain& domain, Info& info) {
                MAYBE_THROW(
                    fi_endpoint,
                    domain.domain_,
                    info.info_,
                    &ep_,
                    NULL
                );
            }

            template<HasFid T>
            void bind(T& e, Flags flags) {
                MAYBE_THROW(fi_ep_bind, ep_, e.get_fid(), flags);
            }

            void accept() {
                MAYBE_THROW(fi_accept, ep_, nullptr, 0);
            }

            template<typename T>
            void send(MemoryBuffer& buf, size_t size, MemoryRegion& mr, T ctx) {
                MAYBE_THROW(fi_send, ep_, buf.buf_, size, mr.desc(), 0, ctx);
            }

            void close() {
                MAYBE_THROW(fi_close, &ep_->fid);
            }

            ~Endpoint() {
                fi_shutdown(ep_, 0);
                fi_close(get_fid());
            }
        };
    } // namespace fab
} // namespace net
} // namespace sqk
