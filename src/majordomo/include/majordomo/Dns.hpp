#ifndef OPENCMW_CPP_DNS_H
#define OPENCMW_CPP_DNS_H

#include <fmt/core.h>

#include <array>
#include <atomic>
#include <chrono>
#include <deque>
#include <iomanip>
#include <IoSerialiserJson.hpp>
#include <majordomo/Broker.hpp>
#include <majordomo/Constants.hpp>
#include <majordomo/Message.hpp>
#include <majordomo/Settings.hpp>
#include <majordomo/SubscriptionMatcher.hpp>
#include <majordomo/Utils.hpp>
#include <majordomo/Worker.hpp>
#include <majordomo/ZmqPtr.hpp>
#include <opencmw.hpp>
#include <optional>
#include <ranges>
#include <set>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <QuerySerialiser.hpp>

#include "URI.hpp"

using namespace std::string_literals;
using namespace opencmw::majordomo;
using namespace std::chrono_literals;

struct Request {
    std::string brokerName;
    std::string signalName;
    std::string serviceName;
    std::set<std::string> uris;
    std::set<std::string> signalNames;
    // std::unordered_map<std::string, std::string> meta;
};
ENABLE_REFLECTION_FOR(Request, brokerName, serviceName, signalNames, uris, signalName)

struct Reply {
    // std::set<opencmw::URI<opencmw::RELAXED>> uris;
    std::set<std::string> uris;
    // std::vector<KeyValue> meta;
    //  std::unordered_map<std::string, std::vector<std::string>> meta;
};
ENABLE_REFLECTION_FOR(Reply, uris)

struct DnsContext {
    opencmw::TimingCtx      ctx;
    std::string             signalFilter;
    opencmw::MIME::MimeType contentType = opencmw::MIME::JSON;
};
ENABLE_REFLECTION_FOR(DnsContext, signalFilter, contentType)

namespace opencmw::DNS {
using opencmw::majordomo::Broker;
using BrokerMessage = opencmw::majordomo::BasicMdpMessage<
        opencmw::majordomo::MessageFormat::WithSourceId>;

namespace detail {
struct DnsServiceInfo {
    std::set<std::string> uris;
     std::set<std::string> signalNames;
    //  std::unordered_map<std::string, std::vector<std::string>> meta;
    DnsServiceInfo() = default;
};

struct DnsServiceItem {
    std::string                                     serviceName;
    std::unordered_map<std::string, DnsServiceInfo> brokers;
    // std::chrono::time_point<std::chrono::steady_clock> expiry;
    std::time_t lastSeen;
    DnsServiceItem() = default;
    DnsServiceItem(const std::string &service)
        : serviceName(service) {}
};

} // namespace detail

/*class ServiceRegistrationMessage {
 public: 
  ServiceRegistrationMessage() = default;
  ServiceRegistrationMessage(const std::string &name, const std::set<std::string> &serviceUris, const std::set<std::string> &serviceSignals, const std::map<std::string, std::string> &serviceMeta) 
                            : serviceName(name), uris(serviceUris), signalNames(serviceSignals), meta(serviceMeta) {}
 private: 
   std::string serviceName;
  std::set<std::string> uris;
  std::set<std::string> signalNames;
  std::map<std::string, std::string> meta;

};*/
class DnsStorage {
public:
    DnsStorage() = default;

    // Get the singleton instance
    static DnsStorage &getInstance() {
        static DnsStorage instance;
        return instance;
    }

    std::set<std::string>                         _dnsAddress;
    std::map<std::string, detail::DnsServiceItem> _dnsCache;

private:
    // Prevent copy constructor and assignment operator
    DnsStorage(DnsStorage const &) = delete;
    DnsStorage &operator=(DnsStorage const &) = delete;
};

template<units::basic_fixed_string serviceName_, typename... Meta_>
class RequestServiceName {
public:
    explicit RequestServiceName(const Request &reqIn, Reply &out)
        : reqIn_(reqIn), out_(out) {
        processRequest(reqIn_, out_);
    }

private:
    DnsContext     dnsIn_;
    const Request &reqIn_;
    Reply         &out_;
    DnsStorage    &storage = DnsStorage::getInstance();
    void           processRequest(const Request &in, Reply &out) {
                  const auto iter = storage._dnsCache.find(in.serviceName);
                  if (iter == storage._dnsCache.end()) {
                      throw std::invalid_argument(
                              fmt::format("Inavlid Service Name {}", in.serviceName));
        }
                  detail::DnsServiceItem item = iter->second;
                  for (auto const &[brokerName, brokerInfo] : item.brokers) {
                 // std::copy(brokerInfo.uris.begin(), brokerInfo.end(), std::inserter(outUris, outUris.begin()));
                 out.uris.insert(brokerInfo.uris.begin(), brokerInfo.uris.end());
                      // Out.meta[serviceInfo.meta.first].push_back(serviceInfo.meta.second);
        }
    }
};

template<units::basic_fixed_string serviceName_, typename... Meta_>
class RequestBrokerName {
public:
    explicit RequestBrokerName(const Request &reqIn, Reply &out)
        : reqIn_(reqIn), out_(out) {
        processRequest(reqIn_, out_);
    }

private:
    DnsContext     dnsIn_;
    const Request &reqIn_;
    Reply         &out_;
    DnsStorage    &storage = DnsStorage::getInstance();

    void           processRequest(const Request &in, Reply &out) {
                  for (auto &item : storage._dnsCache) {
                      if (item.second.brokers.find(in.brokerName) != item.second.brokers.end()) {
                          out.uris.insert(item.second.brokers[in.brokerName].uris.begin(), item.second.brokers[in.brokerName].uris.end());
            }
        }
    }
};

template<units::basic_fixed_string serviceName_, typename... Meta_>
class RequestSignalName {
public:
    explicit RequestSignalName(const Request &reqIn, Reply &out)
        : reqIn_(reqIn), out_(out) {
        processRequest(reqIn_, out_);
    }

private:
    DnsContext     dnsIn_;
    const Request &reqIn_;
    Reply         &out_;
    DnsStorage    &storage = DnsStorage::getInstance();

    void           processRequest(const Request &in, Reply &out) {
                     
                for(auto& [serviceName, service]: storage._dnsCache) {
    for(auto& [brokerName, broker]: service.brokers) {
        if(broker.signalNames.find(in.signalName) != broker.signalNames.end()) {
            out.uris.insert(broker.uris.begin(), broker.uris.end());
            std::string signalName = in.signalName;
            /*std::transform(out.uris.begin(), out.uris.end(), out.uris.begin(),
                [signalName](const std::string &uri) {
                    return uri + "?" + "signal_name" + "=" + signalName;
                });*/ 
            std::vector<std::string> tempContainer(out.uris.begin(), out.uris.end());
            std::transform(tempContainer.begin(), tempContainer.end(), tempContainer.begin(),
                [signalName](const std::string &uri) {
                    return uri + "?" + "signal_name" + "=" + signalName;
                });
            out.uris.insert(tempContainer.begin(), tempContainer.end());
        }
    }
                // Out.meta[broker->second.services[In.serviceName].meta.first].push_back(broker->second.services[In.serviceName].meta.second);
                //}
            }
        }
};

template<units::basic_fixed_string serviceName_, typename... Meta_>
class Dns : public Worker<serviceName_, DnsContext, Request,
                    Reply, Meta_...> {
    using super_t = Worker<serviceName_, DnsContext, Request,
            Reply, Meta_...>;

public:
    const std::string brokerName;
    template<typename BrokerType>
    explicit Dns(const BrokerType &broker)
        : super_t(broker, {}), brokerName(std::move(broker.brokerName)) {
        super_t::setCallback([this](
                                     const RequestContext &rawCtx,
                                     const DnsContext &dnsIn, const Request &reqregIn, 
                                     DnsContext &dnsOut, Reply &out) {
            if (rawCtx.request.command() == Command::Get) {
                fmt::print("worker recieved 'get' request\n");
                handleGetRequest(dnsIn, reqregIn, out);
                //fmt::print("{}, {}", In.brokerName, In.serviceName);
            } else if (rawCtx.request.command() == Command::Set) {
                fmt::print("worker received 'set' request\n");
                 handleSetRequest(reqregIn, dnsOut, out);
            }
        });
    }

    void registerWithDns(const opencmw::URI<> &address) {
        fmt::print("register dns address get called");
        storage._dnsAddress.emplace(address.str());
        auto [iter, inserted]    = storage._dnsCache.try_emplace(serviceName_.c_str(), detail::DnsServiceItem());
        auto &item               = iter->second;
        item.lastSeen            = std::time(nullptr);
        std::string service_name = serviceName_.c_str();
        item.brokers.try_emplace(brokerName, detail::DnsServiceInfo());
        item.brokers[brokerName].uris.insert(address.str() + "/" + service_name);
    }


private:
    using requestBrokerName_t  = RequestBrokerName<serviceName_, Meta_...>;
    using requestSignalName_t  = RequestSignalName<serviceName_, Meta_...>;
    using requestServiceName_t = RequestServiceName<serviceName_, Meta_...>;
    DnsStorage &storage        = DnsStorage::getInstance();

    void        handleGetRequest(const DnsContext &dnsIn, const Request &requestIn,
                   Reply &out) {
               if (!requestIn.serviceName.empty() && requestIn.brokerName.empty() && requestIn.signalName.empty()) {
                   requestServiceName_t worker(requestIn, out);
        } else if (requestIn.serviceName.empty() && !requestIn.brokerName.empty() && requestIn.signalName.empty()) {
                   requestBrokerName_t worker(requestIn, out);
        } else if (!requestIn.signalName.empty() && !requestIn.serviceName.empty() && requestIn.brokerName.empty()) {
                   requestSignalName_t worker(requestIn, out);
        }
    }

  void handleSetRequest(const Request &registerIn, DnsContext &dnsOut, Reply &out) {
       // storage._dnsAddress.emplace(registerIn.uris.begin(), registerIn.uris.end());
        auto [iter, inserted] = storage._dnsCache.try_emplace(registerIn.serviceName, detail::DnsServiceItem());
        auto &item = iter->second;
        item.brokers.try_emplace(brokerName, detail::DnsServiceInfo());
        std::string serviceName = registerIn.serviceName;
       // Item.brokers[brokerName].uris.insert()
        std::transform(registerIn.uris.begin(), registerIn.uris.end(), std::inserter(item.brokers[brokerName].uris, item.brokers[brokerName].uris.begin()), 
    [serviceName](const std::string& uri) {
        return uri + "/" + serviceName;
    });
    
    item.brokers[brokerName].signalNames.insert(registerIn.signalNames.begin(), registerIn.signalNames.end());
  }
};
} // namespace opencmw::DNS
#endif