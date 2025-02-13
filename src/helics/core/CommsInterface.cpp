/*
Copyright © 2017-2019,
Battelle Memorial Institute; Lawrence Livermore National Security, LLC; Alliance for Sustainable Energy, LLC.  See
the top-level NOTICE for additional details. All rights reserved.
SPDX-License-Identifier: BSD-3-Clause
*/
#include "CommsInterface.hpp"
#include "NetworkBrokerData.hpp"
#include <iostream>

namespace helics
{
CommsInterface::CommsInterface (thread_generation threads) : singleThread (threads == thread_generation::single) {}

/** destructor*/
CommsInterface::~CommsInterface ()
{
    std::lock_guard<std::mutex> syncLock (threadSyncLock);
    if (!singleThread)
    {
        if (queue_watcher.joinable ())
        {
            queue_watcher.join ();
        }
    }
    if (queue_transmitter.joinable ())
    {
        queue_transmitter.join ();
    }
}

void CommsInterface::loadNetworkInfo (const NetworkBrokerData &netInfo)
{
    if (propertyLock ())
    {
        localTargetAddress = netInfo.localInterface;
        brokerTargetAddress = netInfo.brokerAddress;
        brokerName = netInfo.brokerName;
        interfaceNetwork = netInfo.interfaceNetwork;
        maxMessageSize = netInfo.maxMessageSize;
        maxMessageCount = netInfo.maxMessageCount;
        autoBroker = netInfo.autobroker;
        switch (netInfo.server_mode)
        {
        case NetworkBrokerData::server_mode_options::server_active:
        case NetworkBrokerData::server_mode_options::server_default_active:
            serverMode = true;
            break;
        case NetworkBrokerData::server_mode_options::server_deactivated:
        case NetworkBrokerData::server_mode_options::server_default_deactivated:
            serverMode = false;
            break;
        case NetworkBrokerData::server_mode_options::unspecified:
            break;
        }
        propertyUnLock ();
    }
}

void CommsInterface::loadTargetInfo (const std::string &localTarget,
                                     const std::string &brokerTarget,
                                     interface_networks targetNetwork)
{
    if (propertyLock ())
    {
        localTargetAddress = localTarget;
        brokerTargetAddress = brokerTarget;
        interfaceNetwork = targetNetwork;
        propertyUnLock ();
    }
}

bool CommsInterface::propertyLock ()
{
    bool exp = false;
    while (!operating.compare_exchange_weak (exp, true))
    {
        if (tx_status != connection_status::startup)
        {
            return false;
        }
    }
    return true;
}

void CommsInterface::propertyUnLock ()
{
    bool exp = true;
    operating.compare_exchange_strong (exp, false);
}

void CommsInterface::transmit (route_id rid, const ActionMessage &cmd)
{
    if (isPriorityCommand (cmd))
    {
        txQueue.emplacePriority (rid, cmd);
    }
    else
    {
        txQueue.emplace (rid, cmd);
    }
}

void CommsInterface::transmit (route_id rid, ActionMessage &&cmd)
{
    if (isPriorityCommand (cmd))
    {
        txQueue.emplacePriority (rid, std::move (cmd));
    }
    else
    {
        txQueue.emplace (rid, std::move (cmd));
    }
}

void CommsInterface::addRoute (route_id rid, const std::string &routeInfo)
{
    ActionMessage rt (CMD_PROTOCOL_PRIORITY);
    rt.payload = routeInfo;
    rt.messageID = NEW_ROUTE;
    rt.setExtraData (rid.baseValue ());
    transmit (control_route, std::move (rt));
}

void CommsInterface::removeRoute (route_id rid)
{
    ActionMessage rt (CMD_PROTOCOL);
    rt.messageID = REMOVE_ROUTE;
    rt.setExtraData (rid.baseValue ());
    transmit (control_route, rt);
}

void CommsInterface::setTxStatus (connection_status txStatus)
{
    if (tx_status == txStatus)
    {
        return;
    }
    switch (txStatus)
    {
    case connection_status::connected:
        if (tx_status == connection_status::startup)
        {
            tx_status = txStatus;
            txTrigger.activate ();
        }
        break;
    case connection_status::terminated:
    case connection_status::error:
        if (tx_status == connection_status::startup)
        {
            tx_status = txStatus;
            txTrigger.activate ();
            txTrigger.trigger ();
        }
        else
        {
            tx_status = txStatus;
            txTrigger.trigger ();
        }
        break;
    default:
        tx_status = txStatus;
    }
}

void CommsInterface::setRxStatus (connection_status rxStatus)
{
    if (rx_status == rxStatus)
    {
        return;
    }
    switch (rxStatus)
    {
    case connection_status::connected:
        if (rx_status == connection_status::startup)
        {
            rx_status = rxStatus;
            rxTrigger.activate ();
        }
        break;
    case connection_status::terminated:
    case connection_status::error:
        if (rx_status == connection_status::startup)
        {
            rx_status = rxStatus;
            rxTrigger.activate ();
            rxTrigger.trigger ();
        }
        else
        {
            rx_status = rxStatus;
            rxTrigger.trigger ();
        }

        break;
    default:
        rx_status = rxStatus;
    }
}

bool CommsInterface::connect ()
{
    if (isConnected ())
    {
        return true;
    }
    if (rx_status != connection_status::startup)
    {
        return false;
    }
    if (tx_status != connection_status::startup)
    {
        return false;
    }
    // bool exp = false;
    if (!ActionCallback)
    {
        logError ("no callback specified, the receiver cannot start");
        return false;
    }
    if (!propertyLock ())
    {
        // this will lock all the properties and should not be unlocked;
        return isConnected ();
    }
    std::unique_lock<std::mutex> syncLock (threadSyncLock);
    if (name.empty ())
    {
        name = localTargetAddress;
    }
    if (localTargetAddress.empty ())
    {
        localTargetAddress = name;
    }
    if (!singleThread)
    {
        queue_watcher = std::thread ([this] {
            try
            {
                queue_rx_function ();
            }
            catch (const std::exception &e)
            {
                rx_status = connection_status::error;
                logError (std::string ("error in receiver >") + e.what ());
            }
        });
    }

    queue_transmitter = std::thread ([this] {
        try
        {
            queue_tx_function ();
        }
        catch (const std::exception &e)
        {
            tx_status = connection_status::error;
            logError (std::string ("error in transmitter >") + e.what ());
        }
    });
    txTrigger.waitActivation ();
    rxTrigger.waitActivation ();
    if (rx_status != connection_status::connected)
    {
        logError ("receiver connection failure");
        if (tx_status == connection_status::connected)
        {
            if (queue_transmitter.joinable ())
            {
                closeTransmitter ();
                queue_transmitter.join ();
            }
        }
        if (!singleThread)
        {
            queue_watcher.join ();
        }
        return false;
    }

    if (tx_status != connection_status::connected)
    {
        logError ("transmitter connection failure");
        if (!singleThread)
        {
            if (rx_status == connection_status::connected)
            {
                if (queue_watcher.joinable ())
                {
                    closeReceiver ();
                    queue_watcher.join ();
                }
            }
        }
        queue_transmitter.join ();
        return false;
    }
    return true;
}

void CommsInterface::setName (const std::string &name_)
{
    if (propertyLock ())
    {
        name = name_;
        propertyUnLock ();
    }
}

void CommsInterface::disconnect ()
{
    if (!operating)
    {
        if (propertyLock ())
        {
            setRxStatus (connection_status::terminated);
            setTxStatus (connection_status::terminated);
            return;
        }
    }
    requestDisconnect.store (true, std::memory_order::memory_order_release);

    if (rx_status.load () <= connection_status::connected)
    {
        closeReceiver ();
    }
    if (tx_status.load () <= connection_status::connected)
    {
        closeTransmitter ();
    }
    if (tripDetector.isTripped ())
    {
        setRxStatus (connection_status::terminated);
        setTxStatus (connection_status::terminated);
        return;
    }
    int cnt = 0;
    while (rx_status.load () <= connection_status::connected)
    {
        if (rxTrigger.wait_for (std::chrono::milliseconds (800)))
        {
            continue;
        }
        ++cnt;
        if ((cnt & 3) == 0)  // call this every 2400 milliseconds
        {
            // try calling closeReceiver again
            closeReceiver ();
        }
        if (cnt == 14)  // Eventually give up
        {
            logError ("unable to terminate receiver connection");
            break;
        }
        // check the trip detector
        if (tripDetector.isTripped ())
        {
            rx_status = connection_status::terminated;
            tx_status = connection_status::terminated;
            return;
        }
    }
    cnt = 0;
    while (tx_status.load () <= connection_status::connected)
    {
        if (txTrigger.wait_for (std::chrono::milliseconds (800)))
        {
            continue;
        }
        ++cnt;
        if ((cnt & 3) == 0)  // call this every 2400 milliseconds
        {
            // try calling closeTransmitter again
            closeTransmitter ();
        }
        if (cnt == 14)  // Eventually give up
        {
            logError ("unable to terminate transmit connection");
            break;
        }
        // check the trip detector
        if (tripDetector.isTripped ())
        {
            rx_status = connection_status::terminated;
            tx_status = connection_status::terminated;
            return;
        }
    }
}

bool CommsInterface::reconnect ()
{
    rx_status = connection_status::reconnecting;
    tx_status = connection_status::reconnecting;
    reconnectReceiver ();
    reconnectTransmitter ();
    int cnt = 0;
    while (rx_status.load () == connection_status::reconnecting)
    {
        std::this_thread::sleep_for (std::chrono::milliseconds (50));
        ++cnt;
        if (cnt == 400)  // Eventually give up
        {
            logError ("unable to reconnect");
            break;
        }
    }
    cnt = 0;
    while (tx_status.load () == connection_status::reconnecting)
    {
        std::this_thread::sleep_for (std::chrono::milliseconds (50));
        ++cnt;
        if (cnt == 400)
        {
            logError ("unable to reconnect");
            break;
        }
    }

    return ((rx_status.load () == connection_status::connected) &&
            (tx_status.load () == connection_status::connected));
}

void CommsInterface::setCallback (std::function<void (ActionMessage &&)> callback)
{
    if (propertyLock ())
    {
        ActionCallback = std::move (callback);
        propertyUnLock ();
    }
}

void CommsInterface::setLoggingCallback (
  std::function<void (int level, const std::string &name, const std::string &message)> callback)
{
    loggingCallback = std::move (callback);
}

void CommsInterface::setMessageSize (int maxMsgSize, int maxCount)
{
    if (propertyLock ())
    {
        if (maxMsgSize > 0)
        {
            maxMessageSize = maxMsgSize;
        }
        if (maxCount > 0)
        {
            maxMessageCount = maxCount;
        }
        propertyUnLock ();
    }
}

void CommsInterface::setFlag (const std::string &flag, bool val)
{
    if (flag == "server_mode")
    {
        setServerMode (val);
    }
}

void CommsInterface::setTimeout (std::chrono::milliseconds timeOut)
{
    if (propertyLock ())
    {
        connectionTimeout = timeOut;
        propertyUnLock ();
    }
}

void CommsInterface::setServerMode (bool serverActive)
{
    if (propertyLock ())
    {
        serverMode = serverActive;
        propertyUnLock ();
    }
}

bool CommsInterface::isConnected () const
{
    return ((tx_status == connection_status::connected) && (rx_status == connection_status::connected));
}

void CommsInterface::logMessage (const std::string &message) const
{
    if (loggingCallback)
    {
        loggingCallback (3, name, message);
    }
    else
    {
        std::cout << "commMessage||" << name << ":" << message << std::endl;
    }
}

void CommsInterface::logWarning (const std::string &message) const
{
    if (loggingCallback)
    {
        loggingCallback (1, name, message);
    }
    else
    {
        std::cerr << "commWarning||" << name << ":" << message << std::endl;
    }
}

void CommsInterface::logError (const std::string &message) const
{
    if (loggingCallback)
    {
        loggingCallback (0, name, message);
    }
    else
    {
        std::cerr << "commERROR||" << name << ":" << message << std::endl;
    }
}

void CommsInterface::closeTransmitter ()
{
    ActionMessage rt (CMD_PROTOCOL);
    rt.messageID = DISCONNECT;
    transmit (control_route, rt);
}

void CommsInterface::closeReceiver ()
{
    ActionMessage cmd (CMD_PROTOCOL);
    cmd.messageID = CLOSE_RECEIVER;
    transmit (control_route, cmd);
}

void CommsInterface::reconnectTransmitter ()
{
    ActionMessage rt (CMD_PROTOCOL);
    rt.messageID = RECONNECT_TRANSMITTER;
    transmit (control_route, rt);
}

void CommsInterface::reconnectReceiver ()
{
    ActionMessage cmd (CMD_PROTOCOL);
    cmd.messageID = RECONNECT_RECEIVER;
    transmit (control_route, cmd);
}

}  // namespace helics
