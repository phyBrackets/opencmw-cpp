#include <fmt/format.h>

#include <catch2/catch.hpp>
#include <charconv>
#include <cstdlib>
#include <majordomo/Broker.hpp>
#include <majordomo/Constants.hpp>
#include <majordomo/Dns.hpp>
#include <majordomo/MockClient.hpp>
#include <majordomo/Utils.hpp>
#include <majordomo/Worker.hpp>
#include <thread>

#include "helpers.hpp"

using namespace opencmw::majordomo;
using namespace opencmw::DNS;
using namespace std::chrono_literals;
using opencmw::majordomo::Worker;
using URI = opencmw::URI<>;

TEST_CASE("Test dns", "DNS") {
    auto settings              = testSettings();
    settings.heartbeatInterval = std::chrono::seconds(1);

    const auto brokerAddress   = opencmw::URI<opencmw::STRICT>("inproc://testbroker");
    Broker     broker("testbroker", settings);
    REQUIRE(broker.bind(brokerAddress, BindOption::Router));
    Dns<"DnsService">     dnsWorker(broker);
    Dns<"AnotherService"> worker(broker);
    dnsWorker.registerWithDns(opencmw::URI<>("inproc://port1"));
    dnsWorker.registerWithDns(opencmw::URI<>("inproc://port1:1"));
    worker.registerWithDns(opencmw::URI<>("inproc://port2"));

    RunInThread dnsWorkerRun(dnsWorker);
    RunInThread workerRun(worker);
    RunInThread brokerRun(broker);
    REQUIRE(waitUntilServiceAvailable(broker.context, "DnsService"));
    REQUIRE(waitUntilServiceAvailable(broker.context, "AnotherService"));
    TestNode<MdpMessage> client(broker.context);
    REQUIRE(client.connect(opencmw::majordomo::INTERNAL_ADDRESS_BROKER));
     
    // Register with the Service Name

    {
      using opencmw::majordomo::Command;
        auto request = MdpMessage::createClientMessage(Command::Set);
        request.setServiceName("DnsService", static_tag);

        request.setBody("{ \"uris\": [\"inproc://ip1\", \"inproc://ip2\"], \"serviceName\": \"NewDnsService\", \"signalNames\": [\"A\", \"B\"]}", static_tag);
        client.send(request);
        
         const auto reply = client.tryReadOne();
        REQUIRE(reply.has_value());
        REQUIRE(reply->isValid());
        REQUIRE(reply->command() == Command::Final);
        REQUIRE(reply->serviceName() == "DnsService");
       std::cout << reply->body();

    }

    // Register with the Service Name

    {
        using opencmw::majordomo::Command;
        auto request = MdpMessage::createClientMessage(Command::Set);
        request.setServiceName("DnsService", static_tag);

        request.setBody("{ \"uris\": [\"inproc://ip1\", \"inproc://ip2\"], \"serviceName\": \"NewDnsService\", \"signalNames\": [\"A\", \"B\"]}", static_tag);
        client.send(request);

        const auto reply = client.tryReadOne();
        REQUIRE(reply.has_value());
        REQUIRE(reply->isValid());
        REQUIRE(reply->command() == Command::Final);
        REQUIRE(reply->serviceName() == "DnsService");
        std::cout << reply->body();
    }

    {
        using opencmw::majordomo::Command;
        auto request = MdpMessage::createClientMessage(Command::Get);
       request.setServiceName("AnotherService", static_tag);

        request.setBody("{ \"serviceName\": \"AnotherService\" }", static_tag);
        client.send(request);

        const auto reply = client.tryReadOne();
        REQUIRE(reply.has_value());
        REQUIRE(reply->isValid());
        REQUIRE(reply->command() == Command::Final);
        REQUIRE(reply->serviceName() == "AnotherService");
        REQUIRE(reply->body() == "{\n\"uris\": [\"inproc://port2/AnotherService\"]\n}");
    }

    // Request with the Broker Name
    {
        using opencmw::majordomo::Command;
        auto request = MdpMessage::createClientMessage(Command::Get);

        request.setServiceName("DnsService", static_tag);
        request.setBody("{ \"brokerName\": \"testbroker\" }", static_tag);
        client.send(request);

        const auto reply = client.tryReadOne();
        REQUIRE(reply.has_value());
        REQUIRE(reply->isValid());
        REQUIRE(reply->command() == Command::Final);
        REQUIRE(reply->body() == "{\n\"uris\": [\"inproc://ip1/NewDnsService\", \"inproc://ip2/NewDnsService\", \"inproc://port1/DnsService\", \"inproc://port1:1/DnsService\", \"inproc://port2/AnotherService\"]\n}");
    }

    // Request With the Signal Name
    {
        using opencmw::majordomo::Command;
        auto request = MdpMessage::createClientMessage(Command::Get);
        request.setServiceName("DnsService", static_tag);

        request.setBody("{ \"serviceName\": \"NewDnsService\", \"signalName\": \"A\" }", static_tag);
        client.send(request);

        const auto reply = client.tryReadOne();
        REQUIRE(reply.has_value());
        REQUIRE(reply->isValid());
        REQUIRE(reply->command() == Command::Final);
        REQUIRE(reply->serviceName() == "DnsService");
        REQUIRE(reply->body() == "{\n\"uris\": [\"inproc://ip1/NewDnsService?signal_name=A\", \"inproc://ip2/NewDnsService?signal_name=A\"]\n}");
    }
}