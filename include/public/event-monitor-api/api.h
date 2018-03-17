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

#ifndef EVENT_MONITOR_INTERNAL_API_H
#define EVENT_MONITOR_INTERNAL_API_H

#include <string>
#include <functional>
#include <PmLogLib.h>
#include <pbnjson.hpp>

#include "error.hpp"

namespace EventMonitor
{

	/**
	 * Current plugin API version. Increment this if any changes are made in this file.
	 */
	const int API_VERSION = 3;

	class Manager;
	class Plugin;

	enum UnloadResult
	{
		UNLOAD_OK = 0,  // Ok to unload
		UNLOAD_CANCEL = 1, // Still have unfinished stuff, I will unload myself later.
	};

	typedef std::function<pbnjson::JValue(const pbnjson::JValue &params)>
	LunaCallHandler;

	typedef std::function<void(pbnjson::JValue &response)> LunaCallback;

	typedef std::function<void(const std::string &timeoutId)> TimeoutCallback;

	/**
	 * Subscribe callback function.
	 * @param previousResponse - response from previous subscribe response.
	 *                           Null JValue if this is the first response.
	 * @param value - the value from current subscribe response.
	 */
	typedef std::function<void(pbnjson::JValue &previousResponse, pbnjson::JValue &response)>
	SubscribeCallback;

	/**
	 * Event monitor public API.
	 */
	class Manager
	{
	public:

		/**
		 * Sets up logging instance for particular plugin.
		 */
		virtual void setupLogging(PmLogContext *context) = 0;

		/**
		 * Returns just the UI locale.
		 */
		virtual const std::string getUILocale() = 0;

		/**
		 * Returns full JSON locale structure.
		 */
		virtual const pbnjson::JValue &getLocaleInfo() = 0;

		/**
		 * Manually unload the plugin.
		 */
		virtual void unloadPlugin() = 0;

		/**
		 * Do a synchronous luna call.
		 * Will throw an exception in case of bus error.
		 * Will return null object if there is no reply within timeout.
		 */
		virtual pbnjson::JValue lunaCall(const std::string &serviceUrl,
		                                 pbnjson::JValue &params,
		                                 unsigned long timeout = 1000) = 0;

		/**
		 * Do a async luna call.
		 */
		virtual void lunaCallAsync(const std::string &serviceUrl,
		                           pbnjson::JValue &params,
		                           LunaCallback callback) = 0;

		/**
		 * Subscribe to luna method.
		 * @parm subscriptionId - subscription identifier - use to unsubscribe or replace existing subscription.
		 * @param methodPath - luna service path to method name to subscribe to, including the luna:// prefix.
		 * @parm params - extra parameters.  Subscribe=true will be added automatically.
		 *                Use JObject() to indicate no parameters.
		 * @param callback - the method to call when
		 */
		virtual void subscribeToMethod(
		    const std::string &subscriptionId,
		    const std::string &methodPath,
		    pbnjson::JValue &params,
		    SubscribeCallback callback,
		    const pbnjson::JSchema &schema = pbnjson::JSchema::AllSchema()) = 0;

		/**
		 * Unsubscribe from luna method.
		 * @param subscriptionId - subscription identifier.
		 * @returns - true if there was a subscription.
		 */
		virtual bool unsubscribeFromMethod(const std::string &subscriptionId) = 0;

		/**
		 * Subscribe to luna signal.
		 * @parm subscriptionId - subscription identifier - use to unsubscribe or replace existing subscription.
		 * @param category - luna signal category.
		 * @parm method - luna method in the category. Leave empty ("") to subscribe
		 *                to all methods.
		 * @param callback - the method to call when signal is fired.
		 */
		virtual void subscribeToSignal(
				const std::string &subscriptionId,
				const std::string &category,
				const std::string &method,
				SubscribeCallback callback,
				const pbnjson::JSchema &schema = pbnjson::JSchema::AllSchema()) = 0;

		/**
		 * Unsubscribe from luna signal.
		 * @param subscriptionId - subscription identifier.
		 * @returns - true if there was a subscription.
		 */
		virtual bool unsubscribeFromSignal(const std::string &subscriptionId) = 0;

		/**
		 * Call a function after a specified time.
		 * @param: timeoutId - string identifier. Note tht setting a new timeout
		 *                     will automatically cancel previous one with the same id.
		 * @param timeMs - time in milliseconds to wait before calling the function.
		 * @param repeat - if true, the callback function will be called repeatedly
		 *                 until cancelTimeout is called.
		 * @param callback - the function to call.
		 */
		virtual void setTimeout(const std::string &timeoutId,
		                        unsigned int timeMs,
		                        bool repeat,
		                        TimeoutCallback callback) = 0;

		/**
		 * Cancel a timeout.
		 * @param - timeout identifier. Same as in setTimeout.
		 * @returns - true if timeout was present.
		 */
		virtual bool cancelTimeout(const std::string &timeoutId) = 0;

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
		                                   const pbnjson::JSchema &schema = pbnjson::JSchema::AllSchema()) = 0;

		/**
		 * Convenience method to create a toast.
		 * Create a toast with optional icon and on click action.
		 * See createAlert API documentation for details.
		 */
		virtual void createToast(
		    const std::string &message,
		    const std::string &iconUrl = "",
		    const pbnjson::JValue &onClickAction = pbnjson::JValue()) = 0;

		/**
		 * Convenience method to create an alert.
		 * See createAlert API documentation for details.
		 */
		virtual void createAlert(const std::string &alertId,
		                         const std::string &title,
		                         const std::string &message,
		                         bool modal,
		                         const std::string &iconUrl,
		                         const pbnjson::JValue &buttons,
		                         const pbnjson::JValue &onClose) = 0;

		/**
		 * Closes the alert specified by id, if open.
		 * @param alertId - alert identifier.
		 * @returns - true of alert was open.
		 */
		virtual bool closeAlert(const std::string &alertId) = 0;
	};

	/**
	 * Plugin interface with event monitor.
	 */
	class Plugin
	{
	public:
		/**
		 * Called when service path goes online, use to subscribe to any relevant services.
		 */
		virtual void startMonitoring() = 0;

		/**
		 * Called when required service goes offline.
		 * @param service - name of the service that went offline.
		 * @return - UNLOAD_OK: active alerts are removed,
		 *               the plugin instance is freed and plugin is unloaded.
		 *         - UNLOAD_CANCEL: no action is taken, plugin continues to receive,
		 *               callbacks from any active alerts. Should unload manually
		 *               when possible by calling manager->unloadPlugin(this);
		 *               Note that the plugin will NOT be reloaded if it's not
		 *               unloaded and service goes back online.
		 */
		virtual UnloadResult stopMonitoring(const std::string &service) = 0;

		/**
		 * Called when system locale is changed. Use to update plugin's resource bundle.
		 */
		virtual void uiLocaleChanged(const std::string &uiLocale) = 0;

		virtual ~Plugin() {};
	};

}; /* Namespace EventMonitor */

#endif //MONITOR_INTERNAL_API_H
