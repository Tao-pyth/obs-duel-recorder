#include <obs-frontend-api.h>
#include <obs-module.h>

#include "plugin-settings.hpp"
#include "plugin-ui.hpp"
#include "worker-launcher.hpp"

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
	odr::plugin::WorkerLaunchConfig config = odr::plugin::make_launch_config(settings);
	worker_manager.start_async(std::move(config));
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
	ui_controller.unregister_ui();
	worker_manager.stop();
	obs_frontend_remove_event_callback(frontend_event, nullptr);
	blog(LOG_INFO, "%s plugin shutdown", kLogPrefix);
}

const char *obs_module_description(void)
{
	return "OBS Duel Recorder plugin skeleton";
}
