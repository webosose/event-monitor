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

#include <PmLogLib.h>

extern PmLogContext logContext;

#define LOG_CRITICAL(msgid, kvcount, ...) \
    PmLogCritical(::logContext, msgid, kvcount, ##__VA_ARGS__)

#define LOG_ERROR(msgid, kvcount, ...) \
    PmLogError(::logContext, msgid, kvcount,##__VA_ARGS__)

#define LOG_WARNING(msgid, kvcount, ...) \
    PmLogWarning(::logContext, msgid, kvcount, ##__VA_ARGS__)

#define LOG_INFO(msgid, kvcount, ...) \
    PmLogInfo(::logContext, msgid, kvcount, ##__VA_ARGS__)

#define LOG_DEBUG(fmt, ...) \
    PmLogDebug(::logContext, "%s:%s() " fmt, __FILE__, __FUNCTION__, ##__VA_ARGS__)


#define MSGID_ERROR_INTERNAL                        "INTERNAL_ERROR"
#define MSGID_PLUGIN_EXCEPTION                      "PLUGIN_EXCEPTION"

#define MSGIC_TERMINATING                           "TERMINATING"

#define MSGID_LS2_FAILED_TO_SUBSCRIBE               "LS2_FAILED_TO_SUBSCRIBE"
#define MSGID_LS2_FAILED_TO_SEND                    "LS2_FAILED_TO_SEND"
#define MSGID_LS2_DISCONNECTED                      "LS2_DISCONNECTED"
#define MSGID_LS2_HUB_ERROR                         "LS2_HUB_ERROR"
#define MSGID_LS2_NOT_SUBSCRIBED                    "LS2_NOT_SUBSCRIBED"
#define MSGID_LS2_RESPONSE_PARSE_ERROR              "LS2_RESPONSE_PARSE_ERROR"
#define MSGID_LS2_RESPONSE_SCHEMA_ERROR             "LS2_RESPONSE_SCHEMA_ERROR"
#define MSGID_LS2_RESPONSE_NOT_AN_OBJECT            "LS2_RESPONSE_NOT_AN_OBJECT"
#define MSGID_LS2_FIRST_RESPONSE_ERROR              "LS2_FIRST_RESPONSE_ERROR"
#define MSGID_LS2_CALL_NO_REPLY                     "LS2_CALL_NO_REPLY"

#define MSGID_SETTINGS_LOCALE_MISSING               "SETTINGS_LOCALE_MISSING"

#define MSGID_PMLOG_GETCONTEXT_FAIL                 "PMLOG_FAILED_TO_GET_CONTEXT"
#define MSGID_UNLOAD_BAD_PARAMS                     "UNLOAD_BAD_PARAMS"

#define MSGID_PLUGIN_LOADER                         "PLUGIN_LOADER"
#define MSGID_PLUGIN_LOAD_FAILED                    "LOAD_PLUGIN_FAILED"
#define MSGID_PLUGIN_ADDED                          "PLUGIN_ADDED"
#define MSGID_PLUGIN_LOADED                         "PLUGIN_LOADED"
#define MSGID_PLUGIN_UNLOADED                       "PLUGIN_UNLOADED"

#define MSGID_SERVICE_STATUS_ERROR                  "SERVICE_STATUS_ERROR"
#define MSGID_SERVICE_STATUS                        "SERVICE_STATUS"

#define MSGID_PLUGIN_INVALID_SUBSCRIBE              "PLUGIN_INVALID_SUBSCRIBE"

#define MSGID_ADD_TIMEOUT_FAILED                    "ADD_TIMEOUT_FAILED"

#define MSGID_LOCALE_ERROR                          "LOCALE_ERROR"

#define MSGID_CREATE_ALERT_FAILED                   "CREATE_ALERT_FAILED"
