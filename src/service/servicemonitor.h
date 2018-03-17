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

#include <set>
#include "lunaservice.h"
#include "pluginloader.h"
#include "pluginmanager.h"

/**
 * Monitors all services present in plugin loader's list and notifies
 * manager about what plugins need to be loaded/unloaded.
 * Also monitors system locale change.
 */
class ServiceMonitor
{
public:
	ServiceMonitor(PluginManager &manager, LunaService &service);
	~ServiceMonitor();

	void startMonitor(const std::vector<PluginInfo> *plugins);
	void stopMonitor();



private:
	void localeCallback(pbnjson::JValue &previousValue, pbnjson::JValue &value);
	void serviceStatusCallback(pbnjson::JValue &previousValue,
	                           pbnjson::JValue &value);
	void addPlugin(const PluginInfo &info);
	void updatePlugins(const std::string &serviceName);

private:
	PluginManager &manager;
	LunaService &service;
	const std::vector<PluginInfo> *plugins;

	std::unordered_map<std::string, bool> serviceStatus;
	bool monitorStarted;
};
