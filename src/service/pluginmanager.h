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

#include <unordered_map>
#include "pluginadapter.h"


/**
 * Manages list of loaded plugins and dispatches common notifications to them.
 * Handles plugin loading unloading.
 */
class PluginManager
{
public:
	PluginManager(PluginLoader &loader, LunaService &service, GMainLoop *mainLoop);

	~PluginManager();

	/**
	 * Called by lunaMonitor
	 */
	void loadPlugin(const PluginInfo *pluginInfo, const std::string &service);

	bool isPluginLoaded(const PluginInfo *pluginInfo);

	//  void unloadPlugin(const PluginInfo* pluginInfo);

	/**
	 * Check if plugin is marked for unload and if so unload it.
	 */
	void processUnload(PluginAdapter *adapter);

	/**
	 * Called by lunaMonitor
	 */
	void notifyPluginShouldUnload(const PluginInfo *pluginInfo,
	                              const std::string &serviceName);

	/**
	 * Called by lunaMonitor
	 */
	void notifyLocaleChanged(const pbnjson::JValue &locale);

	const std::string getUILocale();

public:
	LunaService &lunaService;
	pbnjson::JValue locale;
	GMainLoop *mainLoop;

private:
	PluginLoader &loader;

	/**
	 * Map plugin path to plugin adapter.
	 */
	std::unordered_map<std::string, PluginAdapter *> activePlugins;
};
