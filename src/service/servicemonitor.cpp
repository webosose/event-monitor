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


#include "servicemonitor.h"
#include "logging.h"

using namespace pbnjson;

ServiceMonitor::ServiceMonitor(PluginManager &_manager, LunaService &_service):
	manager(_manager),
	service(_service),
	plugins(nullptr),
	monitorStarted(false)
{
}

ServiceMonitor::~ServiceMonitor()
{
	this->stopMonitor();
}

void ServiceMonitor::startMonitor(const std::vector<PluginInfo> *plugins)
{
	this->plugins = plugins;

	JValue params = JObject{{"keys", JArray({JValue("localeInfo")})}};

	this->service.subscribeToMethod("luna://com.webos.settingsservice/getSystemSettings",
	                          params,
	                          std::bind(&ServiceMonitor::localeCallback,
	                                    this,
	                                    std::placeholders::_1,
	                                    std::placeholders::_2),
	                          pbnjson::JSchema::AllSchema(),
	                          nullptr,
	                          false);
	// Start the actual monitor once we have the locale information
	// Otherwise plugins will be created with incorrect locale.
}

void ServiceMonitor::stopMonitor()
{

}

void ServiceMonitor::localeCallback(pbnjson::JValue &previousValue,
                                    pbnjson::JValue &value)
{
	JValue locale = value["settings"]["localeInfo"];

	if (!locale.isValid())
	{
		LOG_ERROR(MSGID_SETTINGS_LOCALE_MISSING, 0,
		          "settings/localeinfo not found in payload: %s.", value.stringify().c_str());
		return;
	}

	this->manager.notifyLocaleChanged(locale);

	if (!this->monitorStarted)
	{
		for (const auto& iter : *this->plugins)
		{
			this->addPlugin(iter);
		}

		this->monitorStarted = true;
	}
}

void ServiceMonitor::addPlugin(const PluginInfo &info)
{
	LOG_INFO(MSGID_SERVICE_STATUS, 0, "Adding plugin from %s", info.path.c_str());

	for (const auto& service : info.requiredServices)
	{
		if (this->serviceStatus.count(service) > 0)
		{
			continue; //already subscribed.
		}

		LOG_INFO(MSGID_SERVICE_STATUS, 0, "Monitoring service %s", service.c_str());

		this->serviceStatus[service] = false;
		JValue params = JObject{{"serviceName", JValue(service.c_str())}};

		this->service.subscribeToMethod(
		    "luna://com.webos.service.bus/signal/registerServerStatus",
		    params,
		    std::bind(&ServiceMonitor::serviceStatusCallback,
		              this,
		              std::placeholders::_1,
		              std::placeholders::_2),
		    pbnjson::JSchema::AllSchema(),
		    nullptr,
		    false);
	}
}

void ServiceMonitor::serviceStatusCallback(pbnjson::JValue &previousValue,
        pbnjson::JValue &value)
{
	std::string serviceName;
	bool connected;
	ConversionResultFlags problems = 0;

	problems |= value["serviceName"].asString(serviceName);
	problems |= value["connected"].asBool(connected);

	if (problems)
	{
		LOG_ERROR(MSGID_SERVICE_STATUS, 0,
		          "Could not parse registerServerStatus response: %s", value.stringify().c_str());
		throw Error("Could not parse registerServerStatus response");
	}

	if (this->serviceStatus.count(serviceName) == 0)
	{
		LOG_WARNING(MSGID_SERVICE_STATUS, 0,
		            "Service status response on unexpected service: %s", value.stringify().c_str());
		return;
	}

	bool wasConnected = this->serviceStatus[serviceName];
	this->serviceStatus[serviceName] = connected;

	if (connected)
	{
		LOG_INFO(MSGID_SERVICE_STATUS, 0, "Service %s is now online",
		         serviceName.c_str());
	}
	else
	{
		LOG_INFO(MSGID_SERVICE_STATUS, 0, "Service %s is now offline",
		         serviceName.c_str());
	}

	if (wasConnected != connected)
	{
		this->updatePlugins(serviceName);
	}
}

void ServiceMonitor::updatePlugins(const std::string &serviceName)
{
	for (const PluginInfo &plugin : *this->plugins)
	{
		bool dependenciesMet = true;

		for (const std::string &service : plugin.requiredServices)
		{
			if (!this->serviceStatus[service])
			{
				dependenciesMet = false;
				break;
			}
		}

		if (dependenciesMet)
		{
			this->manager.loadPlugin(&plugin, serviceName);
		}
		else
		{
			this->manager.notifyPluginShouldUnload(&plugin, serviceName);
		}
	}
}
