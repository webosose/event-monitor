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

#include <pbnjson.hpp>
#include <unordered_map>
#include <luna-service2++/handle.hpp>

#include <event-monitor-api/api.h>

class LunaService;
class PluginAdapter;

class MethodInfo
{
public:
	MethodInfo():
			plugin(nullptr),
			schema(pbnjson::JSchema::AllSchema())
	{};

	PluginAdapter *plugin; // Null if plugin unloaded
	EventMonitor::LunaCallHandler handler;
	pbnjson::JSchema schema;
	std::string url;
};

class SubscriptionInfo
{
public:
	SubscriptionInfo(const pbnjson::JSchema &_schema):
			service(nullptr),
			plugin(nullptr),
			schema(_schema),
	        counter(0)
	{};

	LunaService *service;
	PluginAdapter *plugin;
	EventMonitor::SubscribeCallback subscribeCallback;
	EventMonitor::LunaCallback simpleCallback;
	std::string serviceUrl;
	pbnjson::JValue previousValue;
	pbnjson::JSchema schema;
	LS::Call call;
	unsigned long long counter;
};

typedef SubscriptionInfo *SubscribeHandle;
typedef SubscriptionInfo *CallHandle;

/**
 * Wrapper around the luna service.
 * Connects to the bus and provides convenience methods.
 */
class LunaService: private LS::Handle
{
public:
	LunaService(std::string servicePath, GMainLoop *mainLoop,
	            const char *identifier);
	~LunaService();

	/**
	 * Not copyable (because the handle is not copyable).
	 */
	LunaService(const LunaService &) = delete;
	LunaService &operator=(const LunaService &) = delete;

	pbnjson::JValue call(const std::string &serviceUrl,
	                     pbnjson::JValue &params,
	                     unsigned long timeout = 1000);
	CallHandle callAsync(const std::string &serviceUrl,
	                     pbnjson::JValue &params,
	                     EventMonitor::LunaCallback callback,
	                     PluginAdapter *plugin);

	/**
	 * Subscribe to luna method
	 * @param checkFirstResponse - if true, will wait synchronously for first
	 * response to arrive and not passs it to the callback method.
	 * The return value of the resposne will be checked and exception raised if
	 * it is not successful.
	 */
	SubscribeHandle subscribeToMethod(
	    const std::string &serviceUrl,
	    pbnjson::JValue &params,
	    EventMonitor::SubscribeCallback callback,
	    const pbnjson::JSchema &schema,
	    PluginAdapter *plugin,
	    bool checkFirstResponse = false);

	void cancelSubscribe(SubscriptionInfo *handle);

	/**
	 * Cancels all subscriptions and calls associated with this plugin.
	 */
	void cleanupPlugin(PluginAdapter *plugin);

	MethodInfo* registerMethod(PluginAdapter *plugin,
	                           const std::string &category,
	                           const std::string &methodName,
	                           EventMonitor::LunaCallHandler handler,
	                           const pbnjson::JSchema &schema);

private:
	static bool callResultHandler(LSHandle *handle, LSMessage *message,
	                              void *context);
	static void onLunaDisconnect(LSHandle *sh, void *user_data);
	bool methodHandler(LSMessage &msg);
	bool callResult(SubscriptionInfo *info, LSMessage *message);

	inline MethodInfo* findMethod(const std::string& category, const std::string& name)
	{
		if (this->categoryMethods.count(category) != 0 && this->categoryMethods[category].count(name) != 0)
		{
			return this->categoryMethods[category][name];
		}
		else
		{
			return nullptr;
		}
	}

public:
	const std::string servicePath;

private:
	std::unordered_map<SubscriptionInfo *, SubscriptionInfo *> subscriptions;
	std::unordered_map<std::string, std::unordered_map<std::string, MethodInfo*> > categoryMethods;
};

