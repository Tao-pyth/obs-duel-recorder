#include "plugin-settings.hpp"

#include <obs-module.h>

#ifdef _WIN32
#include <windows.h>
#endif

#include <algorithm>
#include <cstdint>
#include <string>

namespace odr::plugin {
namespace {

constexpr const char *kDefaultHost = "127.0.0.1";
constexpr uint16_t kDefaultPort = 8787;
constexpr const wchar_t *kSettingsDirectory = L"obs-duel-recorder";
constexpr const wchar_t *kSettingsFile = L"plugin-settings.json";

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
	obs_data_set_default_string(data, "user_data_dir", to_utf8(read_env_wstring(L"ODR_USER_DATA_DIR")).c_str());
	obs_data_set_default_bool(data, "restart_worker_on_change", true);
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
	return config;
}

} // namespace odr::plugin
