// Copyright (c) 2015-2018 LG Electronics, Inc.
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

#pragma once

#include <webosi18n.h>
#include <string>
#include <event-monitor-api.h>
#include <event-monitor-api/pluginbase.hpp>
#include <unordered_map>

class MockPlugin: public EventMonitor::PluginBase
{
public:
	MockPlugin(EventMonitor::Manager *manager);
	virtual ~MockPlugin();
	void startMonitoring();
	EventMonitor::UnloadResult stopMonitoring(const std::string &service);
	virtual void uiLocaleChanged(const std::string &locale);

private:

	//Map of events.
	std::unordered_map<std::string, bool> eventsMockPlugin;

	void foregroundAppCallback(pbnjson::JValue& previousValue,
	                                       pbnjson::JValue& value);
	void batteryStatusCallback(pbnjson::JValue& previousValue,
	                                       pbnjson::JValue& value);
	void boosterFinishedCallback(pbnjson::JValue& previousValue,
	                                       pbnjson::JValue& value);
	void toastNotificationCallback(pbnjson::JValue& previousValue,
	                                       pbnjson::JValue& value);
	void alertNotificationCallback(pbnjson::JValue& previousValue,
	                                       pbnjson::JValue& value);
	void startAlert(const std::string &timeoutId);
	pbnjson::JValue actionCallback(const pbnjson::JValue &params);
	pbnjson::JValue getEventsCb();
};

