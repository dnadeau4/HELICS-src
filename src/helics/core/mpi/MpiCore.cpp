/*

Copyright (C) 2017-2018, Battelle Memorial Institute
All rights reserved.

This software was co-developed by Pacific Northwest National Laboratory, operated by the Battelle Memorial
Institute; the National Renewable Energy Laboratory, operated by the Alliance for Sustainable Energy, LLC; and the
Lawrence Livermore National Laboratory, operated by Lawrence Livermore National Security, LLC.

*/
#include "MpiCore.h"
#include "MpiComms.h"

#include <mpi.h>

namespace helics
{
MpiCore::MpiCore () noexcept {}

MpiCore::~MpiCore () = default;

MpiCore::MpiCore (const std::string &core_name) : CommsBroker (core_name) {}

void MpiCore::initializeFromArgs (int argc, const char *const *argv)
{
    if (brokerState == created)
    {
        // TODO parse args for rank of broker

        CommonCore::initializeFromArgs (argc, argv);
    }
}

bool MpiCore::brokerConnect ()
{
    std::lock_guard<std::mutex> lock (dataMutex);
    if (brokerRank == -1)  // cores require a broker
    {
        brokerRank = 0;
    }
    comms = std::make_unique<MpiComms> (brokerRank);
    comms->setCallback ([this](ActionMessage M) { addActionMessage (std::move (M)); });
    comms->setName (getIdentifier ());

    auto res = comms->connect ();
    return res;
}

std::string MpiCore::getAddress () const
{
    return MpiComms::getAddress();
}

}  // namespace helics
