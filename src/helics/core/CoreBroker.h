/*

Copyright (C) 2017-2018, Battelle Memorial Institute
All rights reserved.

This software was co-developed by Pacific Northwest National Laboratory, operated by the Battelle Memorial Institute; the National Renewable Energy Laboratory, operated by the Alliance for Sustainable Energy, LLC; and the Lawrence Livermore National Laboratory, operated by Lawrence Livermore National Security, LLC.

*/
#ifndef CORE_BROKER_H_
#define CORE_BROKER_H_
#pragma once

#include <atomic>
#include <memory>
#include <string>
#include <thread>
#include <map>
#include <unordered_map>
#include <functional>

#include "BasicHandleInfo.h"
#include "ActionMessage.h"
#include "../common/simpleQueue.hpp"
#include "TimeDependencies.h"
#include "BrokerBase.h"
#include "broker.h"

namespace helics
{
/** class defining the common information for a federate*/
class BasicFedInfo
{
public:
	std::string name; //!< name of the federate
	Core::federate_id_t global_id=invalid_fed_id; //!< the identification code for the federate
	int32_t route_id=invalid_fed_id; //!< the routing information for data to be sent to the federate
	BasicFedInfo(const std::string &fedname) :name(fedname) {};
};

/** class defining the common information about a broker federate*/
class BasicBrokerInfo
{
public:
	std::string name; //!< the name of the broker
	
	Core::federate_id_t global_id=invalid_fed_id;	//!< the global identifier for the broker
	int32_t route_id=invalid_fed_id;	//!< the identifier for the route to take to the broker
	
	bool _initRequested = false;	//!< flag indicating the broker has requesting initialization
	bool _disconnected = false;		//!< flag indicating that the broker has disconnected
	bool _hasEndpoints = true;		//!< flag indicating that a broker has endpoints it is coordinating
	std::string routeInfo;	//!< string describing the connection information for the route
	BasicBrokerInfo(const std::string &brokerName) :name(brokerName) {};
	
};

class TimeCoordinator;
class Logger;
/** a shift in the global federate id numbers to allow discrimination between local ids and global ones
this value allows 65535 federates to be available in each core 
1,878,982,656 allowable federates in the system and
268,435,455 brokers allowed  if we need more than that this program has been phenomenally successful beyond 
all wildest imaginations and we can probably afford to change these to 64 bit numbers to accommodate
*/
constexpr Core::federate_id_t global_federate_id_shift = 0x0001'0000;
/** a shift in the global id index to discriminate between global ids of brokers vs federates*/
constexpr Core::federate_id_t global_broker_id_shift = 0x7000'0000;
/** class implementing most of the functionality of a generic broker
Basically acts as a router for information,  deals with stuff internally if it can and sends higher up if it can't
or does something else if it is the root of the tree
*/
class CoreBroker : public Broker, public BrokerBase
{
protected:
	bool _gateway = false;  //!< set to true if this broker should act as a gateway.
private:
	bool _isRoot = false;  //!< set to true if this object is a root broker
	std::vector<std::pair<Core::federate_id_t, bool>> localBrokersInit; //!< indicator if the local brokers are ready to initialize
	std::vector<BasicFedInfo> _federates; //!< container for all federates
	std::vector<BasicHandleInfo> _handles; //!< container for the basic info for all handles
	std::vector<BasicBrokerInfo> _brokers;  //!< container for the basic broker info for all subbrokers
	std::string previous_local_broker_identifier; //!< the previous identifier in case a rename is required
	std::unordered_map<std::string, int32_t> fedNames;  //!< a map to lookup federates <fed name, local federate index>
	std::unordered_map<std::string, int32_t> brokerNames;  //!< a map to lookup brokers <broker name, local broker index>
	std::unordered_map<std::string, int32_t> publications; //!< map of publications;
	std::unordered_multimap<std::string, int32_t> subscriptions; //!< multimap of subscriptions
	std::unordered_map<std::string, int32_t> endpoints;  //!< map of endpoints
	std::unordered_multimap<std::string, int32_t> filters;  //!< multimap for all the filters

	std::map<Core::federate_id_t, int32_t> global_id_translation; //!< map to translate global ids to local ones
	std::unordered_map<uint64_t, int32_t> handle_table; //!< map to translate global handles to local ones
	std::map<Core::federate_id_t, int32_t> routing_table;  //!< map for external routes  <global federate id, route id>
	std::map<Core::federate_id_t, int32_t> broker_table;  //!< map for translating global broker id's to a local index
	std::map<Core::federate_id_t, int32_t> federate_table; //!< map for translating global federate id's to a local index
	std::unordered_map<std::string, int32_t> knownExternalEndpoints; //!< external map for all known external endpoints with names and route
    std::mutex mutex_; //mutex lock for name and identifier
private:

	/** function that processes all the messages
	@param[in] command -- the message to process
	*/
	virtual void processCommand(ActionMessage &&command) override;
	/** function to process a priority command independent of the main queue
	@detailed called from addMessage function which detects if the command is a priority command
	this mainly deals with some of the registration functions
	@param[in] command the command to process
	*/
	void processPriorityCommand(ActionMessage &&command) override;

	SimpleQueue<ActionMessage> delayTransmitQueue; //!< FIFO queue for transmissions to the root that need to be delayed for a certain time
	/* function to transmit the delayed messages*/
	void transmitDelayedMessages();
    /**function for routing a message,  it will override the destination id with the specified argument
    */
    void routeMessage(ActionMessage &cmd, Core::federate_id_t dest);
    /** function for routing a message from based on the destination specified in the ActionMessage*/
    void routeMessage(const ActionMessage &cmd);
	
	int32_t fillMessageRouteInformation(ActionMessage &mess);
    /**generate the results of a query directed at the broker*/
    void generateQueryResult(const ActionMessage &command);
public:
	/** connect the core to its broker
	@details should be done after initialization has complete*/
	virtual bool connect() override final;
	/** disconnect the broker from any other brokers and communications
	*/
	virtual void disconnect() override final;
    /** unregister the broker from the factory find methods*/
    void unregister();
    /** disconnect the broker from any other brokers and communications
    **if the flag is set it should not do the unregister step of the disconnection, if this is set it is presumed
    the unregistration has already happened or it will be taken care of manually
    */
	virtual void processDisconnect(bool skipUnregister = false) override final;
	/** check if the broker is connected*/
	virtual bool isConnected() const override final;
	/** set the broker to be a root broker
	@details only valid before the initialization function is called*/
	virtual void setAsRoot() override final;
	/** return true if the broker is a root broker
	*/
	virtual bool isRoot() const override final{
		return _isRoot;
	};

    virtual bool isOpenToNewFederates() const override;
    /** display the help for command line arguments on the broker*/
	static void displayHelp();
private:
	/** implementation details of the connection process
	*/
	virtual bool brokerConnect()=0;
	/** implementation details of the disconnection process
	*/
	virtual void brokerDisconnect() = 0;
protected:
	/** this function is the one that will change for various flavors of broker communication
	@details it takes a route info- a code of where to send the data and an action message
	and proceeds to transmit it to the appropriate location
	@param[in] route -the identifier for the routing information
	@param[in] command the actionMessage to transmit
	*/
	virtual void transmit(int32_t route, const ActionMessage &command) = 0;
	/** add a route to the type specific routing information and establish the connection
	@details add a route to a table, the connection information is contained in the string with the described identifier
	@param[in] route_id  the identifier for the route
	@param[in] routeInfo  a string describing the connection info
	*/
	virtual void addRoute(int route_id, const std::string &routeInfo) = 0;

public:
	/**default constructor
	@param setAsRootBroker  set to true to indicate this object is a root broker*/
	CoreBroker(bool setAsRootBroker = false) noexcept;
	/** constructor to set the name of the broker*/
	CoreBroker(const std::string &broker_name);
	/** destructor*/
	virtual ~CoreBroker();
	/** start up the broker with an initialization string containing commands and parameters*/
	virtual void initialize(const std::string &initializationString) override final;
	/** initialize from command line arguments
	*/
	virtual void initializeFromArgs(int argc, const char * const *argv) override;

	/** check if all the local federates are ready to be initialized
	@return true if everyone is ready, false otherwise
	*/
	bool allInitReady() const;
	bool allDisconnected() const;
	/** set the local identification string for the broker*/
	void setIdentifier(const std::string &name);
	/** get the local identification for the broker*/
    virtual const std::string &getIdentifier() const override final
	{
		return identifier;
	}

private:
    /** check if we can remove some dependencies*/
    void checkDependencies();
    /** check subscriptions for completion and mismatches*/
	void checkSubscriptions();
    /** find any existing publishers for a subscription*/
	bool FindandNotifySubscriptionPublisher(BasicHandleInfo &handleInfo);
	void FindandNotifyPublicationSubscribers(BasicHandleInfo &handleInfo);
    /** check endpoints for any issues*/
	void checkEndpoints();
    /** check filters for any issues*/
	void checkFilters();
	bool FindandNotifyFilterEndpoint(BasicHandleInfo &handleInfo);
	void FindandNotifyEndpointFilters(BasicHandleInfo &handleInfo);
    /** answer a query or route the message the appropriate location*/
    void processQuery(const ActionMessage &m);
    /** generate an answer to a local query*/
    void processLocalQuery(const ActionMessage &m);
    /** generate an actual response string to a query*/
    std::string generateQueryAnswer(const std::string &query) const;
	/** locate the route to take to a particular federate*/
	int32_t getRoute(Core::federate_id_t fedid) const;
	int32_t getFedByName(const std::string &fedName) const;
	int32_t getBrokerByName(const std::string &brokerName) const;
	int32_t getBrokerById(Core::federate_id_t fedid) const;
	int32_t getFedById(Core::federate_id_t fedid) const;

	void addLocalInfo(BasicHandleInfo &handleInfo, const ActionMessage &m);
	void addPublication(ActionMessage &m);
	void addSubscription(ActionMessage &m);
	void addEndpoint(ActionMessage &m);
	void addDestFilter(ActionMessage &m);
	void addSourceFilter(ActionMessage &m);
	bool updateSourceFilterOperator(ActionMessage &m);
    /** generate a json string containing the federate/broker/Core Map*/
    std::string generateFederateMap() const;
};


inline uint64_t makeGlobalHandleIdentifier(Core::federate_id_t fed_id, Core::handle_id_t handle)
{
	return (static_cast<uint64_t>(fed_id) << 32) + static_cast<uint64_t>(handle);
}

} //namespace helics

#endif

