/*
Copyright © 2017-2019,
Battelle Memorial Institute; Lawrence Livermore National Security, LLC; Alliance for Sustainable Energy, LLC.  See
the top-level NOTICE for additional details. All rights reserved.
SPDX-License-Identifier: BSD-3-Clause
*/

#include "ZmqCore.h"
#include "../../common/zmqContextManager.h"
#include "../NetworkCore_impl.hpp"
#include "ZmqComms.h"
#include "ZmqCommsSS.h"

namespace helics
{
namespace zeromq
{
ZmqCore::ZmqCore () noexcept { netInfo.server_mode = NetworkBrokerData::server_mode_options::server_deactivated; }

ZmqCore::ZmqCore (const std::string &core_name) : NetworkCore (core_name)
{
    netInfo.server_mode = NetworkBrokerData::server_mode_options::server_deactivated;
}

bool ZmqCore::brokerConnect ()
{
    ZmqContextManager::startContext ();
    return NetworkCore::brokerConnect ();
}

ZmqCoreSS::ZmqCoreSS () noexcept
{
    netInfo.server_mode = NetworkBrokerData::server_mode_options::server_deactivated;
}

ZmqCoreSS::ZmqCoreSS (const std::string &core_name) : NetworkCore (core_name)
{
    netInfo.server_mode = NetworkBrokerData::server_mode_options::server_deactivated;
}

bool ZmqCoreSS::brokerConnect ()
{
    ZmqContextManager::startContext ();
    return NetworkCore::brokerConnect ();
}

}  // namespace zeromq
}  // namespace helics
