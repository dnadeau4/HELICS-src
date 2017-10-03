/*
Copyright (C) 2017, Battelle Memorial Institute
All rights reserved.

This software was co-developed by Pacific Northwest National Laboratory, operated by the Battelle Memorial
Institute; the National Renewable Energy Laboratory, operated by the Alliance for Sustainable Energy, LLC; and the
Lawrence Livermore National Laboratory, operated by Lawrence Livermore National Security, LLC.

*/

#include <boost/test/unit_test.hpp>
#include <boost/test/floating_point_comparison.hpp>

#include <complex>
/** these test cases test out the value converters
 */
#include <boost/test/floating_point_comparison.hpp>

#include "helics/application_api/Publications.hpp"
#include "helics/application_api/Subscriptions.hpp"
#include "test_configuration.h"
#include <future>

BOOST_AUTO_TEST_SUITE (subPubObject_tests)

BOOST_AUTO_TEST_CASE (subscriptionTObject_tests)
{
    helics::FederateInfo fi ("test1");
    fi.coreType = CORE_TYPE_TO_TEST;
    fi.coreInitString = "1";

    auto vFed = std::make_shared<helics::ValueFederate> (fi);
    // register the publications
    auto pubObj = helics::PublicationT<std::string> (helics::GLOBAL, vFed.get (), "pub1");

    auto subObj = helics::SubscriptionT<std::string> (vFed.get (), "pub1");
    vFed->setTimeDelta (1.0);
    vFed->enterExecutionState ();
    // publish string1 at time=0.0;
    pubObj.publish ("string1");
    auto gtime = vFed->requestTime (1.0);

    BOOST_CHECK_EQUAL (gtime, 1.0);
    std::string s = subObj.getValue ();

    // make sure the string is what we expect
    BOOST_CHECK_EQUAL (s, "string1");
    // publish a second string
    pubObj.publish ("string2");
    // make sure the value is still what we expect
    subObj.getValue (s);

    BOOST_CHECK_EQUAL (s, "string1");
    // advance time
    gtime = vFed->requestTime (2.0);
    // make sure the value was updated
    BOOST_CHECK_EQUAL (gtime, 2.0);
    subObj.getValue (s);

    BOOST_CHECK_EQUAL (s, "string2");
    vFed->finalize ();
}

BOOST_AUTO_TEST_CASE (subscriptionObject_tests)
{
    helics::FederateInfo fi ("test1");
    fi.coreType = CORE_TYPE_TO_TEST;
    fi.coreInitString = "1";

    auto vFed = std::make_shared<helics::ValueFederate> (fi);
    // register the publications
    auto pubObj = helics::make_publication<std::string> (helics::GLOBAL, vFed.get (), std::string ("pub1"));

    auto subObj = helics::Subscription (vFed.get (), "pub1");
    vFed->setTimeDelta (1.0);
    vFed->enterExecutionState ();
    // publish string1 at time=0.0;
    pubObj->publish ("string1");
    auto gtime = vFed->requestTime (1.0);

    BOOST_CHECK_EQUAL (gtime, 1.0);
    std::string s = subObj.getValue<std::string> ();
    // int64_t val = subObj.getValue<int64_t>();
    // make sure the string is what we expect
    BOOST_CHECK_EQUAL (s, "string1");
    // publish a second string
    pubObj->publish ("string2");
    // make sure the value is still what we expect
    subObj.getValue<std::string> (s);

    BOOST_CHECK_EQUAL (s, "string1");
    // advance time
    gtime = vFed->requestTime (2.0);
    // make sure the value was updated
    BOOST_CHECK_EQUAL (gtime, 2.0);
    subObj.getValue (s);

    BOOST_CHECK_EQUAL (s, "string2");
    vFed->finalize ();
}

template <class TX, class RX>
void runPubSubTypeTests(const TX &valtx, const RX &valrx)
{
	helics::FederateInfo fi("test1");
	fi.coreType = CORE_TYPE_TO_TEST;
	fi.coreInitString = "1";

	auto vFed = std::make_shared<helics::ValueFederate>(fi);
	//register the publications
	auto pubObj = helics::make_publication<TX>(helics::GLOBAL, vFed.get(), std::string("pub1"));

	auto subObj = helics::Subscription(vFed.get(), "pub1");
	vFed->setTimeDelta(1.0);
	vFed->enterExecutionState();
	//publish string1 at time=0.0;
	pubObj->publish(valtx);
	auto gtime = vFed->requestTime(1.0);

	BOOST_CHECK_EQUAL(gtime, 1.0);
	auto s = subObj.getValue<RX>();
	//int64_t val = subObj.getValue<int64_t>();
	//make sure the string is what we expect
	BOOST_CHECK(s==valrx);
	vFed->finalize();
}


BOOST_AUTO_TEST_CASE(subscriptionObject_type_tests)
{
	runPubSubTypeTests<std::string, std::string>("test1", "test1");
	runPubSubTypeTests<std::string, double>("3.14159", 3.14159);
	runPubSubTypeTests<double, std::string>(3.14159,std::to_string(3.141590));
	runPubSubTypeTests<double, int64_t>(3.14159, 3);
	runPubSubTypeTests<int64_t, double>(34, 34.0);
	runPubSubTypeTests < int64_t, std::string > (34, "34");
	runPubSubTypeTests < std::string, int64_t >("34.14", 34);
}

BOOST_AUTO_TEST_CASE(subscriptionObject_complex_tests)
{
	using c = std::complex<double>;

	runPubSubTypeTests<c, std::string>(c(12.4,0.3), helics::helicsComplexString(c(12.4,0.3)));
	runPubSubTypeTests<std::string, c>("3.14159+2j", c(3.14159,2));
	runPubSubTypeTests<std::string, c>("3.14159-2j", c(3.14159, -2));
	runPubSubTypeTests<std::string, c>("-3.14159-2j", c(-3.14159, -2));
	runPubSubTypeTests<std::string, c>("-3.14159 - 2i", c(-3.14159, -2));
	runPubSubTypeTests<std::string, c>("-3.14159 + 2i", c(-3.14159, 2));

	runPubSubTypeTests<std::string, c>("2i", c(0, 2));
	runPubSubTypeTests<c, double>(c(0, 2),2.0);
	runPubSubTypeTests<c, int64_t>(c(0, 2), 2);

	runPubSubTypeTests< double,c>(2.0, c(2, 0));

	runPubSubTypeTests< int64_t, c>(2, c(2, 0));
	runPubSubTypeTests<c, double>(c(3.0, 4.0), 5.0);
}

BOOST_AUTO_TEST_SUITE_END()
