/*
VST Ex Audio Filter for OBS
Copyright (C) 2026 ChromeBread feat. BlackDream

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program. If not, see <https://www.gnu.org/licenses/>
*/

#include "pitch-filter.h"
#include "plugin-meta.h"
#include "stereo-filter.h"
#include "switch-sets.h"
#include "ui/ex-filters-dialog.h"
#include "ui/menu-hook.h"
#include "master-bus.h"
#include "ui/fx-dock.h"
#include "ui/master-dock.h"
#include "ui/switch-dock.h"
#include "ui/tools-dialog.h"
#include "ui/waveline-dock.h"
#include "loader-api.h"
#include "log-file.h"

#include <obs-frontend-api.h>
#include <obs-module.h>
#include <plugin-support.h>

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE(PLUGIN_NAME, "en-US")

MODULE_EXPORT const char *obs_module_description(void)
{
	return obs_module_text("Description");
}

static void onPluginEdited(void *, calldata_t *cd)
{
	ExFiltersDialog::notifyPluginEdited(static_cast<obs_source_t *>(calldata_ptr(cd, "source")));
}

bool obs_module_load(void)
{
	log_file_set_enabled(PluginMeta::instance().uiValue("logFile").toBool(false));
	apply_saved_language();
	register_pitch_filter();
	register_stereo_filter();
	// The loader declares its signals when it loads, possibly after us.
	obs_frontend_add_event_callback(
		[](enum obs_frontend_event event, void *) {
			if (event == OBS_FRONTEND_EVENT_FINISHED_LOADING && loader_installed())
				signal_handler_connect(obs_get_signal_handler(), "vst_enigmax_edited", onPluginEdited, nullptr);
		},
		nullptr);
	install_mixer_menu_hook();
	install_tools_menu();
	switchsets::install();
	masterbus::install(); // always, locked or not: its saved filters must survive in the scene collection
	// Optional parts, chosen in the installer or Tools > VST Ex Audio Filter (read once, at load).
	if (PluginMeta::instance().uiValue("featureSwitcher").toBool(true))
		install_switch_dock();
	if (PluginMeta::instance().uiValue("featureMixer").toBool(true))
		install_fx_dock();
	if (masterbus::unlocked())
		install_master_dock();
	if (PluginMeta::instance().uiValue("featureWaveline").toBool(true))
		install_waveline_dock();
	obs_log(LOG_INFO, "plugin loaded successfully (version %s)", PLUGIN_VERSION);
	return true;
}

void obs_module_unload(void)
{
	remove_waveline_dock();
	remove_master_dock();
	remove_fx_dock();
	remove_switch_dock();
	masterbus::uninstall();
	switchsets::uninstall();
	remove_mixer_menu_hook();
	if (loader_installed())
		signal_handler_disconnect(obs_get_signal_handler(), "vst_enigmax_edited", onPluginEdited, nullptr);
	obs_log(LOG_INFO, "plugin unloaded");
}
