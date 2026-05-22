#pragma once

#include "worker-launcher.hpp"

namespace odr::plugin {

struct PluginSettings {
	WorkerEndpoint endpoint;
	std::wstring user_data_dir;
	std::wstring settings_path;
	bool restart_worker_on_change = true;
};

std::wstring default_plugin_settings_path();
PluginSettings load_plugin_settings();
bool save_plugin_settings(const PluginSettings &settings);
WorkerLaunchConfig make_launch_config(const PluginSettings &settings);

} // namespace odr::plugin
