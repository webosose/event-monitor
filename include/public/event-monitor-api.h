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

#ifndef EVENT_MONITOR_API_H
#define EVENT_MONITOR_API_H

#include <PmLogLib.h>
#include "event-monitor-api/api.h"
#include "event-monitor-api/logging.h"
#include "event-monitor-api/error.hpp"
#include "event-monitor-api/pluginbase.hpp"

/**
 * Array of luna service paths that are required before the plugin can be instantiated.
 * A single plugin can have multiple services as a requirement.
 */
extern "C" const char *requiredServices[];

/**
 * @brief  Factory method to create new plugin instance.
 *         The result must be an subclass of Plugin.
 *         This method wil be called only once.
 *         The plugin will be freed by the caller.
 *         After that the plugin shared library will be unloaded.
 *
 *
 * @param Version of the API to use, the current api version is available in
 *        EventMonitor::API_VERSION
 */
extern "C" EventMonitor::Plugin *instantiatePlugin(int version,
        EventMonitor::Manager *manager);


/**
 * Log context forward declaration. To use logging, plugin must declare variable
 * for it and call manager->setupLogging.
 */
extern PmLogContext pluginLogContext;


#endif //MONITOR_PLUGINAPI_H
