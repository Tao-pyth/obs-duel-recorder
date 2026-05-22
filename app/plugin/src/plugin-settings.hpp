#pragma once

#include "worker-launcher.hpp"

#include <string>

namespace odr::plugin {

struct OverlayFieldSettings {
	std::string source_name;
	std::string default_text;
};

struct OverlaySettings {
	bool enabled = true;
	bool auto_create_sources = true;
	OverlayFieldSettings deck_name{"ODR Deck Name", "Deck: -"};
	OverlayFieldSettings sequence_number{"ODR Sequence", "#---"};
	OverlayFieldSettings result{"ODR Result", "Result: unknown"};
	OverlayFieldSettings opponent_deck{"ODR Opponent Deck", "Opponent: unknown"};
	OverlayFieldSettings recording_state{"ODR Recording State", "Idle"};
};

struct PluginSettings {
	WorkerEndpoint endpoint;
	OverlaySettings overlay;
	std::wstring user_data_dir;
	std::wstring settings_path;
	bool restart_worker_on_change = true;
};

std::wstring default_plugin_settings_path();
PluginSettings load_plugin_settings();
bool save_plugin_settings(const PluginSettings &settings);
WorkerLaunchConfig make_launch_config(const PluginSettings &settings);

} // namespace odr::plugin
