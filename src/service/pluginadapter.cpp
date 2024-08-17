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

#include "pluginadapter.h"
#include "pluginmanager.h"
#include "logging.h"
#include "utils.h"
#include "config.h"

using namespace pbnjson;

PluginAdapter::PluginAdapter(PluginManager *_manager, const PluginInfo *_info):
	needUnload(false),
	manager(_manager),
	info(_info),
	plugin(nullptr),
	unloadNotified(false)
{
	//Prepare logging context
	const std::string name = std::string(COMPONENT_NAME) + "-" + this->info->name;
	PmLogErr error = PmLogGetContext(name.c_str(), &this->logContext);

	if (error != kPmLogErr_None)
	{
		LOG_WARNING(MSGID_PMLOG_GETCONTEXT_FAIL, 0 ,
		            "Failed to setup up log context %s, error %d\n", name.c_str(), error);
		//Will use the global context.
		this->logContext = ::logContext;
	}

	//Plugin instance will now be constructed and pluginLoaded called.
}

void PluginAdapter::unloadPlugin()
{
	if (!this->plugin)
	{
		return;
	}

	LOG_DEBUG("Preparing to unload plugin %s", this->info->name.c_str());

	// cleanup pending luna calls
	this->manager->lunaService.cleanupPlugin(this);

	// close alerts
	while (!this->alerts.empty())
	{
		(void) this->closeAlert(this->alerts.begin()->first);
	}

	// cleanup timeouts
	while (!this->timeouts.empty())
	{
		(void) this->cancelTimeout(this->timeouts.cbegin()->first);
	}

	//Cannot delete the plugin here, it might still be in our call stack.
	//Instead set a flag and process later.
	//Need to call manager->processUnload(adapter) when scope out of adapter.
	this->needUnload = true;
}

PluginAdapter::~PluginAdapter()
{
	if (this->plugin)
	{
		delete this->plugin;
		this->plugin = nullptr;
	}
}

void PluginAdapter::pluginLoaded(Plugin *plugin)
{
	if (plugin)
	{
		//First time
		this->plugin = plugin;
		//It's unloaded until we have called startMonitoring
		this->unloadNotified = true;
	}
	else
	{
		//Only if was notified about unloading before
		if (!this->unloadNotified)
		{
			return;
		}
	}

	this->unloadNotified = false;

	LOG_DEBUG("Calling startMonitoring on plugin %s", this->info->path.c_str());

	try
	{
		this->plugin->startMonitoring();
	}
	catch (const std::exception &e)
	{
		LOG_ERROR(MSGID_PLUGIN_EXCEPTION, 0,
		          "Exception while executing startMonitoring in plugin %s, message: %s",
		          this->info->path.c_str(), e.what());
		this->unloadPlugin();
	}
	catch (...)
	{
		LOG_ERROR(MSGID_PLUGIN_EXCEPTION, 0,
		          "Exception while executing startMonitoring in plugin %s",
		          this->info->path.c_str());
		this->unloadPlugin();
	}

	LOG_DEBUG("Done startMonitoring on plugin %s", this->info->path.c_str());
}

void PluginAdapter::setupLogging(PmLogContext *context)
{
	if (!context)
	{
		LOG_ERROR(MSGID_UNLOAD_BAD_PARAMS, 0, "Bad parameters");
		return;
	}

	*context = this->logContext;
}

void PluginAdapter::notifyLocaleChanged(const std::string &locale)
{
	if (!this->plugin)
	{
		//Plugin not loaded at this moment.
		return;
	}

	try
	{
		this->plugin->uiLocaleChanged(locale);
	}
	catch (const std::exception &e)
	{
		LOG_ERROR(MSGID_PLUGIN_EXCEPTION, 0,
		          "Exception while executing uiLocaleChanged in plugin %s, message: %s",
		          this->info->path.c_str(), e.what());
		this->unloadPlugin();
	}
	catch (...)
	{
		LOG_ERROR(MSGID_PLUGIN_EXCEPTION, 0,
		          "Unknown exception while executing uiLocaleChanged in plugin %s",
		          this->info->path.c_str());
		this->unloadPlugin();
	}
}

void PluginAdapter::notifyPluginShouldUnload(const std::string &service)
{
	if (!this->plugin)
	{
		//Ok to unload if not yet loaded.
		return;
	}

	UnloadResult result;

	this->unloadNotified = true;

	LOG_DEBUG("Calling stopMonitoring on plugin %s", this->info->path.c_str());

	try
	{
		result = this->plugin->stopMonitoring(service);
	}
	catch (const std::exception &e)
	{
		LOG_ERROR(MSGID_PLUGIN_EXCEPTION, 0,
		          "Exception while executing stopMonitoring in plugin %s, message: %s",
		          this->info->path.c_str(), e.what());
		result = UNLOAD_OK;
	}
	catch (...)
	{
		LOG_ERROR(MSGID_PLUGIN_EXCEPTION, 0,
		          "Exception while executing stopMonitoring in plugin %s",
		          this->info->path.c_str());
		result = UNLOAD_OK;
	}

	if (result == UNLOAD_OK)
	{
		this->unloadPlugin();
	}

	LOG_DEBUG("Done stopMonitoring on plugin %s", this->info->path.c_str());
}

void PluginAdapter::subscribeToMethod(const std::string &subscriptionId,
                                       const std::string &serviceName,
                                       JValue &params,
                                       SubscribeCallback callback,
                                       const pbnjson::JSchema &schema)
{
	(void) this->unsubscribeFromMethod(subscriptionId);

	LOG_DEBUG("Plugin %s trying to subscribe to method: %s",
	          this->info->name.c_str(),
	          serviceName.c_str());

	//Do not allow to subscribe if the service is not a requirement.
	if (!this->info->containsURI(serviceName))
	{
		LOG_ERROR(MSGID_PLUGIN_INVALID_SUBSCRIBE, 0,
		          "Can only subscribe to services that are in required list, plugin: %s, service: %s",
		          this->info->name.c_str(),
		          serviceName.c_str());
		throw Error("Can only subscribe to services that are in required list");
	}

	SubscribeHandle handle = this->manager->lunaService.subscribeToMethod(
	                             serviceName,
	                             params,
	                             callback,
	                             schema,
	                             this,
	                             false);
	this->subscriptions[subscriptionId] = handle;
}

bool PluginAdapter::unsubscribeFromMethod(const std::string &subscriptionId)
{
	if (this->subscriptions.count(subscriptionId) == 0)
	{
		return false;
	}

	SubscribeHandle handle = this->subscriptions[subscriptionId];
	this->manager->lunaService.cancelSubscribe(handle);
	this->subscriptions.erase(subscriptionId);
	return true;
}

void PluginAdapter::subscribeToSignal(const std::string &subscriptionId,
                                      const std::string &category,
                                      const std::string &method,
                                      SubscribeCallback callback,
                                      const pbnjson::JSchema &schema)
{
	(void) this->unsubscribeFromSignal(subscriptionId);

	LOG_DEBUG("Plugin %s trying to subscribe to signal: %s, method %s",
	          this->info->name.c_str(),
	          category.c_str(),
	          method.c_str());

	JObject params = JObject{{"category", category.c_str()}};
	if (method.length() > 0)
	{
		params.put("method", method.c_str());
	}

	SubscribeHandle handle = this->manager->lunaService.subscribeToMethod(
			"luna://com.webos.service.bus/signal/addmatch",
			params,
			callback,
			schema,
			this,
			true);
	this->subscriptions[subscriptionId] = handle;
}

bool PluginAdapter::unsubscribeFromSignal(const std::string &subscriptionId)
{
	return this->unsubscribeFromMethod(subscriptionId);
}

pbnjson::JValue PluginAdapter::lunaCall(const std::string &serviceUrl,
                                        pbnjson::JValue &params,
                                        unsigned long timeout)
{
	return this->manager->lunaService.call(serviceUrl, params, timeout);
}

void PluginAdapter::lunaCallAsync(const std::string &serviceUrl,
                                  pbnjson::JValue &params,
                                  LunaCallback callback)
{
	this->manager->lunaService.callAsync(serviceUrl, params, callback, this);
}

void PluginAdapter::setTimeout(const std::string &timeoutId,
                               unsigned int timeMs,
                               bool repeat,
                               TimeoutCallback callback)
{
	(void) this->cancelTimeout(timeoutId);

	LOG_DEBUG("Plugin %s set timeout: %s",
	          this->info->name.c_str(),
	          timeoutId.c_str());

	auto timeout = new TimeoutState();

	guint handle = g_timeout_add(timeMs, PluginAdapter::timeoutCallback, timeout);

	if (handle == 0)
	{
		LOG_ERROR(MSGID_ADD_TIMEOUT_FAILED, 0,
		          "Add timeout failed, id: %s, timeout: %u", timeoutId.c_str(), timeMs);
		delete timeout;
		return;
	}

	timeout->adapter = this;
	timeout->callback = callback;
	timeout->timeoutId = timeoutId;
	timeout->repeat = repeat;
	timeout->handle = handle;
	this->timeouts[timeoutId] = timeout;
}

bool PluginAdapter::cancelTimeout(const std::string &timeoutId)
{
	if (this->timeouts.count(timeoutId) == 0)
	{
		return false;
	}

	LOG_DEBUG("Plugin %s cancel timeout: %s",
	          this->info->name.c_str(),
	          timeoutId.c_str());

	TimeoutState *timeout = this->timeouts[timeoutId];
	this->timeouts.erase(timeoutId);
	g_source_remove(timeout->handle);
	delete timeout;
	return true;
}

gboolean PluginAdapter::timeoutCallback(gpointer userData)
{
	TimeoutState *state = reinterpret_cast<TimeoutState *>(userData);
	PluginAdapter *adapter = state->adapter;
	gboolean continueTimeout;

	{
		std::string timeoutId = state->timeoutId;
		TimeoutCallback callback = state->callback;

		LOG_DEBUG("Plugin %s timeout happened: %s",
		          state->adapter->info->name.c_str(),
		          timeoutId.c_str());

		//Do any processing before doing the callback, as it might unload the plugin

		if (state->repeat)
		{
			continueTimeout = G_SOURCE_CONTINUE;
		}
		else
		{
			(void) state->adapter->cancelTimeout(timeoutId);
			continueTimeout = G_SOURCE_REMOVE;
		}

		try
		{
			callback(timeoutId);
		}
		catch (const std::exception &e)
		{
			LOG_ERROR(MSGID_PLUGIN_EXCEPTION, 0,
			          "Exception while executing timeout callback in plugin %s, message: %s",
			          adapter->info->path.c_str(), e.what());
			adapter->unloadPlugin();
		}
		catch (...)
		{
			LOG_ERROR(MSGID_PLUGIN_EXCEPTION, 0,
			          "Exception while executing timeout callback in plugin %s",
			          adapter->info->path.c_str());
			adapter->unloadPlugin();
		}
	}

	adapter->manager->processUnload(adapter);
	return continueTimeout;
}

const std::string PluginAdapter::getUILocale()
{
	return this->manager->getUILocale();
}

const pbnjson::JValue &PluginAdapter::getLocaleInfo()
{
	return this->manager->locale;
}

void PluginAdapter::createToast(
    const std::string &message,
    const std::string &iconUrl,
    const pbnjson::JValue &onClickAction)
{
	JObject params = JObject();
	params.put("message", JValue(message));
	params.put("sourceId", JValue(this->manager->lunaService.servicePath + "-" +
	                              this->info->name));

	if (iconUrl.length() > 0)
	{
		params.put("iconUrl", JValue(iconUrl));
	}

	if (!onClickAction.isNull())
	{
		params.put("onclick", onClickAction);
	}

	this->manager->lunaService.callAsync("luna://com.webos.notification/createToast",
	                                     params,
	                                     nullptr,
	                                     this);
}

void PluginAdapter::createAlert(const std::string &alertId,
                                const std::string &title,
                                const std::string &message,
                                bool modal,
                                const std::string &iconUrl,
                                const pbnjson::JValue &buttons,
                                const pbnjson::JValue &onClose)
{
	(void) this->closeAlert(alertId);

	JObject params = JObject{{"title", JValue(title)},
		{"modal", JValue(modal)},
		{"message", JValue(message)},
		{"buttons", buttons}};

	if (!onClose.isNull())
	{
		params.put("onclose", onClose);
	}

	if (iconUrl.length() > 0)
	{
		params.put("iconUrl", JValue(iconUrl));
	}

	JValue result = this->manager->lunaService.call(
	                    "luna://com.webos.notification/createAlert",
	                    params);

	bool success = false;
	std::string internalId = "";
	ConversionResultFlags jsonError = 0;

	jsonError =  result["returnValue"].asBool(success);
	jsonError |= result["alertId"].asString(internalId);

	if (jsonError || !success || internalId.length() == 0)
	{
		LOG_ERROR(MSGID_CREATE_ALERT_FAILED, 0,
		          "Failed to create alert, plugin %s, params %s, response was %s",
		          this->info->path.c_str(),
		          params.stringify("").c_str(),
		          result.stringify("").c_str());
		throw Error("Failed to create alert");
	}

	this->alerts[alertId] = std::move(internalId);
}

bool PluginAdapter::closeAlert(const std::string &alertId)
{
	if (this->alerts.count(alertId) == 0)
	{
		return false;
	}

	JObject params = JObject{{"alertId", JValue(this->alerts[alertId])}};
	this->alerts.erase(alertId);
	this->manager->lunaService.call("luna://com.webos.notification/closeAlert",
	                                params);
	return true;
}

std::string PluginAdapter::registerMethod(const std::string &category,
                                          const std::string &name,
                                          LunaCallHandler handler,
                                          const pbnjson::JSchema &schema)
{

	if (name.length() == 0)
	{
		throw Error("Name length = 0");
	}
	if (category.length() == 0 || category[0] != '/')
	{
		throw Error("Category needs to start with /");
	}

	MethodInfo* method;
	method = this->manager->lunaService.registerMethod(
			this,
			category,
			name,
			handler,
			schema);

	return method->url;
}
