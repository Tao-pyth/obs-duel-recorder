#include <obs-frontend-api.h>
#include <obs-module.h>

#include "overlay-sources.hpp"
#include "plugin-settings.hpp"
#include "plugin-ui.hpp"
#include "worker-launcher.hpp"

#include <string>
#include <utility>

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("obs-duel-recorder", "en-US")

namespace {

constexpr const char *kLogPrefix = "OBS Duel Recorder";
odr::plugin::WorkerProcessManager worker_manager;
odr::plugin::PluginUiController ui_controller(worker_manager);

void launch_worker()
{
	odr::plugin::PluginSettings settings = odr::plugin::load_plugin_settings();
	odr::plugin::log_overlay_source_result(odr::plugin::ensure_overlay_text_sources(settings.overlay));
	odr::plugin::WorkerLaunchConfig config = odr::plugin::make_launch_config(settings);
	worker_manager.start_async(std::move(config));
}

void log_recording_command_result(const char *action, const odr::plugin::RecordingCommandResult &result)
{
	if (result.accepted()) {
		blog(LOG_INFO,
		     "%s recording event_command=%s accepted state=%s session_id=%s source=%s last_action=%s",
		     kLogPrefix, action, result.state.state.c_str(), result.state.session_id.c_str(),
		     result.state.command_source.c_str(), result.state.last_action.c_str());
		return;
	}
	blog(LOG_WARNING, "%s recording event_command=%s failed status=%d http=%lu error=%s",
	     kLogPrefix, action, static_cast<int>(result.status), result.http_status, result.error.c_str());
}

std::string take_frontend_path(char *path)
{
	std::string result;
	if (path && path[0] != '\0') {
		result = path;
	}
	if (path) {
		bfree(path);
	}
	return result;
}

void record_current_output_path()
{
	const std::string path = take_frontend_path(obs_frontend_get_current_record_output_path());
	if (path.empty()) {
		return;
	}
	worker_manager.record_recording_output_path(path, "OBS current recording output path");
	blog(LOG_INFO, "%s recording output_path_current=%s", kLogPrefix, path.c_str());
}

std::string record_last_output_path()
{
	const std::string path = take_frontend_path(obs_frontend_get_last_recording());
	if (!path.empty()) {
		worker_manager.record_recording_output_path(path, "OBS last recording path");
		blog(LOG_INFO, "%s recording output_path_last=%s", kLogPrefix, path.c_str());
		return path;
	}

	worker_manager.record_recording_output_evidence(
		"OBS did not return a last recording path; check OBS Output settings and OBS logs.");
	blog(LOG_WARNING, "%s recording output_path_last unavailable", kLogPrefix);
	return {};
}

void frontend_event(enum obs_frontend_event event, void *)
{
	switch (event) {
	case OBS_FRONTEND_EVENT_FINISHED_LOADING:
		blog(LOG_INFO, "%s frontend ready", kLogPrefix);
		ui_controller.register_ui();
		launch_worker();
		break;
	case OBS_FRONTEND_EVENT_EXIT:
		blog(LOG_INFO, "%s frontend exit", kLogPrefix);
		worker_manager.stop();
		break;
	case OBS_FRONTEND_EVENT_RECORDING_STARTED:
		record_current_output_path();
		log_recording_command_result(
			"confirm_started",
			worker_manager.send_recording_command("confirm_started", "manual"));
		break;
	case OBS_FRONTEND_EVENT_RECORDING_STOPPED:
	{
		const std::string output_path = record_last_output_path();
		log_recording_command_result(
			"confirm_stopped",
			worker_manager.send_recording_command("confirm_stopped", "manual", output_path));
		break;
	}
	default:
		break;
	}
}

} // namespace

bool obs_module_load(void)
{
	obs_frontend_add_event_callback(frontend_event, nullptr);
	blog(LOG_INFO, "%s plugin startup", kLogPrefix);
	return true;
}

void obs_module_unload(void)
{
	blog(LOG_INFO, "%s plugin shutdown", kLogPrefix);
	ui_controller.unregister_ui();
	worker_manager.stop();
	obs_frontend_remove_event_callback(frontend_event, nullptr);
}

const char *obs_module_description(void)
{
	return "OBS Duel Recorder plugin skeleton";
}
