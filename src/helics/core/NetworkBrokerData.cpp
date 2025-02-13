/*
Copyright © 2017-2019,
Battelle Memorial Institute; Lawrence Livermore National Security, LLC; Alliance for Sustainable Energy, LLC.  See
the top-level NOTICE for additional details. All rights reserved.
SPDX-License-Identifier: BSD-3-Clause
*/

#include "NetworkBrokerData.hpp"
#include "BrokerFactory.hpp"
#include "helicsCLI11.hpp"

#include "../common/AsioContextManager.h"
#include <asio/ip/host_name.hpp>
#include <asio/ip/tcp.hpp>

#include <iostream>

using namespace std::string_literals;

namespace helics
{
std::shared_ptr<helicsCLI11App> NetworkBrokerData::commandLineParser (const std::string &localAddress)
{
    auto nbparser = std::make_shared<helicsCLI11App> (
      "Network connection information \n(arguments allow '_' characters in the names and ignore them)");
    nbparser->option_defaults ()->ignore_underscore ();
    nbparser
      ->add_flag ("--local{0},--ipv4{4},--ipv6{6},--all{10},--external{10}", interfaceNetwork,
                  "specify external interface to use, default is --local")
      ->disable_flag_override ();
    nbparser->add_option_function<std::string> ("--brokeraddress",
                                                [this, localAddress](const std::string &addr) {
                                                    auto brkprt = extractInterfaceandPort (addr);
                                                    brokerAddress = brkprt.first;
                                                    brokerPort = brkprt.second;
                                                    checkAndUpdateBrokerAddress (localAddress);
                                                },
                                                "location of the broker i.e network address");
    nbparser->add_flag ("--reuse_address", reuse_address,
                        "allow the server to reuse a bound address, mostly useful for tcp cores");
    nbparser->add_option_function<std::string> (
      "--broker",
      [this, localAddress](std::string addr) {
          auto brkr = BrokerFactory::findBroker (addr);
          if (brkr)
          {
              addr = brkr->getAddress ();
          }
          if (brokerAddress.empty ())
          {
              auto brkprt = extractInterfaceandPort (addr);
              brokerAddress = brkprt.first;
              brokerPort = brkprt.second;
              checkAndUpdateBrokerAddress (localAddress);
          }
          else
          {
              brokerName = addr;
          }
      },
      "identifier for the broker, this is either the name or network address use --broker_address or --brokername "
      "to explicitly set the network address or name the search for the broker is first by name");
    nbparser->add_option ("--brokername", brokerName, "the name of the broker");
    nbparser->add_option ("--maxsize", maxMessageSize, "The message buffer size")
      ->capture_default_str ()
      ->check (CLI::PositiveNumber);
    nbparser->add_option ("--maxcount", maxMessageCount, "The maximum number of message to have in a queue")
      ->capture_default_str ()
      ->check (CLI::PositiveNumber);
    nbparser->add_option ("--networkretries", maxRetries, "the maximum number of network retries")
      ->capture_default_str ();
    nbparser->add_flag ("--osport,--use_os_port", use_os_port,
                        "specify that the ports should be allocated by the host operating system");
    nbparser->add_flag ("--autobroker", autobroker,
                        "allow a broker to be automatically created if one is not available");
    nbparser->add_option ("--brokerinit", brokerInitString, "the initialization string for the broker");
    nbparser
      ->add_flag_function ("--client{0},--server{1}",
                           [this](int64_t val) {
                               switch (server_mode)
                               {
                               case server_mode_options::unspecified:
                               case server_mode_options::server_default_active:
                               case server_mode_options::server_default_deactivated:
                                   server_mode = (val > 0) ? server_mode_options::server_active :
                                                             server_mode_options::server_deactivated;
                                   break;
                               default:
                                   break;
                               }
                           },
                           "specify that the network connection should be a server or client")
      ->disable_flag_override ();
    nbparser->add_option_function<std::string> ("--interface,--localinterface",
                                                [this](const std::string &addr) {
                                                    auto localprt = extractInterfaceandPort (addr);
                                                    localInterface = localprt.first;
                                                    // this may get overridden later
                                                    portNumber = localprt.second;
                                                },
                                                "the local interface to use for the receive ports");
    nbparser->add_option ("--port,-p", portNumber, "port number to use")
      ->transform (CLI::Transformer ({{"auto", "-1"}}, CLI::ignore_case));
    nbparser->add_option ("--brokerport", brokerPort, "The port number to use to connect with the broker");
    nbparser
      ->add_option_function<int> ("--localport",
                                  [this](int port) {
                                      if (port == -999)
                                      {
                                          use_os_port = true;
                                      }
                                      else
                                      {
                                          portNumber = port;
                                      }
                                  },
                                  "port number for the local receive port")
      ->transform (CLI::Transformer ({{"auto", "-1"}, {"os", "-999"}}, CLI::ignore_case));
    nbparser->add_option ("--portstart", portStart, "starting port for automatic port definitions");

    nbparser->add_callback ([this]() {
        if ((!brokerAddress.empty ()) && (brokerPort == -1))
        {
            if ((localInterface.empty ()) && (portNumber != -1))
            {
                std::swap (brokerPort, portNumber);
            }
        }
    });

    return nbparser;
}

void NetworkBrokerData::checkAndUpdateBrokerAddress (const std::string &localAddress)
{
    switch (allowedType)
    {
    case interface_type::tcp:
        if ((brokerAddress == "tcp://*") || (brokerAddress == "*") || (brokerAddress == "tcp"))
        {  // the broker address can't use a wild card
            brokerAddress = localAddress;
        }
        break;
    case interface_type::udp:
        if ((brokerAddress == "udp://*") || (brokerAddress == "*") || (brokerAddress == "udp"))
        {  // the broker address can't use a wild card
            brokerAddress = localAddress;
        }
        break;
    case interface_type::ip:
        if ((brokerAddress == "udp://*") || (brokerAddress == "udp"))
        {  // the broker address can't use a wild card
            if (localAddress.compare (3, 3, "://") == 0)
            {
                brokerAddress = std::string ("udp://") + localAddress.substr (6);
            }
            else
            {
                brokerAddress = std::string ("udp://") + localAddress;
            }
        }
        else if ((brokerAddress == "tcp://*") || (brokerAddress == "tcp"))
        {  // the broker address can't use a wild card
            if (localAddress.compare (3, 3, "://") == 0)
            {
                brokerAddress = std::string ("tcp://") + localAddress.substr (6);
            }
            else
            {
                brokerAddress = std::string ("tcp://") + localAddress;
            }
        }
        else if (brokerAddress == "*")
        {
            brokerAddress = localAddress;
        }
        break;
    case interface_type::ipc:
    case interface_type::inproc:
        if ((brokerAddress.empty ()) && (!localAddress.empty ()))
        {
            brokerAddress = localAddress;
        }
    }
}

std::string makePortAddress (const std::string &networkInterface, int portNumber)
{
    std::string newAddress = networkInterface;
    if (portNumber >= 0)
    {
        newAddress.push_back (':');
        newAddress.append (std::to_string (portNumber));
    }
    return newAddress;
}

std::pair<std::string, int> extractInterfaceandPort (const std::string &address)
{
    std::pair<std::string, int> ret;
    auto lastColon = address.find_last_of (':');
    if (lastColon == std::string::npos)
    {
        ret = std::make_pair (address, -1);
    }
    else
    {
        try
        {
            if ((address.size () > lastColon + 1) && (address[lastColon + 1] != '/'))
            {
                auto val = std::stoi (address.substr (lastColon + 1));
                ret.first = address.substr (0, lastColon);
                ret.second = val;
            }
            else
            {
                ret = std::make_pair (address, -1);
            }
        }
        catch (const std::invalid_argument &)
        {
            ret = std::make_pair (address, -1);
        }
    }

    return ret;
}

std::pair<std::string, std::string> extractInterfaceandPortString (const std::string &address)
{
    auto lastColon = address.find_last_of (':');
    return {address.substr (0, lastColon), address.substr (lastColon + 1)};
}

std::string stripProtocol (const std::string &networkAddress)
{
    auto loc = networkAddress.find ("://");
    if (loc != std::string::npos)
    {
        return networkAddress.substr (loc + 3);
    }
    return networkAddress;
}

void removeProtocol (std::string &networkAddress)
{
    auto loc = networkAddress.find ("://");
    if (loc != std::string::npos)
    {
        networkAddress.erase (0, loc + 3);
    }
}

std::string addProtocol (const std::string &networkAddress, interface_type interfaceT)
{
    if (networkAddress.find ("://") == std::string::npos)
    {
        switch (interfaceT)
        {
        case interface_type::ip:
        case interface_type::tcp:
            return std::string ("tcp://") + networkAddress;
        case interface_type::ipc:
            return std::string ("ipc://") + networkAddress;
        case interface_type::udp:
            return std::string ("udp://") + networkAddress;
        case interface_type::inproc:
            return std::string ("inproc://") + networkAddress;
        }
    }
    return networkAddress;
}

void insertProtocol (std::string &networkAddress, interface_type interfaceT)
{
    if (networkAddress.find ("://") == std::string::npos)
    {
        switch (interfaceT)
        {
        case interface_type::ip:
        case interface_type::tcp:
            networkAddress.insert (0, "tcp://");
            break;
        case interface_type::ipc:
            networkAddress.insert (0, "ipc://");
            break;
        case interface_type::udp:
            networkAddress.insert (0, "udp://");
            break;
        case interface_type::inproc:
            networkAddress.insert (0, "inproc://");
            break;
        }
    }
}

bool isipv6 (const std::string &address)
{
    auto cntcolon = std::count (address.begin (), address.end (), ':');
    if (cntcolon > 2)
    {
        return true;
    }

    auto brkcnt = address.find_first_of ('[');
    if (brkcnt != std::string::npos)
    {
        return true;
    }
    if (address.compare (0, 2, "::") == 0)
    {
        return true;
    }
    return false;
}

template <class InputIt1, class InputIt2>
auto matchcount (InputIt1 first1, InputIt1 last1, InputIt2 first2, InputIt2 last2)
{
    int cnt = 0;
    while (first1 != last1 && first2 != last2 && *first1 == *first2)
    {
        ++first1, ++first2, ++cnt;
    }
    return cnt;
}

std::string getLocalExternalAddressV4 ()
{
    auto srv = AsioContextManager::getContextPointer ();

    asio::ip::tcp::resolver resolver (srv->getBaseContext ());
    asio::ip::tcp::resolver::query query (asio::ip::tcp::v4 (), asio::ip::host_name (), "");
    asio::ip::tcp::resolver::iterator it = resolver.resolve (query);
    asio::ip::tcp::endpoint endpoint = *it;

    return endpoint.address ().to_string ();
}

std::string getLocalExternalAddressV4 (const std::string &server)
{
    auto srv = AsioContextManager::getContextPointer ();

    asio::ip::tcp::resolver resolver (srv->getBaseContext ());

    asio::ip::tcp::resolver::query query_server (asio::ip::tcp::v4 (), server, "");
    std::error_code ec;
    asio::ip::tcp::resolver::iterator it_server = resolver.resolve (query_server, ec);
    if (ec)
    {
        return getLocalExternalAddressV4 ();
    }
    asio::ip::tcp::endpoint servep = *it_server;

    asio::ip::tcp::resolver::iterator end;

    auto sstring = (it_server == end) ? server : servep.address ().to_string ();

    asio::ip::tcp::resolver::query query (asio::ip::tcp::v4 (), asio::ip::host_name (), "");
    asio::ip::tcp::resolver::iterator it = resolver.resolve (query);
    asio::ip::tcp::endpoint endpoint = *it;
    int cnt = 0;
    std::string def = endpoint.address ().to_string ();
    cnt = matchcount (sstring.begin (), sstring.end (), def.begin (), def.end ());
    ++it;
    while (it != end)
    {
        asio::ip::tcp::endpoint ept = *it;
        std::string ndef = ept.address ().to_string ();
        auto mcnt = matchcount (sstring.begin (), sstring.end (), ndef.begin (), ndef.end ());
        if ((mcnt > cnt) && (mcnt >= 7))
        {
            def = ndef;
            cnt = mcnt;
        }
        ++it;
    }
    return def;
}

std::string getLocalExternalAddressV6 ()
{
    auto srv = AsioContextManager::getContextPointer ();

    asio::ip::tcp::resolver resolver (srv->getBaseContext ());
    asio::ip::tcp::resolver::query query (asio::ip::tcp::v6 (), asio::ip::host_name (), "");
    asio::ip::tcp::resolver::iterator it = resolver.resolve (query);
    asio::ip::tcp::endpoint endpoint = *it;

    return endpoint.address ().to_string ();
}

std::string getLocalExternalAddressV6 (const std::string &server)
{
    auto srv = AsioContextManager::getContextPointer ();

    asio::ip::tcp::resolver resolver (srv->getBaseContext ());

    asio::ip::tcp::resolver::query query_server (asio::ip::tcp::v6 (), server, "");
    asio::ip::tcp::resolver::iterator it_server = resolver.resolve (query_server);
    asio::ip::tcp::endpoint servep = *it_server;
    asio::ip::tcp::resolver::iterator end;

    auto sstring = (it_server == end) ? server : servep.address ().to_string ();

    asio::ip::tcp::resolver::query query (asio::ip::tcp::v6 (), asio::ip::host_name (), "");
    asio::ip::tcp::resolver::iterator it = resolver.resolve (query);
    asio::ip::tcp::endpoint endpoint = *it;

    if (it == end)
    {
        return std::string ();
    }
    int cnt = 0;
    std::string def = endpoint.address ().to_string ();
    cnt = matchcount (sstring.begin (), sstring.end (), def.begin (), def.end ());
    ++it;
    while (it != end)
    {
        asio::ip::tcp::endpoint ept = *it;
        std::string ndef = ept.address ().to_string ();
        auto mcnt = matchcount (sstring.begin (), sstring.end (), ndef.begin (), ndef.end ());
        if ((mcnt > cnt) && (mcnt >= 9))
        {
            def = ndef;
            cnt = mcnt;
        }
        ++it;
    }
    return def;
}

std::string getLocalExternalAddress (const std::string &server)
{
    if (isipv6 (server))
    {
        return getLocalExternalAddressV6 (server);
    }
    return getLocalExternalAddressV4 (server);
}

std::string generateMatchingInterfaceAddress (const std::string &server, interface_networks network)
{
    std::string newInterface;
    switch (network)
    {
    case interface_networks::local:
        if (server.empty ())
        {
            newInterface = "tcp://127.0.0.1";
        }
        else
        {
            newInterface = getLocalExternalAddress (server);
        }
        break;
    case interface_networks::ipv4:
        if (server.empty ())
        {
            newInterface = "tcp://*";
        }
        else
        {
            newInterface = getLocalExternalAddressV4 (server);
        }
        break;
    case interface_networks::ipv6:
        if (server.empty ())
        {
            newInterface = "tcp://*";
        }
        else
        {
            newInterface = getLocalExternalAddressV6 (server);
        }
        break;
    case interface_networks::all:
        if (server.empty ())
        {
            newInterface = "tcp://*";
        }
        else
        {
            newInterface = getLocalExternalAddress (server);
        }
        break;
    }
    return newInterface;
}

}  // namespace helics
