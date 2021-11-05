/*
 * SPDX-FileCopyrightText: 2021 Andrea Pappacoda
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#pragma once
#define BOOST_UT_DISABLE_MODULE
#include <boost/ut.hpp>
#include <tachi/cloudflare-ddns.h>
#include "credentials.hpp"
#ifdef TACHI_HAS_ARPA_INET_H
	#include <arpa/inet.h>
#else
	#define INET6_ADDRSTRLEN 46
#endif

using namespace boost::ut;
