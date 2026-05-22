#pragma once

#include "worker-launcher.hpp"

class QLabel;
class QTimer;
class QWidget;

namespace odr::plugin {

class PluginUiController {
public:
	explicit PluginUiController(WorkerProcessManager &worker_manager);

	void register_ui();
	void unregister_ui();
	void refresh();

private:
	static void open_settings_from_menu(void *private_data);
	void show_settings_dialog();
	void save_settings_and_restart(const PluginSettings &settings);

	WorkerProcessManager &worker_manager_;
	QWidget *dock_widget_ = nullptr;
	QTimer *refresh_timer_ = nullptr;
	QLabel *state_value_ = nullptr;
	QLabel *endpoint_value_ = nullptr;
	QLabel *user_data_value_ = nullptr;
	QLabel *logs_value_ = nullptr;
	QLabel *ownership_value_ = nullptr;
	QLabel *detail_value_ = nullptr;
	QLabel *action_value_ = nullptr;
	bool tools_menu_registered_ = false;
};

} // namespace odr::plugin
