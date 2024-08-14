#include <cstring>
#include <functional>
#include <new>
#include <optional>
#include <string>
#include <system_error>
#include <unordered_set>

#include "core.hpp"
#include "rdma/fabric.h"
#include "rdma/fi_cm.h"
#include "rdma/fi_domain.h"
#include "rdma/fi_endpoint.h"
#include "rdma/fi_eq.h"
#include "rdma/fi_rma.h"

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
        using OpaqueAddr = void*;

        template<typename T>
        concept HasFid = requires(T a) { a.get_fid(); };

        struct IOVector {
            char* buf_;
            size_t size_;

            IOVector(size_t size) : size_(size) {
                buf_ = new char[size_];
            }

            ~IOVector() {
                delete [] buf_;
            }
        };
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
    type* operator->() noexcept {                                              \
        return &member;                                                        \
    }                                                                          \
    operator type*() noexcept {                                                \
        return &member;                                                        \
    }
#define REDIRECT_INNER_PTR(type, member)                                       \
    type* operator->() const noexcept {                                        \
        return member;                                                         \
    }                                                                          \
    operator type*() noexcept {                                                \
        return member;                                                         \
    }

        class DomainAttr {
            fi_domain_attr* attr_;

          public:
            DomainAttr(fi_domain_attr* attr) : attr_(attr) {}

            REDIRECT_INNER_PTR(fi_domain_attr, attr_);
        };

        class RxAttr {
            fi_rx_attr* attr_;

          public:
            RxAttr(fi_rx_attr* attr) : attr_(attr) {}

            REDIRECT_INNER_PTR(fi_rx_attr, attr_);
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
            REDIRECT_INNER_PTR(fi_fabric_attr, attr_);
            FabricAttr(fi_fabric_attr* attr) : attr_(attr) {}
        };

        class AddressVectorAttr {
            fi_av_attr attr_;

          public:
            REDIRECT_INNER(fi_av_attr, attr_);
        };

class Address {
    fi_addr_t addr_;
public:
    operator fi_addr_t () {
        return addr_;
    }
    fi_addr_t* operator()() {
        return &addr_;
    }
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

            OpaqueAddr dst_addr() {
                return info_->dest_addr;
            }

            OpaqueAddr src_addr() {
                return info_->src_addr;
            }
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

            Self with_rx_attr(std::function<void(RxAttr)> fn) {
                fn(RxAttr(info_->rx_attr));
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
                S_DBUG("info: {}", buf);
            }

            Self with_fabric_attr(std::function<void(FabricAttr)> fn) {
                fn(FabricAttr(info_->fabric_attr));
                return std::move(*this);
            }

            FabricAttr get_fabric_attr() {
                return FabricAttr(info_->fabric_attr);
            }

            DomainAttr get_domain_attr() {
                return DomainAttr(info_->domain_attr);
            }

            static Info get_info(
                std::optional<std::string> node,
                std::optional<std::string> service,
                Flags flags,
                Info& hint
            ) {
                fi_info* info;
                MAYBE_THROW(
                    fi_getinfo,
                    FI_VER,
                    node ? node.value().c_str() : nullptr,
                    service ? service.value().c_str() : nullptr,
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
            // FIME: coroutine do not support dyn arr on gcc11.4
            fi_eq_cm_entry* ent_;
            friend class EventQueue;

          public:
            EventQueueCompleteEntry() {
                ent_ = new fi_eq_cm_entry;
            }

            EventQueueCompleteEntry(const EventQueueCompleteEntry&) = delete;

            ~EventQueueCompleteEntry() {
                delete ent_;
            }

            EventQueueCompleteEntry&
            operator=(const EventQueueCompleteEntry&) = delete;
            REDIRECT_INNER_PTR(fi_eq_cm_entry, ent_);
        };

        struct CompletionQueueErrEntry {
            fi_cq_err_entry ent_;
            REDIRECT_INNER(fi_cq_err_entry, ent_);
        };

        class CompletionQueueMsgEntry {
            fi_cq_msg_entry ent_;
            Address addr_;
            friend class CompletionQueue;

          public:
        Address addr() { return addr_; }
            REDIRECT_INNER(fi_cq_msg_entry, ent_);
        };
        class PassiveEndpoint;

        class EventQueue {
            fid_eq* eq_;
            std::unordered_map<void*, Awaker<void>*> map_;
            friend class Endpoint;
            friend class PassiveEndpoint;
            std::unordered_map<fid*, void*> peps_;
            template<HasFid T>
            void add_ep(T* ep);
            template<HasFid T>
            void del_ep(T* ep);

          public:
            fid* get_fid() {
                return &eq_->fid;
            }

            EventQueue(Fabric& fabric, EventQueueAttr attr) : map_ {} {
                MAYBE_THROW(
                    fi_eq_open,
                    fabric.fabric_,
                    &attr.attr_,
                    &eq_,
                    nullptr
                );
                S_DBUG("EventQueue()");
            }
            HAS_FID_DTOR(EventQueue)

            int sread(
                Event& event,
                EventQueueCompleteEntry& ent,
                int timeout,
                Flags flags
            ) {
                return fi_eq_sread(
                    eq_,
                    &event,
                    ent.ent_,
                    sizeof(*ent.ent_),
                    timeout,
                    flags
                );
            }

            int poll(Event& event, Flags flags);
        };

        class Domain {
            friend class CompletionQueue;
            friend class MemoryRegion;
            friend class Endpoint;

          public:
            fid_domain* domain_;
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

        int poll() {
            CompletionQueueMsgEntry ent {};
            int rc = fi_cq_readfrom(cq, &ent, 1, ent.addr_());
            if (rc == -EAGAIN) {
                return 0;
            }
            if (rc == -FI_EAVAIL) {
                CompletionQueueErrEntry err_ent {};
                rc = fi_cq_readerr(cq, &err_ent.ent_, 0);
                auto err_data = std::vector<char>(1024);
                fi_cq_strerror(
                    cq,
                    err_ent->prov_errno,
                    err_ent->err_data,
                    err_data.data(),
                    1024
                );
                auto err_string =
                    std::string_view(err_data.begin(), err_data.end());
                S_ERROR(
                    "cq_poll error: {}, prov_errno: {}, prov_error: {}",
                    err_ent->err,
                    err_ent->prov_errno,
                    err_string
                );
                memcpy(&ent, &err_ent, sizeof(ent));
            }
            if (rc == 1 && ent->flags & FI_RECV) {
                S_DBUG("receive FI_RECV with addr: {}", fmt::ptr(ent.addr_()));
                auto awaker = static_cast<Awaker<Address>*>(ent->op_context);
                awaker->wake(ent.addr());
            } else if (rc == 1 && ent->flags & (FI_SEND | FI_WRITE)) {
                auto awaker = static_cast<Awaker<void>*>(ent->op_context);
                awaker->wake();
            } else if (rc == 1) {
                S_ERROR("unexpected event={}", ent->flags);
            } else {
                S_WARN("fi_cq_readfrom: rc={}", rc);
            }
            return rc;
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
            EventQueue& eq_;

            friend class EventQueue;
            Awaker<Info> awaker_;

          public:
            fid* get_fid() {
                return &pep_->fid;
            }

            PassiveEndpoint(Fabric& fabric, Info& info, EventQueue& eq) :
                eq_(eq) {
                MAYBE_THROW(
                    fi_passive_ep,
                    fabric.fabric_,
                    info.info_,
                    &pep_,
                    nullptr
                );
                bind(eq_);
                eq_.add_ep(this);
            }

            ~PassiveEndpoint() {
                eq_.del_ep(this);
                fi_close(get_fid());
            }

            sqk::Task<Info> next() {
                S_DBUG("before waiter {}", fmt::ptr(&awaker_));
                Info info = co_await awaker_;
                S_DBUG(
                    "after waiter {}, ret: {}",
                    fmt::ptr(&awaker_),
                    fmt::ptr(info.info_)
                );
                co_return std::move(info);
            }

            template<HasFid T>
            void bind(T& e) {
                MAYBE_THROW(fi_pep_bind, pep_, e.get_fid(), 0);
            }

            void listen() {
                MAYBE_THROW(fi_listen, pep_);
            }
        };

        template<HasFid T>
        inline void EventQueue::add_ep(T* ep) {
            this->peps_.emplace(ep->get_fid(), ep);
        }

        template<HasFid T>
        inline void EventQueue::del_ep(T* ep) {
            this->peps_.erase(ep->get_fid());
        }

        class AddressVector {
            fid_av* av_;

          public:
            fid* get_fid() {
                return &av_->fid;
            }

            REDIRECT_INNER_PTR(fid_av, av_);

            AddressVector(Domain& domain, AddressVectorAttr& attr) {
                MAYBE_THROW(fi_av_open, domain.domain_, attr, &av_, nullptr);
            }

            Address insert(void* opaque_addr) {
                Address addr;
                int rc = fi_av_insert(av_, opaque_addr, 1, addr(), 0, addr());
                assert(rc == 1);
                return addr;
            }
        };

        class Endpoint {
            fid_ep* ep_;
            EventQueue& eq_;
            Awaker<void> stop_waker_;

            fid* get_fid() {
                return &ep_->fid;
            }

            friend class EventQueue;

          public:
            Endpoint(
                Domain& domain,
                Info& info,
                EventQueue& eq,
                CompletionQueue& cq,
                std::optional<AddressVector> av = std::nullopt
            ) :
                eq_(eq) {
                MAYBE_THROW(
                    fi_endpoint,
                    domain.domain_,
                    info.info_,
                    &ep_,
                    NULL
                );
                bind(eq, 0);
                eq.add_ep(this);
                bind(cq, FI_TRANSMIT | FI_RECV);
                if (av) {
                    bind(av.value(), 0);
                }
                MAYBE_THROW(fi_enable, ep_);
            }

            template<HasFid T>
            void bind(T& e, Flags flags) {
                MAYBE_THROW(fi_ep_bind, ep_, e.get_fid(), flags);
            }

            Task<void> wait_disconn() {
                co_await stop_waker_;
            }

            void get_name(IOVector& iov) {
                MAYBE_THROW(fi_getname, get_fid(), iov.buf_, &iov.size_);
            }

            sqk::Task<void> accept() {
                MAYBE_THROW(fi_accept, ep_, nullptr, 0);
                Awaker<void> awaker;
                eq_.map_.emplace(get_fid(), &awaker);
                S_DBUG(
                    "emplace: {}->{}",
                    fmt::ptr(get_fid()),
                    fmt::ptr(&awaker)
                );
                co_await awaker;
                S_DBUG("awaker done");
            }

            sqk::Task<void> send(
                MemoryBuffer& buf,
                size_t size,
                MemoryRegion& mr,
                std::optional<Address> dst = std::nullopt
            ) {
                sqk::Awaker<void> waker;
                int rc;
                for (;;) {
                    rc = fi_send(
                        ep_,
                        buf.buf_,
                        size,
                        mr.desc(),
                        dst ? dst.value() : 0,
                        &waker
                    );
                    if (rc != -EAGAIN) {
                        break;
                    }
                }
                if (rc) {
                    throw std::system_error(-rc, std::system_category());
                }
                co_await waker;
            }

            sqk::Task<Address>
            recv(MemoryBuffer& buf, size_t size, MemoryRegion& mr) {
                sqk::Awaker<Address> waker;
                S_DBUG("fi_recv: {}", fmt::ptr(&waker));
                MAYBE_THROW(fi_recv, ep_, buf.buf_, size, mr.desc(), 0, &waker);
                Address addr = co_await waker;
                co_return addr;
            }

            sqk::Task<void> write(
                MemoryBuffer& buf,
                size_t size,
                MemoryRegion& mr,
                uint64_t addr,
                uint64_t key,
                Address dst
            ) {
                sqk::Awaker<Address> waker;
                MAYBE_THROW(
                    fi_write,
                    ep_,
                    buf.buf_,
                    size,
                    mr.desc(),
                    dst,
                    addr,
                    key,
                    &waker
                );
                co_await waker;
            }

            sqk::Task<void> read(
                MemoryBuffer& buf,
                size_t size,
                MemoryRegion& mr,
                int addr,
                uint64_t key,
                Address src
            ) {
                sqk::Awaker<void> waker;
                fi_read(
                    ep_,
                    buf.buf_,
                    size,
                    mr.desc(),
                    addr,
                    addr,
                    key,
                    &waker
                );
                co_await waker;
            }

            void close() {
                MAYBE_THROW(fi_close, &ep_->fid);
            }

            ~Endpoint() {
                eq_.del_ep(this);
                fi_shutdown(ep_, 0);
                fi_close(get_fid());
            }
        };

        inline int EventQueue::poll(Event& event, Flags flags) {
            EventQueueCompleteEntry ent;
            int ret =
                fi_eq_read(eq_, &event, ent.ent_, sizeof(*ent.ent_), flags);
            if (ret > 0) {
                if (ret != sizeof(*ent.ent_)) {
                    S_WARN("eq::read ret: {}", ret);
                }
                S_DBUG(
                    "find awaker, fid: {} eq: {}",
                    fmt::ptr(ent->fid),
                    fmt::ptr(eq_)
                );
                if (event == FI_CONNREQ) {
                    auto iter = peps_.find(ent->fid);
                    if (iter == peps_.end()) {
                        S_WARN("drop connreq: fid={}", fmt::ptr(ent->fid));
                    }
                    auto& awaker =
                        static_cast<PassiveEndpoint*>(iter->second)->awaker_;
                    S_DBUG(
                        "FI_CONNREQ wakeup : {}, fid={}",
                        fmt::ptr(&awaker),
                        fmt::ptr(ent->info)
                    );
                    awaker.wake(Info::from_raw(ent->info));
                    return ret;
                }
                if (event == FI_SHUTDOWN) {
                    auto iter = peps_.find(ent->fid);
                    if (iter == peps_.end()) {
                        S_WARN("drop connreq: fid={}", fmt::ptr(ent->fid));
                    }
                    auto& awaker =
                        static_cast<Endpoint*>(iter->second)->stop_waker_;
                    S_DBUG(
                        "FI_SHUTDOWN wakeup : {}, fid={}",
                        fmt::ptr(&awaker),
                        fmt::ptr(ent->info)
                    );
                    awaker.wake();
                    return ret;
                }
                auto awaker = map_.find(ent->fid);
                if (awaker != map_.end()) {
                    S_DBUG("awaker: {}", fmt::ptr(awaker->second));
                    awaker->second->wake();
                    map_.erase(awaker);
                } else {
                    S_DBUG("no awaker, fid: {}", fmt::ptr(ent->fid));
                }
            } else if (ret != -EAGAIN) {
                S_DBUG("eq::poll: {}", ret);
            }
            return ret;
        }
    } // namespace fab
} // namespace net
} // namespace sqk
