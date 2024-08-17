// Copyright (c) 2015-2024 LG Electronics, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// SPDX-License-Identifier: Apache-2.0

#include <event-monitor-api/api.h>

#include "plugininfo.h"
#include "utils.h"
#include "logging.h"

using namespace EventMonitor;

bool PluginInfo::containsURI(const std::string &uri) const
{
	auto parts = splitString(uri, '/');

	if (parts.size() < 3)
	{
		LOG_INFO(MSGID_PLUGIN_INVALID_SUBSCRIBE, 0,
		         "Parts.size < 3, %u", static_cast<unsigned int>(parts.size()));

		throw Error("Bad luna URL");
	}

	if (parts[0] != "luna:" || parts[1] != "" || parts[2].length() == 0)
	{

		LOG_INFO(MSGID_PLUGIN_INVALID_SUBSCRIBE, 0,
		         "Parts bad, %s, %s, %s", parts[0].c_str(), parts[1].c_str(), parts[2].c_str());

		throw Error("Bad luna URL");
	}

	std::string serviceName = parts[2];

	for (const auto& s : this->requiredServices)
	{
		if (s == serviceName)
		{
			return true;
		}
	}

	return false;
}
