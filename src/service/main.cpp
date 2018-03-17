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

#include <iostream>
#include <glib.h>
#include <sys/signalfd.h>

#include "logging.h"
#include "config.h"
#include "pluginloader.h"
#include "pluginmanager.h"
#include "servicemonitor.h"

static const char *LOG_CONTEXT_NAME = COMPONENT_NAME;

PmLogContext logContext;
static bool terminated = false;
static GMainLoop *mainLoop = NULL;

//Option variables
static gboolean option_version = FALSE;

static GOptionEntry options[] =
{
	{
		"version", 'v', 0, G_OPTION_ARG_NONE, &option_version,
		"Show version information and exit"
	},
	{ nullptr },
};


void processOptions(int argc, char **argv)
{
	GOptionContext *context;
	GError *err = nullptr;

	context = g_option_context_new(nullptr);
	g_option_context_add_main_entries(context, options, nullptr);

	if (g_option_context_parse(context, &argc, &argv, &err) == FALSE)
	{
		if (err != nullptr)
		{
			g_printerr("%s\n", err->message);
			g_error_free(err);
		}
		else
		{
			g_printerr("An unknown error occurred\n");
		}

		exit(EXIT_FAILURE);
	}

	g_option_context_free(context);
}

void setupLogging()
{
	PmLogErr error = PmLogGetContext(LOG_CONTEXT_NAME, &logContext);

	if (error != kPmLogErr_None)
	{
		std::cerr << "Failed to setup up log context " << LOG_CONTEXT_NAME << std::endl;
		abort();
	}
}

static gboolean signal_handler(GIOChannel *channel, GIOCondition cond,
                               gpointer user_data)
{
	struct signalfd_siginfo si;
	ssize_t result;
	int fd;

	if (cond & (G_IO_NVAL | G_IO_ERR | G_IO_HUP))
	{
		return FALSE;
	}

	fd = g_io_channel_unix_get_fd(channel);

	result = read(fd, &si, sizeof(si));

	if (result != sizeof(si))
	{
		return FALSE;
	}

	switch (si.ssi_signo)
	{
		case SIGUSR1:
			/* This will exit the service without correctly terminating and leaving every
			 * applied configured as it is. This is needed to simulate various test
			 * situations. */
			exit(EXIT_FAILURE);
			break;

		case SIGINT:
		case SIGTERM:
			if (terminated == 0)
			{
				LOG_INFO(MSGIC_TERMINATING, 0, "Terminating");
				g_main_loop_quit(mainLoop);
			}

			terminated = 1;
			break;

		default:
			break;
	}

	return TRUE;
}

static guint setup_signalfd()
{
	GIOChannel *channel;
	guint source;
	sigset_t mask;
	int fd;

	sigemptyset(&mask);
	sigaddset(&mask, SIGINT);
	sigaddset(&mask, SIGTERM);
	sigaddset(&mask, SIGUSR1);

	if (sigprocmask(SIG_BLOCK, &mask, NULL) < 0)
	{
		std::cerr << "Failed to set signal mask";
		return 0;
	}

	fd = signalfd(-1, &mask, 0);

	if (fd < 0)
	{
		std::cerr << "Failed to create signal descriptor";
		return 0;
	}

	channel = g_io_channel_unix_new(fd);

	g_io_channel_set_close_on_unref(channel, TRUE);
	g_io_channel_set_encoding(channel, NULL, NULL);
	g_io_channel_set_buffered(channel, FALSE);

	source = g_io_add_watch(channel,
	                        static_cast<GIOCondition>(G_IO_IN | G_IO_HUP | G_IO_ERR | G_IO_NVAL),
	                        signal_handler, NULL);

	g_io_channel_unref(channel);

	return source;
}

int main(int argc, char **argv)
{
	std::unique_ptr<GMainLoop, std::function<void(GMainLoop *)>> main_loop{g_main_loop_new(nullptr, FALSE), g_main_loop_unref};
	mainLoop = main_loop.get();

	guint signal = setup_signalfd();

	processOptions(argc, argv);

	if (option_version == TRUE)
	{
		std::cout << VERSION << std::endl;
		exit(EXIT_SUCCESS);
	}

	setupLogging();

	try
	{
		//setup the service
		LunaService service{SERVICE_BUS_NAME, mainLoop, nullptr};
		PluginLoader loader{WEBOS_EVENT_MONITOR_PLUGIN_PATH};
		PluginManager manager{loader, service, mainLoop};

		ServiceMonitor monitor{manager, service};
		monitor.startMonitor(loader.getPlugins());

		g_main_loop_run(mainLoop);
	}
	catch(...)
	{
		LOG_ERROR(MSGID_SERVICE_STATUS_ERROR, 0,"startMonitor failure :");
		exit(EXIT_FAILURE);
	}

	g_source_remove(signal);

	return EXIT_SUCCESS;
}

