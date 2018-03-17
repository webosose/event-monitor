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

#include "pluginmanager.h"
#include "logging.h"

PluginManager::PluginManager(PluginLoader &_loader,
                             LunaService &_lunaService,
                             GMainLoop *_mainLoop):
	lunaService(_lunaService),
	mainLoop(_mainLoop),
	loader(_loader)
{
}

PluginManager::~PluginManager()
{
	while (this->activePlugins.size() > 0)
	{
		PluginAdapter *adapter = this->activePlugins.begin()->second;
		adapter->unloadPlugin();
		this->processUnload(adapter);
	}
}

/**
 * Called by lunaMonitor.
 */
void PluginManager::loadPlugin(const PluginInfo *info,
                               const std::string &service)
{
	if (this->isPluginLoaded(info))
	{
		PluginAdapter *adapter = this->activePlugins[info->path];
		adapter->pluginLoaded(nullptr);
		this->processUnload(adapter);
	}
	else // new plugin
	{
		PluginAdapter *adapter = new PluginAdapter(this, info);
		Plugin *plugin = this->loader.loadPlugin(info, adapter);

		if (!plugin)
		{
			//Plugin loader method successfully called but returned null.
			// Most likely API incompatibility.
			LOG_ERROR(MSGID_PLUGIN_LOAD_FAILED,
			          0,
			          "Plugin %s instantiatePlugin returned NULL",
			          info->name.c_str());

			this->loader.unloadPlugin(info);
			delete adapter;
			return;
		}

		this->activePlugins[info->path] = adapter;
		adapter->pluginLoaded(plugin);
		this->processUnload(adapter);
	}
}

bool PluginManager::isPluginLoaded(const PluginInfo *pluginInfo)
{
	return this->activePlugins.count(pluginInfo->path) > 0;
}

void PluginManager::processUnload(PluginAdapter *adapter)
{
	if (!adapter->needUnload)
	{
		return;
	}

	const PluginInfo *info = adapter->getInfo();
	this->activePlugins.erase(info->path);

	adapter->unloadPlugin();
	delete adapter;

	//Unload the shared library, must happen after deleting the adapter
	this->loader.unloadPlugin(info);
}

void PluginManager::notifyPluginShouldUnload(const PluginInfo *pluginInfo,
        const std::string &serviceName)
{
	if (!this->isPluginLoaded(pluginInfo))
	{
		return;
	}

	PluginAdapter *adapter = this->activePlugins[pluginInfo->path];
	adapter->notifyPluginShouldUnload(serviceName);
	this->processUnload(adapter);
}

void PluginManager::notifyLocaleChanged(const pbnjson::JValue &locale)
{
	this->locale = locale;

	std::string localeStr = this->getUILocale();
	locale.asString(localeStr);

	for (auto iter = this->activePlugins.begin();
	        iter != this->activePlugins.end(); /* no increment */)
	{
		PluginAdapter *adapter = iter->second;
		adapter->notifyLocaleChanged(localeStr);

		iter ++;
		this->processUnload(adapter);
	}
}

const std::string PluginManager::getUILocale()
{
	std::string uiLocaleStr = "en-US";
	ConversionResultFlags resultFail = this->locale["locales"]["UI"].asString(
	                                       uiLocaleStr);

	if (resultFail)
	{
		LOG_ERROR(MSGID_LOCALE_ERROR, 0, "Could not parse ui locale: %s.",
		          this->locale.stringify().c_str());
	}

	return uiLocaleStr;
}
