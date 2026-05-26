#include "plugin-settings.hpp"

#include <obs-module.h>

#ifdef _WIN32
#include <windows.h>
#endif

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

namespace odr::plugin {
namespace {

constexpr const char *kDefaultHost = "127.0.0.1";
constexpr uint16_t kDefaultPort = 8787;
constexpr const wchar_t *kSettingsDirectory = L"obs-duel-recorder";
constexpr const wchar_t *kSettingsFile = L"plugin-settings.json";

constexpr const char *kOverlayKey = "overlay";
constexpr const char *kOverlaySourcesKey = "sources";
constexpr const char *kOverlayDefaultsKey = "defaults";
constexpr const char *kDefaultDockTheme = "classic";
constexpr const char *kDefaultUiLanguage = "en";

std::wstring from_utf8(const char *value)
{
	if (!value || value[0] == '\0') {
		return {};
	}
#ifdef _WIN32
	const int size = MultiByteToWideChar(CP_UTF8, 0, value, -1, nullptr, 0);
	if (size <= 0) {
		return {};
	}
	std::wstring result(static_cast<size_t>(size - 1), L'\0');
	MultiByteToWideChar(CP_UTF8, 0, value, -1, result.data(), size);
	return result;
#else
	return std::wstring(value, value + std::char_traits<char>::length(value));
#endif
}

std::wstring parent_directory(const std::wstring &path)
{
	const size_t pos = path.find_last_of(L"\\/");
	if (pos == std::wstring::npos) {
		return {};
	}
	return path.substr(0, pos);
}

bool file_exists(const std::wstring &path)
{
	if (path.empty()) {
		return false;
	}
#ifdef _WIN32
	const DWORD attributes = GetFileAttributesW(path.c_str());
	return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
#else
	return false;
#endif
}

std::wstring current_module_path()
{
#ifdef _WIN32
	HMODULE module = nullptr;
	if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
				 GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
				reinterpret_cast<LPCWSTR>(&current_module_path), &module)) {
		return {};
	}

	std::vector<wchar_t> buffer(MAX_PATH);
	while (true) {
		const DWORD written = GetModuleFileNameW(module, buffer.data(), static_cast<DWORD>(buffer.size()));
		if (written == 0) {
			return {};
		}
		if (written < buffer.size() - 1) {
			return std::wstring(buffer.data(), written);
		}
		buffer.resize(buffer.size() * 2);
	}
#else
	return {};
#endif
}

struct WorkerPathDefaults {
	std::wstring command = L"odr-worker";
	std::wstring expected_worker_path;
	std::wstring wrong_nested_worker_path;
};

WorkerPathDefaults default_worker_paths()
{
	WorkerPathDefaults defaults;
	const std::wstring plugin_dir = parent_directory(current_module_path());
	if (plugin_dir.empty()) {
		return defaults;
	}

	const std::wstring app_dir = parent_directory(plugin_dir);
	defaults.expected_worker_path = app_dir + L"\\worker\\odr-worker\\odr-worker.exe";
	defaults.wrong_nested_worker_path = plugin_dir + L"\\worker\\odr-worker\\odr-worker.exe";
	if (file_exists(defaults.expected_worker_path)) {
		defaults.command = defaults.expected_worker_path;
	}

	return defaults;
}

std::wstring default_user_data_dir()
{
	const std::wstring configured = read_env_wstring(L"ODR_USER_DATA_DIR");
	if (!configured.empty()) {
		return configured;
	}

	const std::wstring app_data = read_env_wstring(L"APPDATA");
	if (!app_data.empty()) {
		return app_data + L"\\" + kSettingsDirectory + L"\\user_data";
	}

	return L"user_data";
}

bool ensure_directory(const std::wstring &path)
{
	if (path.empty()) {
		return false;
	}
#ifdef _WIN32
	std::wstring current;
	for (size_t i = 0; i < path.size(); ++i) {
		current.push_back(path[i]);
		if (path[i] != L'\\' && path[i] != L'/') {
			continue;
		}
		if (current.size() <= 3) {
			continue;
		}
		CreateDirectoryW(current.c_str(), nullptr);
	}
	if (!CreateDirectoryW(path.c_str(), nullptr)) {
		const DWORD error = GetLastError();
		return error == ERROR_ALREADY_EXISTS;
	}
	return true;
#else
	return false;
#endif
}

uint16_t clamp_port(long long value)
{
	if (value <= 0 || value > 65535) {
		return kDefaultPort;
	}
	return static_cast<uint16_t>(value);
}

void apply_defaults(obs_data_t *data)
{
	obs_data_set_default_string(data, "host", kDefaultHost);
	obs_data_set_default_int(data, "port", kDefaultPort);
	obs_data_set_default_string(data, "user_data_dir", to_utf8(default_user_data_dir()).c_str());
	obs_data_set_default_bool(data, "restart_worker_on_change", true);
	obs_data_set_default_string(data, "dock_theme", kDefaultDockTheme);
	obs_data_set_default_string(data, "ui_language", kDefaultUiLanguage);
}

bool is_valid_dock_theme(const std::string &theme)
{
	return theme == "classic" || theme == "forest" || theme == "bright";
}

bool is_valid_ui_language(const std::string &language)
{
	return language == "en" || language == "ja";
}

void apply_overlay_sources_defaults(obs_data_t *sources)
{
	const OverlaySettings defaults;
	obs_data_set_default_string(sources, "deck_name", defaults.deck_name.source_name.c_str());
	obs_data_set_default_string(sources, "sequence_number", defaults.sequence_number.source_name.c_str());
	obs_data_set_default_string(sources, "result", defaults.result.source_name.c_str());
	obs_data_set_default_string(sources, "opponent_deck", defaults.opponent_deck.source_name.c_str());
	obs_data_set_default_string(sources, "recording_state", defaults.recording_state.source_name.c_str());
}

void apply_overlay_text_defaults(obs_data_t *defaults_data)
{
	const OverlaySettings defaults;
	obs_data_set_default_string(defaults_data, "deck_name", defaults.deck_name.default_text.c_str());
	obs_data_set_default_string(defaults_data, "sequence_number", defaults.sequence_number.default_text.c_str());
	obs_data_set_default_string(defaults_data, "result", defaults.result.default_text.c_str());
	obs_data_set_default_string(defaults_data, "opponent_deck", defaults.opponent_deck.default_text.c_str());
	obs_data_set_default_string(defaults_data, "recording_state", defaults.recording_state.default_text.c_str());
}

void apply_overlay_defaults(obs_data_t *overlay)
{
	obs_data_set_default_bool(overlay, "enabled", true);
	obs_data_set_default_bool(overlay, "auto_create_sources", true);

	obs_data_t *sources = obs_data_create();
	apply_overlay_sources_defaults(sources);
	obs_data_set_default_obj(overlay, kOverlaySourcesKey, sources);
	obs_data_release(sources);

	obs_data_t *defaults_data = obs_data_create();
	apply_overlay_text_defaults(defaults_data);
	obs_data_set_default_obj(overlay, kOverlayDefaultsKey, defaults_data);
	obs_data_release(defaults_data);
}

std::string read_string(obs_data_t *data, const char *key, const std::string &fallback, const char *section)
{
	const char *value = obs_data_get_string(data, key);
	if (!value || value[0] == '\0') {
		blog(LOG_WARNING, "OBS Duel Recorder overlay diagnostic=overlay_settings_invalid field=%s action=load reason=empty_%s",
		     key, section);
		return fallback;
	}
	return value;
}

void load_overlay_field(obs_data_t *sources, obs_data_t *defaults_data, const char *key, OverlayFieldSettings &field)
{
	field.source_name = read_string(sources, key, field.source_name, "source_name");
	field.default_text = read_string(defaults_data, key, field.default_text, "default_text");
}

OverlaySettings load_overlay_settings(obs_data_t *data)
{
	OverlaySettings settings;
	obs_data_t *overlay = obs_data_get_obj(data, kOverlayKey);
	if (!overlay) {
		overlay = obs_data_create();
	}
	apply_overlay_defaults(overlay);

	settings.enabled = obs_data_get_bool(overlay, "enabled");
	settings.auto_create_sources = obs_data_get_bool(overlay, "auto_create_sources");

	obs_data_t *sources = obs_data_get_obj(overlay, kOverlaySourcesKey);
	if (!sources) {
		sources = obs_data_create();
	}
	apply_overlay_sources_defaults(sources);

	obs_data_t *defaults_data = obs_data_get_obj(overlay, kOverlayDefaultsKey);
	if (!defaults_data) {
		defaults_data = obs_data_create();
	}
	apply_overlay_text_defaults(defaults_data);

	load_overlay_field(sources, defaults_data, "deck_name", settings.deck_name);
	load_overlay_field(sources, defaults_data, "sequence_number", settings.sequence_number);
	load_overlay_field(sources, defaults_data, "result", settings.result);
	load_overlay_field(sources, defaults_data, "opponent_deck", settings.opponent_deck);
	load_overlay_field(sources, defaults_data, "recording_state", settings.recording_state);

	obs_data_release(defaults_data);
	obs_data_release(sources);
	obs_data_release(overlay);
	return settings;
}

void save_overlay_field(obs_data_t *sources, obs_data_t *defaults_data, const char *key,
			const OverlayFieldSettings &field)
{
	obs_data_set_string(sources, key, field.source_name.c_str());
	obs_data_set_string(defaults_data, key, field.default_text.c_str());
}

void save_overlay_settings(obs_data_t *data, const OverlaySettings &settings)
{
	obs_data_t *overlay = obs_data_create();
	obs_data_set_bool(overlay, "enabled", settings.enabled);
	obs_data_set_bool(overlay, "auto_create_sources", settings.auto_create_sources);

	obs_data_t *sources = obs_data_create();
	obs_data_t *defaults_data = obs_data_create();
	save_overlay_field(sources, defaults_data, "deck_name", settings.deck_name);
	save_overlay_field(sources, defaults_data, "sequence_number", settings.sequence_number);
	save_overlay_field(sources, defaults_data, "result", settings.result);
	save_overlay_field(sources, defaults_data, "opponent_deck", settings.opponent_deck);
	save_overlay_field(sources, defaults_data, "recording_state", settings.recording_state);

	obs_data_set_obj(overlay, kOverlaySourcesKey, sources);
	obs_data_set_obj(overlay, kOverlayDefaultsKey, defaults_data);
	obs_data_set_obj(data, kOverlayKey, overlay);

	obs_data_release(defaults_data);
	obs_data_release(sources);
	obs_data_release(overlay);
}

} // namespace

std::wstring default_plugin_settings_path()
{
#ifdef _WIN32
	const std::wstring app_data = read_env_wstring(L"APPDATA");
	if (!app_data.empty()) {
		return app_data + L"\\" + kSettingsDirectory + L"\\" + kSettingsFile;
	}
#endif
	return std::wstring(kSettingsFile);
}

PluginSettings load_plugin_settings()
{
	PluginSettings settings;
	settings.settings_path = default_plugin_settings_path();

	obs_data_t *data = obs_data_create_from_json_file_safe(to_utf8(settings.settings_path).c_str(), "bak");
	if (!data) {
		data = obs_data_create();
	}
	apply_defaults(data);

	settings.endpoint.host = from_utf8(obs_data_get_string(data, "host"));
	if (settings.endpoint.host.empty()) {
		settings.endpoint.host = from_utf8(kDefaultHost);
	}
	settings.endpoint.port = clamp_port(obs_data_get_int(data, "port"));
	settings.user_data_dir = from_utf8(obs_data_get_string(data, "user_data_dir"));
	settings.restart_worker_on_change = obs_data_get_bool(data, "restart_worker_on_change");
	settings.dock_theme = obs_data_get_string(data, "dock_theme");
	if (!is_valid_dock_theme(settings.dock_theme)) {
		settings.dock_theme = kDefaultDockTheme;
	}
	settings.ui_language = obs_data_get_string(data, "ui_language");
	if (!is_valid_ui_language(settings.ui_language)) {
		settings.ui_language = kDefaultUiLanguage;
	}
	settings.overlay = load_overlay_settings(data);

	obs_data_release(data);
	return settings;
}

bool save_plugin_settings(const PluginSettings &settings)
{
	const std::wstring parent = parent_directory(settings.settings_path);
	if (!parent.empty() && !ensure_directory(parent)) {
		blog(LOG_WARNING, "OBS Duel Recorder settings save failed path=%s", to_utf8(settings.settings_path).c_str());
		return false;
	}

	obs_data_t *data = obs_data_create();
	obs_data_set_string(data, "host", to_utf8(settings.endpoint.host).c_str());
	obs_data_set_int(data, "port", settings.endpoint.port);
	obs_data_set_string(data, "user_data_dir", to_utf8(settings.user_data_dir).c_str());
	obs_data_set_bool(data, "restart_worker_on_change", settings.restart_worker_on_change);
	obs_data_set_string(data, "dock_theme",
			    is_valid_dock_theme(settings.dock_theme) ? settings.dock_theme.c_str() : kDefaultDockTheme);
	obs_data_set_string(data, "ui_language",
			    is_valid_ui_language(settings.ui_language) ? settings.ui_language.c_str() : kDefaultUiLanguage);
	save_overlay_settings(data, settings.overlay);

	const bool saved = obs_data_save_json_safe(data, to_utf8(settings.settings_path).c_str(), "tmp", "bak");
	obs_data_release(data);

	if (!saved) {
		blog(LOG_WARNING, "OBS Duel Recorder settings save failed path=%s", to_utf8(settings.settings_path).c_str());
	}
	return saved;
}

WorkerLaunchConfig make_launch_config(const PluginSettings &settings)
{
	WorkerLaunchConfig config;
	config.endpoint = settings.endpoint;
	config.user_data_dir = settings.user_data_dir;
	const WorkerPathDefaults worker_paths = default_worker_paths();
	config.command = worker_paths.command;
	config.expected_worker_path = worker_paths.expected_worker_path;
	config.wrong_nested_worker_path = worker_paths.wrong_nested_worker_path;
	return config;
}

} // namespace odr::plugin
