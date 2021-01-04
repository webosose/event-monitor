// Copyright (c) 2015-2021 LG Electronics, Inc.
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

#include <dlfcn.h>
#include <event-monitor-api/api.h>

#include "pluginloader.h"
#include "logging.h"
#include "utils.h"

PluginLoader::PluginLoader(const std::string &_pluginPath):
	pluginPath(_pluginPath)
{

	LOG_INFO(MSGID_PLUGIN_LOADER, 0 , "Loooking for plugins in %s",
	         this->pluginPath.c_str());

	GError *error = nullptr;
	std::unique_ptr<GDir, std::function<void(GDir *)>> dir { g_dir_open(this->pluginPath.c_str(), 0, &error), g_dir_close};

	if (error)
	{
		LOG_ERROR(MSGID_PLUGIN_LOADER, 0,
		          "Failed to open plugin directory: %s, error: %s", this->pluginPath.c_str(),
		          error->message);
		g_error_free(error);
		return; // No plugins
	}

	while (const gchar *filename = g_dir_read_name(dir.get()))
	{
		if (!g_str_has_suffix(filename, ".so"))
		{
			continue;
		}

		std::string filePath = g_build_path("/", this->pluginPath.c_str(), filename,
		                                    NULL);
		LOG_INFO(MSGID_PLUGIN_LOADER, 0, "Loading file: %s", filePath.c_str());

		std::unique_ptr<void, std::function<void(void *)>> handle { dlopen(filePath.c_str(), RTLD_NOW), dlclose};

		if (!handle.get())
		{
			LOG_CRITICAL(MSGID_PLUGIN_LOADER, 0, "Failed to load plugin file: %s",
			             filePath.c_str());
			continue;
		}

		auto services = reinterpret_cast<const char **>(dlsym(handle.get(),
		                "requiredServices"));
		auto instantiateFunc =
		    reinterpret_cast<EventMonitor::Plugin* (*)(int, EventMonitor::Manager *)>(dlsym(
		                handle.get(), "instantiatePlugin"));

		if (!services || !instantiateFunc)
		{
			LOG_CRITICAL(MSGID_PLUGIN_LOADER, 0,
			             "Failed to find plugin methods, requiredServices and instantiatePlugin.");
			continue;
		}

		//strip the .so part
		std::string name{filename, strlen(filename) - 3};

		PluginInfo info;
		info.name = name;
		info.path = std::string(filePath);
		info.dlHandle = nullptr;

		for (size_t pos = 0; services[pos]; pos += 1)
		{
			info.requiredServices.push_back(std::string(services[pos]));
		}

		this->plugins.push_back(info);
	}

};

const std::vector<PluginInfo> *PluginLoader::getPlugins()
{
	return &this->plugins;
}

Plugin *PluginLoader::loadPlugin(const PluginInfo *info, Manager *manager)
{
	LOG_INFO(MSGID_PLUGIN_LOADED, 0, "Loading plugin %s", info->path.c_str());

	void *handle = dlopen(info->path.c_str(), RTLD_NOW);

	if (!handle)
	{
		LOG_CRITICAL(MSGID_PLUGIN_LOADER, 0, "Failed to load plugin file: %s",
		             info->path.c_str());
		throw EventMonitor::Error("Failed to load plugin");
	}

	auto instantiateFunc = reinterpret_cast<Plugin* (*)(int, Manager *)>(dlsym(
	                           handle, "instantiatePlugin"));

	if (!instantiateFunc)
	{
		dlclose(handle);
		LOG_CRITICAL(MSGID_PLUGIN_LOADER, 0,
		             "Failed to find plugin method instantiatePlugin.");
		throw EventMonitor::Error("Failed to load plugin");
	}

	Plugin *plugin;

	try
	{
		plugin = instantiateFunc(API_VERSION, manager);
	}
	catch (...)
	{
		//FIXME: should do this with unique_ptr
		dlclose(handle);
		throw;
	}

	const_cast<PluginInfo *>(info)->dlHandle = handle;

	return plugin;
}

void PluginLoader::unloadPlugin(const PluginInfo *info)
{
	LOG_INFO(MSGID_PLUGIN_UNLOADED, 0, "Unloading plugin %s", info->path.c_str());

	if (!info->dlHandle)
	{
		return;
	}

	dlclose(info->dlHandle);
	const_cast<PluginInfo *>(info)->dlHandle = nullptr;
}
