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


#include "mockplugin.h"
#include "config.h"

using namespace pbnjson;
using namespace EventMonitor;

#define TOAST_SOURCE_ID "com.webos.service.eventmonitor-mock-plugin"
#define ALERT_SOURCE_ID "com.webos.service.eventmonitor"
#define ALERT_SERVICE_URI "luna://com.webos.service.eventmonitor/mockPlugin/"

std::string g_alertTimeStampID = "";

const char *requiredServices[] = {"com.webos.applicationManager","com.webos.notification",
                                  nullptr
                                 };

PmLogContext pluginLogContext;

EventMonitor::Plugin *instantiatePlugin(int version,
                                        EventMonitor::Manager *manager)
{
	if (version != EventMonitor::API_VERSION)
	{
		return nullptr;
	}

	return new MockPlugin(manager);
}

MockPlugin::MockPlugin(Manager *_manager):
	PluginBase(_manager, WEBOS_LOCALIZATION_PATH),
	eventsMockPlugin({{"pluginLoaded",false},
		{"subscribedMethod",false},
		{"subscribedSignal",false},
		{"unsubscribed",false},
		{"createdToast",false},
		{"createdAlert",false},
		{"closedAlert",false},
		{"setTimeout",false}})
{
}

MockPlugin::~MockPlugin()
{
	LOG_DEBUG("Destructor called");
}

void MockPlugin::uiLocaleChanged(const std::string &locale)
{
	PluginBase::uiLocaleChanged(locale);

	LOG_DEBUG("Locale set to %s", locale.c_str());

	this->manager->createToast(this->getLocString("Locale set to ") +
	                           locale);
}

void MockPlugin::startMonitoring()
{
	LOG_DEBUG("Starting to monitor");

	eventsMockPlugin["pluginLoaded"] = true;

	(void) this->manager->cancelTimeout("unloadTimeout");

	this->manager->registerMethod(
		"/mockPlugin",
		"getEvents",
		std::bind(&MockPlugin::getEventsCb, this));

	this->manager->createToast(
	    this->getLocString("Mock plugin started, will show alert in 2 seconds"));

	this->manager->setTimeout("startAlert",
	                          2000,
	                          false,
	                          std::bind(&MockPlugin::startAlert, this, std::placeholders::_1));

	JValue params = JObject();
	this->manager->subscribeToMethod(
			"foregroundApp",
			"luna://com.webos.applicationManager/getForegroundAppInfo",
			params,
			std::bind(&MockPlugin::foregroundAppCallback, this,  std::placeholders::_1, std::placeholders::_2));

	this->manager->subscribeToMethod(
			"toastNotification",
			"luna://com.webos.notification/getToastNotification",
			params,
			std::bind(&MockPlugin::toastNotificationCallback, this,  std::placeholders::_1, std::placeholders::_2));

	this->manager->subscribeToMethod(
			"alertNotification",
			"luna://com.webos.notification/getAlertNotification",
			params,
			std::bind(&MockPlugin::alertNotificationCallback, this,  std::placeholders::_1, std::placeholders::_2));

	// You can subscribe to signals even when the appropriate service is not started.
	this->manager->subscribeToSignal(
			"batteryStatus",
			"/com/palm/power",
			"batteryStatus",
			std::bind(&MockPlugin::batteryStatusCallback, this,  std::placeholders::_1, std::placeholders::_2));

	this->manager->subscribeToSignal(
			"processFinished",
			"/booster",
			"processFinished",
			std::bind(&MockPlugin::boosterFinishedCallback, this, std::placeholders::_1, std::placeholders::_2));
}

void MockPlugin::foregroundAppCallback(JValue& previousValue,
                                       JValue& value)
{
	eventsMockPlugin["subscribedMethod"] = true;

	if (previousValue.isNull())
	{
		return;
	}

	std::string prevApp;
	std::string curApp;
	previousValue["appId"].asString(prevApp);
	value["appId"].asString(curApp);

	LOG_DEBUG("Foreground app callback: %s", value.stringify("").c_str());

	if (prevApp != curApp)
	{
		this->manager->createToast(this->getLocString("Active application changed to ") + curApp);
	}
}

UnloadResult MockPlugin::stopMonitoring(const std::string &service)
{
	LOG_DEBUG("Stopping plugin");

	this->manager->createToast(this->getLocString("Required services unloaded, waiting 5 seconds to unload the plugin."));

	// Try using a lambda function.
	auto timeoutCallback = [this](const std::string & id)
	{
		LOG_DEBUG("Timout finished, toasting");
		this->manager->createToast(this->getLocString("5 seconds passed, unloading plugin"));
		LOG_DEBUG("Timout finished, unloading plugin");
		this->manager->unloadPlugin();
	};

	this->manager->setTimeout("unloadTimeout", 5000, false, timeoutCallback);

	LOG_DEBUG( "Stopping plugin - done");
	return UNLOAD_CANCEL;
}

void MockPlugin::startAlert(const std::string &timeoutId)
{
	eventsMockPlugin["setTimeout"] = true;

	std::string actionUrl = this->manager->registerMethod(
			"/mockPlugin",
			"action",
			std::bind(&MockPlugin::actionCallback, this,  std::placeholders::_1));

	JValue buttons = JArray
	{
		JObject{{"label", "close"},
		        {"onclick", actionUrl},
		        {"position", "left"},
		        {"params", JObject{{"close",true}}}},
		JObject{{"label", "toast"},
		        {"onclick", actionUrl},
		        {"params", JObject{{"close",false},{"toast",this->getLocString("toast")}}}}
	};

	JValue onClose = JObject();

	this->manager->createAlert("question",
	                           this->getLocString("Event Monitor Mock plugin started"),
	                           this->getLocString(
	                               "Do you see this alert? "
	                               "I will show toasts whenever active application is changed."
	                               "<br>Closing the alert in 20 seconds. "),
	                           false,
	                           "",
	                           buttons,
	                           onClose);

	auto timeoutCallback = [this](const std::string & id)
	{
		this->manager->closeAlert("question");
		this->manager->createToast(
		    this->getLocString("Alert closed after 20 seconds"));
	};

	this->manager->setTimeout("closeQuestion",
	                          10000,
	                          false,
	                          timeoutCallback);
}

pbnjson::JValue MockPlugin::actionCallback(const pbnjson::JValue &params)
{
	(void) this->manager->cancelTimeout("closeQuestion");

	std::string message = "";
	bool closeAlert = false;
	auto wasError = params["close"].asBool(closeAlert);
	// Toast is optional parameter.
	auto toastMissing = params["toast"].asString(message);

	if (wasError)
	{
		return JObject{{"returnValue", false},
		               {"errorCode", 100},
		               {"errorMessage", "Error parsing JSON"}};
	}

	if (toastMissing)
	{
		this->manager->createToast(
				this->getLocString("Button with no message"));
	}
	else
	{
		this->manager->createToast(
				this->getLocString("Button said ") + message);
	}

	if (!closeAlert)
	{
		this->manager->setTimeout("startAlert",
		                          100,
		                          false,
		                          std::bind(&MockPlugin::startAlert, this, std::placeholders::_1));
	}

	return JObject{{"returnValue", true}};
}

pbnjson::JValue MockPlugin::getEventsCb()
{
	return JObject{{"pluginLoaded", eventsMockPlugin["pluginLoaded"]},
	               {"subscribedMethod", eventsMockPlugin["subscribedMethod"]},
	               {"subscribedSignal", eventsMockPlugin["subscribedSignal"]},
	               {"unsubscribed", eventsMockPlugin["unsubscribed"]},
	               {"createdToast", eventsMockPlugin["createdToast"]},
	               {"createdAlert", eventsMockPlugin["createdAlert"]},
	               {"closedAlert", eventsMockPlugin["closedAlert"]},
	               {"setTimeout", eventsMockPlugin["setTimeout"]},
	               {"returnValue", true}
	};
}
void MockPlugin::batteryStatusCallback(pbnjson::JValue& previousValue,
                                       pbnjson::JValue& value)
{
	this->manager->createToast("Battery status callback");

	int percent = 0;
	auto wasError = value["percent"].asNumber(percent);

	if (wasError)
	{
		return;
	}

	eventsMockPlugin["subscribedSignal"] = true;
	char buf[1000];
	snprintf(buf,
	         1000,
	         this->getLocString("Battery Status update: percent: %d").c_str(),
	         percent);
	this->manager->createToast(buf);
}

void MockPlugin::boosterFinishedCallback(pbnjson::JValue& previousValue,
                                       pbnjson::JValue& value)
{
	int exitCode = 0;
	auto wasError = value["exitCode"].asNumber(exitCode);

	if (wasError)
	{
		return;
	}

	eventsMockPlugin["subscribedSignal"] = true;
	auto boosterTimeoutCallback = [this,exitCode](const std::string & id)
	{
		this->manager->createToast(this->getLocString("Signal received. Boosted QML app terminated with exitcode: ") + to_string(exitCode));
	};

	this->manager->setTimeout("boosterTimer",
	                          5000,
	                          false,
	                          boosterTimeoutCallback);

	auto unsubscribeTimeoutCallback = [this](const std::string & id)
	{
		this->manager->unsubscribeFromMethod("foregroundApp");
		this->manager->unsubscribeFromSignal("batteryStatus");
		this->manager->unsubscribeFromSignal("processFinished");
		this->manager->createToast(this->getLocString("Unsubscribed from signals and methods"));
		eventsMockPlugin["subscribedSignal"] = false;
		eventsMockPlugin["subscribedMethod"] = false;
		eventsMockPlugin["unsubscribed"] = true;
	};

	this->manager->setTimeout("unsubscribeTimer",
	                          10000,
	                          false,
	                          unsubscribeTimeoutCallback);

}
void MockPlugin::toastNotificationCallback(pbnjson::JValue& previousValue,
	                                       pbnjson::JValue& value)
{
	std::string toastSourceId = "";
	auto wasError = value["sourceId"].asString(toastSourceId);

	if (wasError)
	{
		return;
	}

	if (toastSourceId == TOAST_SOURCE_ID)
		eventsMockPlugin["createdToast"] = true;
}

void MockPlugin::alertNotificationCallback(pbnjson::JValue& previousValue,
	                                       pbnjson::JValue& value)
{
	std::string alertSourceID = "";
	std::string alertServiceURI = "";
	std::string alertAction = "";
	std::string alertTimeStamp = "";

	auto wasError = value["alertAction"].asString(alertAction);

	if (wasError)
	{
		return;
	}

	if (alertAction == "close")
	{
		wasError |= value["alertInfo"]["timestamp"].asString(alertTimeStamp);

		if (wasError)
		{
			return;
		}

		if (alertTimeStamp == g_alertTimeStampID)
			eventsMockPlugin["closedAlert"] = true;
	}

	wasError |= value["alertInfo"]["sourceId"].asString(alertSourceID);

	if (wasError)
	{
		return;
	}

	if (value["alertInfo"]["buttons"].isArray())
	{
		for (auto buttonInfo: value["alertInfo"]["buttons"].items())
		{
			wasError |= buttonInfo["action"]["serviceURI"].asString(alertServiceURI);
			if (wasError)
			{
				return;
			}
		}
	}

	if (alertSourceID == ALERT_SOURCE_ID && alertServiceURI == ALERT_SERVICE_URI && alertAction == "open")
	{
		eventsMockPlugin["createdAlert"] = true;
		wasError |= value["timestamp"].asString(g_alertTimeStampID);
		if (wasError)
		{
			return;
		}
	}
}
