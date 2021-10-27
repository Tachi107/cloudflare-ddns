/*
 * SPDX-FileCopyrightText: 2021 Andrea Pappacoda
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#pragma once
#define BOOST_UT_DISABLE_MODULE
#undef  NDEBUG // need C assert()
#include <boost/ut.hpp>
#include <tachi/cloudflare-ddns.hpp>
#include "credentials.hpp"
#include <cassert>

using namespace boost::ut;
