#pragma once

#include "plugin-settings.hpp"
#include "worker-launcher.hpp"

#include <string>

class QLabel;
class QPushButton;
class QFrame;
class QGroupBox;
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
	void apply_dock_theme(const std::string &theme);
	void show_settings_dialog();
	void save_settings_and_restart(const PluginSettings &settings);
	void request_manual_start();
	void request_manual_stop();
	void request_upload_retry();
	void request_upload_discard();
	void request_upload_mark_uploaded();
	void request_edit_metadata();
	void request_preview_upload_metadata();
	void handle_automatic_recording(const WorkerStatusSnapshot &snapshot);
	void log_recording_command_result(const char *action, const RecordingCommandResult &result);
	void log_queue_command_result(const char *action, const QueueCommandResult &result);

	WorkerProcessManager &worker_manager_;
	QWidget *dock_widget_ = nullptr;
	QTimer *refresh_timer_ = nullptr;
	QLabel *state_value_ = nullptr;
	QLabel *setup_value_ = nullptr;
	QLabel *recording_value_ = nullptr;
	QLabel *output_value_ = nullptr;
	QLabel *queue_value_ = nullptr;
	QLabel *review_item_value_ = nullptr;
	QLabel *endpoint_value_ = nullptr;
	QLabel *user_data_value_ = nullptr;
	QLabel *worker_path_value_ = nullptr;
	QLabel *logs_value_ = nullptr;
	QLabel *ownership_value_ = nullptr;
	QLabel *detail_value_ = nullptr;
	QLabel *action_value_ = nullptr;
	QFrame *header_card_ = nullptr;
	QFrame *setup_card_ = nullptr;
	QFrame *recording_card_ = nullptr;
	QFrame *upload_card_ = nullptr;
	QFrame *metadata_card_ = nullptr;
	QGroupBox *diagnostics_group_ = nullptr;
	QPushButton *settings_button_ = nullptr;
	QPushButton *start_button_ = nullptr;
	QPushButton *stop_button_ = nullptr;
	QPushButton *retry_upload_button_ = nullptr;
	QPushButton *discard_upload_button_ = nullptr;
	QPushButton *mark_uploaded_button_ = nullptr;
	QPushButton *edit_metadata_button_ = nullptr;
	QPushButton *preview_metadata_button_ = nullptr;
	std::string dock_theme_ = "classic";
	OverlayStatePayload last_applied_overlay_state_;
	std::string automatic_recording_request_key_;
	bool overlay_state_applied_ = false;
	bool tools_menu_registered_ = false;
};

} // namespace odr::plugin
