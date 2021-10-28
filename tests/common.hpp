/*
 * SPDX-FileCopyrightText: 2021 Andrea Pappacoda
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#pragma once
#define BOOST_UT_DISABLE_MODULE
#undef  NDEBUG // need C assert()
// don't know why, but I get errors about multiple definitions
#if defined _WIN32 || defined __CYGWIN__
	#define TACHI_PUB __declspec(dllimport)
#endif
#include <boost/ut.hpp>
#include <tachi/cloudflare-ddns.hpp>
#include "credentials.hpp"
#include <cassert>

using namespace boost::ut;
