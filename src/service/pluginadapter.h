// Copyright (c) 2015-2019 LG Electronics, Inc.
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

#include <string>
#include <unordered_map>

#include <event-monitor-api/api.h>

#include "pluginloader.h"
#include "lunaservice.h"

using namespace EventMonitor;

class PluginManager;
class PluginAdapter;

class TimeoutState
{
public:
	PluginAdapter *adapter;
	std::string timeoutId;
	bool repeat;
	guint handle;
	TimeoutCallback callback;
};

/**
 * Implement the Manager API and handles all incoming calls from the plugin.
 * Each plugin instance has a matching adapter instance that handles calls
 * form that particular plugin.
 */
class PluginAdapter: public Manager
{
public:
	PluginAdapter(PluginManager *manager, const PluginInfo *info);
	virtual ~PluginAdapter();
        PluginAdapter(const PluginAdapter&) = delete;
        PluginAdapter& operator=(const PluginAdapter&) = delete;

	// Manager methods - called from plugin
	void setupLogging(PmLogContext *context);
	void unloadPlugin();

	const std::string getUILocale();

	const pbnjson::JValue &getLocaleInfo();

	pbnjson::JValue lunaCall(const std::string &serviceUrl,
	                         pbnjson::JValue &params,
	                         unsigned long timeout = 1000);

	void lunaCallAsync(const std::string &serviceUrl,
	                   pbnjson::JValue &params,
	                   LunaCallback callback);

	/**
	 * Registers a luna method on bus.
	 * This method may be called multiple times for same methodName to update
	 * the handler or schema or simply to retrieve the method path.
	 * Overriding methods registered by different plugin is not allowed
	 * and will result in exception.
	 * @param categoryName - category name. Must start with "/".
	 * @param methodName - method name. Registering again
	 *                   with same name will override previous method.
	 * @param handler - function to call when method is called,
	 * @params schema - schema for method parameters. All calls not conforming
	 *                 to schema will dropped and logged.
	 * @return method service URL, eg: luna://com.webos.service.event-monitor/pluginName/methodName
	 */
	virtual std::string registerMethod(const std::string &categoryName,
	                                   const std::string &methodName,
	                                   LunaCallHandler handler,
	                                   const pbnjson::JSchema &schema = pbnjson::JSchema::AllSchema());

	void subscribeToMethod(
	    const std::string &subscriptionId,
	    const std::string &methodPath,
	    pbnjson::JValue &params,
	    SubscribeCallback callback,
	    const pbnjson::JSchema &schema = pbnjson::JSchema::AllSchema());

	bool unsubscribeFromMethod(const std::string &subscriptionId);


	void subscribeToSignal(
			const std::string &subscriptionId,
			const std::string &category,
			const std::string &method,
			SubscribeCallback callback,
			const pbnjson::JSchema &schema = pbnjson::JSchema::AllSchema());

	bool unsubscribeFromSignal(const std::string &subscriptionId);

	void setTimeout(const std::string &timeoutId,
	                unsigned int timeMs,
	                bool repeat,
	                TimeoutCallback callback);

	bool cancelTimeout(const std::string &timeoutId);

	void createToast(
	    const std::string &message,
	    const std::string &iconUrl = "",
	    const pbnjson::JValue &onClickAction = pbnjson::JValue());

	/**
	 * Convenience method to create an alert.
	 */
	void createAlert(const std::string &alertId,
	                 const std::string &title,
	                 const std::string &message,
	                 bool modal,
	                 const std::string &iconUrl,
	                 const pbnjson::JValue &buttons,
	                 const pbnjson::JValue &onClose);

	/**
	 * Closes the alert specified by id, if open.
	 */
	bool closeAlert(const std::string &alertId);

	// Methods - called from manager
	void pluginLoaded(Plugin *plugin);

	void notifyLocaleChanged(const std::string &locale);
	void notifyPluginShouldUnload(const std::string &service);

	inline const PluginInfo *getInfo()
	{
		return this->info;
	}

public:
	//Plugin needs to be unloaded
	bool needUnload;
	PluginManager *manager;

private:
	static gboolean timeoutCallback(gpointer userData);

	const PluginInfo *info;
	PmLogContext logContext;
	Plugin *plugin;
	//Plugin was notified that it should unload
	bool unloadNotified;

	// Active subscriptions
	std::unordered_map<std::string, SubscribeHandle> subscriptions;

	// Active alerts
	std::unordered_map<std::string, TimeoutState *> timeouts;

	// Active alerts
	std::unordered_map<std::string, std::string> alerts;
};
