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
#include <event-monitor-api.h>

extern PmLogContext pluginLogContext;

namespace EventMonitor{

/**
 * Convenience base class.
 * Initializes logging and localization.
 * Use this->getLocString for localization.
 */
class PluginBase: public EventMonitor::Plugin
{
public:
	PluginBase(EventMonitor::Manager *_manager,
	           const char* _localizationPath):
		manager(_manager),
		resourceBundle(nullptr),
	    localizationPath(_localizationPath)
	{
		this->manager->setupLogging(&pluginLogContext);
		this->uiLocaleChanged(this->manager->getUILocale());
	}

	virtual ~PluginBase()
	{
		this->manager = nullptr;
	}


	const std::string getLocString(const std::string& source){
		return this->resourceBundle->getLocString(source);
	}

	const std::string getLocString(const std::string& key,
	                               const std::string& source){
		return this->resourceBundle->getLocString(key, source);
	}

	void uiLocaleChanged(const std::string &uiLocale){
		this->resourceBundle.reset(new ResBundle(
				manager->getUILocale().c_str(),
				"cppstrings.json",
				this->localizationPath));
	}

protected:
	EventMonitor::Manager *manager;
	std::unique_ptr<ResBundle> resourceBundle;
	const char * localizationPath;
};


}