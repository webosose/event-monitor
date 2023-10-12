# Copyright (c) 2015-2023 LG Electronics, Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 2.8.7)

project(event-monitor CXX)

include(webOS/webOS)
webos_modules_init(1 6 0)
webos_component(1 1 0)

webos_add_compiler_flags(ALL -Wall -std=c++11)

if (_LOCAL_BUILD)
    webos_add_compiler_flags(ALL -DSECURITY_COMPATIBILITY)
endif (_LOCAL_BUILD)

include(FindPkgConfig)

pkg_check_modules(GLIB2 REQUIRED glib-2.0)
include_directories(${GLIB2_INCLUDE_DIRS})
webos_add_compiler_flags(ALL ${GLIB2_CFLAGS_OTHER})

pkg_check_modules(LUNASERVICE2PP REQUIRED luna-service2++)
include_directories(${LUNASERVICE2PP_INCLUDE_DIRS})
webos_add_compiler_flags(ALL ${LUNASERVICE2PP_CFLAGS_OTHER})

pkg_check_modules(PBNJSON_CPP REQUIRED pbnjson_cpp)
include_directories(${PBNJSON_CPP_INCLUDE_DIRS})
webos_add_compiler_flags(ALL ${PBNJSON_CPP_CFLAGS_OTHER})

pkg_check_modules(PMLOG REQUIRED PmLogLib)
include_directories(${PMLOG_INCLUDE_DIRS})
webos_add_compiler_flags(ALL ${PMLOG_CFLAGS_OTHER})

pkg_check_modules(I18N REQUIRED webosi18n)
include_directories(${I18N_INCLUDE_DIRS})
webos_add_compiler_flags(ALL ${I18N_CFLAGS_OTHER})


# Require that all undefined symbols are satisfied by the libraries from target_link_libraries()
webos_add_linker_options(ALL --no-undefined)

include_directories(include/public)

######## Service ########

set(LIBS
        ${GLIB2_LDFLAGS}
        ${PBNJSON_CPP_LDFLAGS}
        ${PMLOG_LDFLAGS}
        ${LUNASERVICE2PP_LDFLAGS}
        ${I18N_LDFLAGS}
        dl
        )

file(GLOB SOURCES src/service/*.cpp)

webos_configure_source_files(SOURCES src/config.h)
webos_add_compiler_flags(ALL -I${CMAKE_CURRENT_BINARY_DIR}/Configured/src)

add_executable(event-monitor ${SOURCES})
target_link_libraries(event-monitor ${LIBS})
webos_build_daemon()

message("BUILD_MOCK_PLUGIN is ${BUILD_MOCK_PLUGIN}")
if(${BUILD_MOCK_PLUGIN} MATCHES YES)
    webos_build_system_bus_files(files/sysbus_mockplugin)
else()
    webos_build_system_bus_files()
endif()

######## Headers for plugins ########

install(DIRECTORY "include/public/" DESTINATION @WEBOS_INSTALL_INCLUDEDIR@ FILES_MATCHING PATTERN "*.h*" PATTERN ".*" EXCLUDE)

######## Mock plugin ########
if (BUILD_MOCK_PLUGIN)

    set(MOCK_LIBS
            ${GLIB2_LDFLAGS}
            ${PBNJSON_CPP_LDFLAGS}
            ${PMLOG_LDFLAGS}
            ${I18N_LDFLAGS}
            )

    file(GLOB MOCK_SOURCES src/mockplugin/*.cpp)

    set(CMAKE_SHARED_MODULE_PREFIX "")
    add_library(mock-plugin MODULE ${MOCK_SOURCES})
    target_link_libraries(mock-plugin ${MOCK_LIBS})

    install(TARGETS mock-plugin DESTINATION ${WEBOS_EVENT_MONITOR_PLUGIN_PATH})

endif (BUILD_MOCK_PLUGIN)