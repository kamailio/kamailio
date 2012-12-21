/* Kamailio PURPLE MODULE
 * 
 * Copyright (C) 2008 Atos Worldline
 * Contact: Eric PTAK <eric.ptak@atosorigin.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */
#include <unistd.h>
#include <glib.h>
#include <errno.h>
#include <string.h>
#include <signal.h>

#include "../../dprint.h"
#include "../../str.h"

#include <libpurple/account.h>
#include <libpurple/accountopt.h>
#include <libpurple/conversation.h>
#include <libpurple/connection.h>
#include <libpurple/core.h>
#include <libpurple/debug.h>
#include <libpurple/eventloop.h>
#include <libpurple/ft.h>
#include <libpurple/log.h>
#include <libpurple/notify.h>
#include <libpurple/plugin.h>
#include <libpurple/prefs.h>
#include <libpurple/prpl.h>
#include <libpurple/pounce.h>
#include <libpurple/savedstatuses.h>
#include <libpurple/sound.h>
#include <libpurple/status.h>
#include <libpurple/util.h>
#include <libpurple/whiteboard.h>
#include <libpurple/xmlnode.h>

#include "miniclient.h"
#include "defines.h"
#include "purple.h"
#include "mapping.h"
#include "clientsig.h"
#include "clientpipe.h"
#include "clientops.h"
#include "hashtable.h"

PurpleProxyInfo *proxy = NULL;
extern str httpProxy_host;
extern int httpProxy_port;


/**
 * The following eventloop functions are used in both pidgin and purple-text. If your
 * application uses glib mainloop, you can safely use this verbatim.
 */
#define PURPLE_GLIB_READ_COND  (G_IO_IN | G_IO_HUP | G_IO_ERR)
#define PURPLE_GLIB_WRITE_COND (G_IO_OUT | G_IO_HUP | G_IO_ERR | G_IO_NVAL)

static void io_destroy(gpointer data) {
	g_free(data);
}

static gboolean io_invoke(GIOChannel *source, GIOCondition condition, gpointer data) {
	PurpleGLibIOClosure *closure = data;
	PurpleInputCondition purple_cond = 0;

	if (condition & PURPLE_GLIB_READ_COND)
		purple_cond |= PURPLE_INPUT_READ;
	if (condition & PURPLE_GLIB_WRITE_COND)
		purple_cond |= PURPLE_INPUT_WRITE;

	closure->function(closure->data, g_io_channel_unix_get_fd(source), purple_cond);

	return TRUE;
}

static guint input_add(gint fd, PurpleInputCondition condition, PurpleInputFunction function, gpointer data) {
	PurpleGLibIOClosure *closure = g_new0(PurpleGLibIOClosure, 1);
	GIOChannel *channel;
	GIOCondition cond = 0;

	closure->function = function;
	closure->data = data;

	if (condition & PURPLE_INPUT_READ)
		cond |= PURPLE_GLIB_READ_COND;
	if (condition & PURPLE_INPUT_WRITE)
		cond |= PURPLE_GLIB_WRITE_COND;

	channel = g_io_channel_unix_new(fd);
	g_io_channel_set_encoding(channel, NULL, NULL);
	closure->result = g_io_add_watch_full(channel, G_PRIORITY_DEFAULT, cond, io_invoke, closure, io_destroy);

	g_io_channel_unref(channel);
	return closure->result;
}

static PurpleEventLoopUiOps glib_eventloops = {
	g_timeout_add,
	g_source_remove,
	input_add,
	g_source_remove,
	NULL,
#if GLIB_CHECK_VERSION(2,14,0)
	g_timeout_add_seconds,
#else
	NULL,
#endif

	/* padding */
	NULL,
	NULL,
	NULL
};
/*** End of the eventloop functions. ***/

static PurpleConversationUiOps conv_uiops = {
	NULL,                      /* create_conversation  */
	NULL,                      /* destroy_conversation */
	NULL,		               /* write_chat           */
	NULL,	                   /* write_im             */
	write_conv,                /* write_conv           */
	NULL,                      /* chat_add_users       */
	NULL,                      /* chat_rename_user     */
	NULL,                      /* chat_remove_users    */
	NULL,                      /* chat_update_user     */
	NULL,                      /* present              */
	NULL,                      /* has_focus            */
	NULL,                      /* custom_smiley_add    */
	NULL,                      /* custom_smiley_write  */
	NULL,                      /* custom_smiley_close  */
	NULL,                      /* send_confirm         */
	NULL,
	NULL,
	NULL,
	NULL
};

static void ui_init(void) {
	purple_conversations_set_ui_ops(&conv_uiops);
}

static PurpleCoreUiOps core_uiops = {
	NULL,
	NULL,
	ui_init,
	NULL,

	/* padding */
	NULL,
	NULL,
	NULL,
	NULL
};

static void init_libpurple(int fd) {
	/* Set a custom user directory (optional) */
	purple_util_set_user_dir(USER_DIRECTORY);

	purple_debug_set_enabled(FALSE);

	purple_core_set_ui_ops(&core_uiops);

	purple_eventloop_set_ui_ops(&glib_eventloops);

	purple_plugins_add_search_path(PLUGIN_PATH);
	
	purple_input_add(fd, PURPLE_INPUT_READ, pipe_reader, NULL);
	

	if (!purple_core_init(UI_ID)) {
		/* Initializing the core failed. Terminate. */
		LM_ERR("libpurple initialization failed.\n");
		abort();
	}

	purple_set_blist(purple_blist_new());
	purple_blist_load();

	purple_prefs_load();

	purple_plugins_load_saved(PLUGIN_PREF);

	purple_pounces_load();
}

void miniclient_start(int fd) {
	LM_DBG("starting miniclient... \n");

	GMainLoop *loop = g_main_loop_new(NULL, FALSE);

	/* libpurple's built-in DNS resolution forks termination ignore */
	signal(SIGCHLD, SIG_IGN);

	LM_DBG("initializing libpurple...\n");
	init_libpurple(fd);
	LM_DBG("libpurple initialized successfully...\n");
	
	if (httpProxy_host.len > 0) {
		proxy = purple_proxy_info_new();
		purple_proxy_info_set_type(proxy, PURPLE_PROXY_HTTP);
		purple_proxy_info_set_host(proxy, httpProxy_host.s);
		purple_proxy_info_set_port(proxy, httpProxy_port);
	}

	hashtable_init();
	client_connect_signals();

	g_main_loop_run(loop);
}

