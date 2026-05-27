#pragma once

#include "worker-launcher.hpp"

#include <string>
#include <vector>

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
	std::string dock_theme = "classic";
	std::string ui_language = "en";
	std::vector<std::string> deck_candidates;
	std::vector<std::string> opponent_deck_candidates;
	std::string last_deck_name;
	std::string last_opponent_deck;
	std::string last_rank;
	std::string last_dp;
	std::string upload_title_template = "Duel {match_id} vs {opponent_deck} - {result}";
	std::string upload_description_template =
		"OBS Duel Recorder Archive\n\nMatch ID: {match_id}\nDeck: {deck_name}\nOpponent: {opponent_deck}\nResult: {result}\nRank: {rank}\nDP: {dp}\nStarted: {started_at}\nEnded: {ended_at}\n\nNotes:\n{memo}";
	std::string upload_tags_template = "Yu-Gi-Oh! Master Duel,{deck_name},{opponent_deck},{result}";
	std::wstring user_data_dir;
	std::wstring settings_path;
	bool restart_worker_on_change = true;
};

std::wstring default_plugin_settings_path();
PluginSettings load_plugin_settings();
bool save_plugin_settings(const PluginSettings &settings);
WorkerLaunchConfig make_launch_config(const PluginSettings &settings);

} // namespace odr::plugin
