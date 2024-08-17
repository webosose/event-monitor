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

#include <functional>

#include "lunaservice.h"
#include "pluginadapter.h"
#include "pluginmanager.h"
#include "logging.h"

using namespace pbnjson;
using namespace EventMonitor;

LunaService::LunaService(std::string _servicePath, GMainLoop *mainLoop,
                         const char *identifier):
	LS::Handle(_servicePath.c_str(), identifier),
	servicePath(_servicePath)
{
	this->setDisconnectHandler(LunaService::onLunaDisconnect, this);
	this->attachToLoop(mainLoop);
}

LunaService::~LunaService()
{
	//Cleanup the subscriptions
	for (auto i : this->subscriptions)
	{
		SubscriptionInfo* subscription = i.first;
		subscription->call.cancel();
		delete subscription;
	}

	// Cleanup the methods
	for (auto cat : this->categoryMethods)
	{
		for(const auto& method: cat.second)
		{
			delete method.second;
		}
	}

	this->categoryMethods.clear();
}

JValue LunaService::call(const std::string &serviceUrl,
                         JValue &params,
                         unsigned long timeout)
{
	std::string paramsStr;

	if (params.getType() != JValueType::JV_OBJECT)
	{
		paramsStr = R"({})";
	}
	else
	{
		paramsStr = params.stringify();
	}

	LOG_DEBUG("Luna call %s params %s", serviceUrl.c_str(), paramsStr.c_str());

	try
	{
		LS::Call call = this->callOneReply(serviceUrl.c_str(), paramsStr.c_str());
		LS::Message reply = call.get(timeout);

		if (!reply)
		{
			LOG_ERROR(MSGID_LS2_CALL_NO_REPLY, 0, "Luna call %s has no reply within timeout %lu", serviceUrl.c_str(), timeout);
			return JValue(); // Return null value.
		}

		LOG_DEBUG("Call result %s: %s", serviceUrl.c_str(), reply.getPayload());

		JValue value = JDomParser::fromString(reply.getPayload(), JSchema::AllSchema());

		if (!value.isValid())
		{
			LOG_ERROR(MSGID_LS2_RESPONSE_PARSE_ERROR, 0, "Failed to parse luna reply: %s",
			          reply.getPayload());
		}
		else if (!value.isObject())
		{
			LOG_ERROR(MSGID_LS2_RESPONSE_NOT_AN_OBJECT, 0,
			          "Luna reply not an JSON object: %s", reply.getPayload());
		}

		return value;
	}
	catch (const LS::Error &error)
	{
		LOG_ERROR(MSGID_LS2_FAILED_TO_SUBSCRIBE, 0, "Failed to call %s, params %s" ,
		          serviceUrl.c_str(), paramsStr.c_str());
		throw;
	}
}

CallHandle LunaService::callAsync(const std::string &serviceUrl,
                                  pbnjson::JValue &params,
                                  LunaCallback callback,
                                  PluginAdapter *plugin)
{
	std::string paramsStr;

	if (params.getType() != JValueType::JV_OBJECT)
	{
		paramsStr = R"({})";
	}
	else
	{
		paramsStr = params.stringify();
	}

	LOG_DEBUG("Call async to %s params %s", serviceUrl.c_str(), paramsStr.c_str());

	SubscriptionInfo *info = nullptr;

	try
	{
		if (!callback)
		{
			//Call and forget
			this->callOneReply(serviceUrl.c_str(), paramsStr.c_str());
			return nullptr;
		}
		else
		{
			info = new SubscriptionInfo(JSchema::AllSchema());
			this->subscriptions[info] = info;

			info->service = this;
			info->call = this->callMultiReply(serviceUrl.c_str(),
			                                  paramsStr.c_str());
			info->simpleCallback = callback;
			info->counter = 0;
			info->plugin = plugin;
			info->serviceUrl = serviceUrl;
			info->previousValue = JValue(); // Null value
			info->call.continueWith(LunaService::callResultHandler, info);
			return info;
		}
	}
	catch (const LS::Error &error)
	{
		LOG_ERROR(MSGID_LS2_FAILED_TO_SUBSCRIBE, 0, "Failed to call %s, params %s" ,
		          serviceUrl.c_str(), paramsStr.c_str());
		delete info;
		throw;
	}
}

MethodInfo* LunaService::registerMethod(PluginAdapter *plugin,
                                        const std::string &category,
                                        const std::string &methodName,
                                        EventMonitor::LunaCallHandler handler,
                                        const pbnjson::JSchema &schema)
{
	if (!plugin)
	{
		throw Error("Plugin == null");
	}

	MethodInfo *info = this->findMethod(category, methodName);

	if (info && info->plugin && info->plugin != plugin)
	{
		throw Error("Method already registered for different plugin. Cross-plugin method override not allowed.");
	}

	if (info == nullptr)
	{
		/* Register new method */
		LSMethod methods[] =
				{
						{
								methodName.c_str(),
								&LS::Handle::methodWraper<LunaService, &LunaService::methodHandler>,
								LUNA_METHOD_FLAGS_NONE
						},
						nullptr
				};
		this->registerCategoryAppend(category.c_str(), methods, nullptr);
		this->setCategoryData(category.c_str(), this);

		info = new MethodInfo();
		this->categoryMethods[category][methodName] = info;
		info->url = "luna://" + this->servicePath + category + "/" + methodName;
	}

	info->plugin = plugin;
	info->handler = handler;
	info->schema = schema;
	return info;
}

bool LunaService::methodHandler(LSMessage &msg)
{
	LS::Message request{&msg};
	std::string methodName = request.getMethod();
	std::string categoryName = request.getCategory();

	LOG_DEBUG("Luna method called  %s/%s: %s", categoryName.c_str(), methodName.c_str(),
	          request.getPayload());

	MethodInfo* method = this->findMethod(categoryName, methodName);

	if (!method || method->plugin == nullptr || method->handler == nullptr)
	{
		/** Most likely plugin unloaded. */
		LOG_DEBUG("No handler for method call");
		request.respond(
				R"({"returnValue":false, "errorCode":1, "errorMessage":"Method removed."})");
		return true;
	}

	JValue value = JDomParser::fromString(request.getPayload(), method->schema);

	if (!value.isValid())
	{
		LOG_ERROR(MSGID_LS2_RESPONSE_PARSE_ERROR, 0,
		          "Failed to validate luna request against schema: %s, error: %s",
		          request.getPayload(),
		          value.errorString().c_str());
		request.respond(
		    R"({"returnValue":false, "errorCode":2, "errorMessage":"Failed to validate request against schema"})");
		return true;
	}
	else
	{
		LOG_DEBUG("Calling method handler");
		JValue result = method->handler(value);
		request.respond(result.stringify("").c_str());

		//FIXME: plugin unloading should be decoupled from luna service.
		method->plugin->manager->processUnload(method->plugin);
		return true;
	}
}

SubscribeHandle LunaService::subscribeToMethod(const std::string &serviceUrl,
        JValue &params,
        SubscribeCallback callback,
        const pbnjson::JSchema &schema,
        PluginAdapter *plugin,
		bool checkFirstResponse)
{
	std::string paramsStr;

	if (params.getType() != JValueType::JV_OBJECT)
	{
		paramsStr = R"({"subscribe":true})";
	}
	else
	{
		params.put("subscribe", JValue(true));
		paramsStr = params.stringify();
	}

	LOG_DEBUG("Subscribing to %s params %s", serviceUrl.c_str(), paramsStr.c_str());

	SubscriptionInfo *info = new SubscriptionInfo(schema);

	try
	{
		this->subscriptions[info] = info;

		info->service = this;
		info->call = this->callMultiReply(serviceUrl.c_str(),
		                                  paramsStr.c_str());

		if (checkFirstResponse)
		{
			LS::Message reply = info->call.get(1000);

			if (!reply)
			{
				LOG_ERROR(MSGID_LS2_CALL_NO_REPLY, 0, "Luna call %s has no reply within timeout %d", serviceUrl.c_str(), 1000);
				delete info;
				info = nullptr;
				throw Error("SubscribeToMethod: No luna call response within 1000 ms");
			}

			LOG_DEBUG("Subscribe first result %s: %s", serviceUrl.c_str(), reply.getPayload());

			// First response is not validated against the shcema.
			// As it's frequently in different format than the subscribe responses.
			JValue value = JDomParser::fromString(reply.getPayload(), JSchema::AllSchema());
			bool success = false;
			ConversionResultFlags error = value["returnValue"].asBool(success);

			if (error)
			{
				LOG_ERROR(MSGID_LS2_RESPONSE_PARSE_ERROR, 0, "Failed to parse returnValue in first response: %s",
				          reply.getPayload());
				delete info;
				info = nullptr;
				throw Error("Failed to parse returnValue in first response");
			}
			else if (!success)
			{
				LOG_ERROR(MSGID_LS2_FIRST_RESPONSE_ERROR, 0, "First response failed: %s",
				          reply.getPayload());
				delete info;
				info = nullptr;
				throw Error("First response failed");
			}

			LOG_DEBUG("Subscribe first result success");
		}

		info->subscribeCallback = callback;
		info->counter = 0;
		info->plugin = plugin;
		info->serviceUrl = serviceUrl;
		info->previousValue = JValue(); // Null value
		info->call.continueWith(LunaService::callResultHandler, info);
		LOG_DEBUG("Subscribe successful");

		return info;
	}
	catch (const LS::Error &error)
	{
		LOG_ERROR(MSGID_LS2_FAILED_TO_SUBSCRIBE, 0,
		          "Failed to subscribe %s, params %s" , serviceUrl.c_str(), paramsStr.c_str());
		delete info;
		info = nullptr;
		throw;
	}
}

void LunaService::cancelSubscribe(SubscriptionInfo *info)
{
	if (this->subscriptions.count(info) > 0)
	{
		LOG_DEBUG("Canceling subscribe to %s", info->serviceUrl.c_str());
		info->call.cancel();
		delete info;
		this->subscriptions.erase(info);
	}
}

void LunaService::cleanupPlugin(PluginAdapter *plugin)
{
	// Erase all subscriptions associated with the plugin
	for (auto iter = this->subscriptions.cbegin();
	        iter != this->subscriptions.cend(); /* no increment */)
	{
		if (iter->first->plugin == plugin)
		{
			SubscriptionInfo *info = iter->first;
			info->call.cancel();
			delete info;
			this->subscriptions.erase(iter++);
		}
		else
		{
			++iter;
		}
	}

	// Set plugin to null for all methods associated with the plugin.
	// Note that we cannot remove methods from bus, instead the method
	// handler will return a generic error - method removed.
	for (auto cat : this->categoryMethods)
	{
		for (const auto& methodIter: cat.second)
		{
			if (methodIter.second->plugin == plugin)
			{
				MethodInfo *info = methodIter.second;
				info->plugin = nullptr;
				info->handler = nullptr;
			}
		}
	}
}

bool LunaService::callResultHandler(LSHandle *handle UNUSED_VAR,
                                    LSMessage *message,
                                    void *context)
{
	auto info = reinterpret_cast<SubscriptionInfo *>(context);

	LunaService *service = info->service;

	if (service->subscriptions.count(info) == 0)
	{
		//Should not ever happen, but anyway. At this point info is most likely
		//Freed memory.
		LS::Message reply{message};
		LOG_CRITICAL(MSGID_LS2_HUB_ERROR, 0,
		             "No subscription info for subscription reply from %s", reply.getSender());
		return false;
	}

	return info->service->callResult(info, message);
}

bool LunaService::callResult(SubscriptionInfo *info, LSMessage *message)
{
	LS::Message reply{message};

	if (reply.isHubError())
	{
		LOG_INFO(MSGID_LS2_HUB_ERROR, 0, "Luna hub error, service %s",
		         info->serviceUrl.c_str());
		this->cancelSubscribe(info);
		return false;
	}

	LOG_DEBUG("Subscribe callback %s: %s", info->serviceUrl.c_str(),
	          reply.getPayload());

	JValue value = JDomParser::fromString(reply.getPayload(), JSchema::AllSchema());
	JResult validation = info->schema.validate(value);

	if (!value.isValid())
	{
		LOG_ERROR(MSGID_LS2_RESPONSE_PARSE_ERROR, 0, "Failed to parse luna reply: %s",
		          reply.getPayload());
	}
	else if (!value.isObject())
	{
		LOG_ERROR(MSGID_LS2_RESPONSE_NOT_AN_OBJECT, 0,
		          "Luna reply not an JSON object: %s", reply.getPayload());
	}
	else if (validation.isError())
	{
		LOG_ERROR(MSGID_LS2_RESPONSE_SCHEMA_ERROR, 0,
		          "Failed to validate against schema: %s, schema: %s", reply.getPayload(), validation.errorString().c_str());
		return false;
	}
	else
	{
		PluginAdapter *plugin = info->plugin;

		if (info->simpleCallback)
		{
			LunaCallback callback = info->simpleCallback;
			this->cancelSubscribe(info);
			//Callback always last as it can change the state or even delete everyting
			callback(value);
		}
		else if (info->subscribeCallback)
		{
			info->counter += 1;
			JValue previousValue = info->previousValue;
			info->previousValue = value;
			//Callback always last as it can change the state or even delete info
			info->subscribeCallback(previousValue, value);
		}

		//FIXME: not nice calling it from here. Luna service should be decoupled
		// from plugins
		if (plugin)
		{
			plugin->manager->processUnload(plugin);
		}
	}

	return true;
}


void LunaService::onLunaDisconnect(LSHandle *handle UNUSED_VAR, void *data)
{
	auto service = reinterpret_cast<LunaService *>(data);

	LOG_INFO(MSGID_LS2_DISCONNECTED, 0, "Luna service disconnected.");
	std::cout << "LS disconnect";
	service->detach(); //Detaching from mainloop should cause it to stop, terminating the application.
}
