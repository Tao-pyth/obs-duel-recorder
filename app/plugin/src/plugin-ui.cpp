#include "plugin-ui.hpp"

#include "overlay-sources.hpp"
#include "plugin-settings.hpp"

#include <obs-frontend-api.h>
#include <obs-module.h>

#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

#include <string>

namespace odr::plugin {
namespace {

constexpr const char *kDockId = "obs-duel-recorder-status";
constexpr const char *kLogPrefix = "OBS Duel Recorder";

QString qstr_utf8(const std::string &value)
{
	return QString::fromUtf8(value.c_str());
}

QString qstr_wide(const std::wstring &value)
{
	return QString::fromStdWString(value);
}

const char *state_name(WorkerDiagnosticState state)
{
	switch (state) {
	case WorkerDiagnosticState::not_started:
		return "not_started";
	case WorkerDiagnosticState::starting:
		return "starting";
	case WorkerDiagnosticState::running:
		return "running";
	case WorkerDiagnosticState::unhealthy:
		return "unhealthy";
	case WorkerDiagnosticState::config_error:
		return "config_error";
	case WorkerDiagnosticState::runtime_dir_error:
		return "runtime_dir_error";
	case WorkerDiagnosticState::api_incompatible:
		return "api_incompatible";
	case WorkerDiagnosticState::crashed:
		return "crashed";
	}
	return "unknown";
}

const char *ownership_name(WorkerOwnership ownership)
{
	switch (ownership) {
	case WorkerOwnership::none:
		return "none";
	case WorkerOwnership::spawned_by_plugin:
		return "plugin-spawned";
	case WorkerOwnership::reused_existing:
		return "reused-existing";
	}
	return "unknown";
}

const char *recommended_action(WorkerDiagnosticState state)
{
	switch (state) {
	case WorkerDiagnosticState::not_started:
		return "Open settings and save to start Worker.";
	case WorkerDiagnosticState::starting:
		return "Wait for Worker readiness; check logs if it times out.";
	case WorkerDiagnosticState::running:
		return "No action required.";
	case WorkerDiagnosticState::unhealthy:
		return "Check Worker logs and restart through settings if needed.";
	case WorkerDiagnosticState::config_error:
		return "Check user_data_dir and Worker install layout.";
	case WorkerDiagnosticState::runtime_dir_error:
		return "Fix user_data_dir or stop the wrong-root Worker.";
	case WorkerDiagnosticState::api_incompatible:
		return "Update Plugin and Worker to a compatible pair.";
	case WorkerDiagnosticState::crashed:
		return "Check Worker logs and save settings to restart.";
	}
	return "Check OBS and Worker logs.";
}

QLabel *add_row(QFormLayout *layout, const char *name)
{
	auto *value = new QLabel;
	value->setTextInteractionFlags(Qt::TextSelectableByMouse);
	value->setWordWrap(true);
	layout->addRow(QString::fromUtf8(name), value);
	return value;
}

} // namespace

PluginUiController::PluginUiController(WorkerProcessManager &worker_manager)
	: worker_manager_(worker_manager)
{
}

void PluginUiController::register_ui()
{
	if (!tools_menu_registered_) {
		obs_frontend_add_tools_menu_item("OBS Duel Recorder Settings", open_settings_from_menu, this);
		tools_menu_registered_ = true;
	}

	if (dock_widget_) {
		refresh();
		return;
	}

	dock_widget_ = new QWidget;
	dock_widget_->setMinimumWidth(320);

	auto *root = new QVBoxLayout(dock_widget_);
	root->setContentsMargins(10, 10, 10, 10);
	root->setSpacing(8);

	auto *form = new QFormLayout;
	form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
	state_value_ = add_row(form, "State");
	endpoint_value_ = add_row(form, "Endpoint");
	user_data_value_ = add_row(form, "User data");
	worker_path_value_ = add_row(form, "Worker path");
	logs_value_ = add_row(form, "Logs");
	ownership_value_ = add_row(form, "Ownership");
	detail_value_ = add_row(form, "Detail");
	action_value_ = add_row(form, "Action");
	root->addLayout(form);

	auto *settings_button = new QPushButton("Settings");
	QObject::connect(settings_button, &QPushButton::clicked, [this]() { show_settings_dialog(); });
	root->addWidget(settings_button);

	auto *recording_controls = new QHBoxLayout;
	start_button_ = new QPushButton("Start Recording");
	stop_button_ = new QPushButton("Stop Recording");
	QObject::connect(start_button_, &QPushButton::clicked, [this]() { request_manual_start(); });
	QObject::connect(stop_button_, &QPushButton::clicked, [this]() { request_manual_stop(); });
	recording_controls->addWidget(start_button_);
	recording_controls->addWidget(stop_button_);
	root->addLayout(recording_controls);
	root->addStretch(1);

	if (!obs_frontend_add_dock_by_id(kDockId, "OBS Duel Recorder", dock_widget_)) {
		blog(LOG_WARNING, "%s dock registration failed id=%s", kLogPrefix, kDockId);
		delete dock_widget_;
		dock_widget_ = nullptr;
		return;
	}

	refresh_timer_ = new QTimer(dock_widget_);
	QObject::connect(refresh_timer_, &QTimer::timeout, [this]() { refresh(); });
	refresh_timer_->start(1000);
	refresh();
	blog(LOG_INFO, "%s dock registered id=%s", kLogPrefix, kDockId);
}

void PluginUiController::unregister_ui()
{
	if (refresh_timer_) {
		refresh_timer_->stop();
		refresh_timer_ = nullptr;
	}
	if (dock_widget_) {
		obs_frontend_remove_dock(kDockId);
		delete dock_widget_;
		dock_widget_ = nullptr;
	}
}

void PluginUiController::refresh()
{
	if (!dock_widget_) {
		return;
	}

	const WorkerStatusSnapshot snapshot = worker_manager_.status_snapshot();
	const std::string endpoint = to_utf8(snapshot.endpoint.host) + ":" + std::to_string(snapshot.endpoint.port);
	const std::string logs = snapshot.user_data_dir.empty() ? std::string{} :
				 to_utf8(snapshot.user_data_dir) + "/logs/";
	const std::wstring worker_path = snapshot.expected_worker_path.empty() ?
						 snapshot.worker_command :
						 snapshot.expected_worker_path;

	state_value_->setText(QString::fromUtf8(state_name(snapshot.state)));
	endpoint_value_->setText(qstr_utf8(endpoint));
	user_data_value_->setText(snapshot.user_data_dir.empty() ? QString::fromUtf8("not configured") : qstr_wide(snapshot.user_data_dir));
	worker_path_value_->setText(worker_path.empty() ? QString::fromUtf8("not available") : qstr_wide(worker_path));
	logs_value_->setText(logs.empty() ? QString::fromUtf8("not available") : qstr_utf8(logs));
	ownership_value_->setText(QString::fromUtf8(ownership_name(snapshot.ownership)));
	detail_value_->setText(qstr_utf8(snapshot.error.empty() ? snapshot.last_probe_summary : snapshot.error));
	action_value_->setText(QString::fromUtf8(recommended_action(snapshot.state)));
	const bool worker_running = snapshot.state == WorkerDiagnosticState::running;
	if (start_button_) {
		start_button_->setEnabled(worker_running);
	}
	if (stop_button_) {
		stop_button_->setEnabled(worker_running);
	}

	if (snapshot.overlay_state_available &&
	    (!overlay_state_applied_ ||
	     snapshot.overlay_state.deck_name != last_applied_overlay_state_.deck_name ||
	     snapshot.overlay_state.sequence_number != last_applied_overlay_state_.sequence_number ||
	     snapshot.overlay_state.result != last_applied_overlay_state_.result ||
	     snapshot.overlay_state.opponent_deck != last_applied_overlay_state_.opponent_deck ||
	     snapshot.overlay_state.recording_state != last_applied_overlay_state_.recording_state)) {
		const PluginSettings settings = load_plugin_settings();
		log_overlay_source_result(update_overlay_sources(settings.overlay, snapshot.overlay_state));
		last_applied_overlay_state_ = snapshot.overlay_state;
		overlay_state_applied_ = true;
	}
}

void PluginUiController::open_settings_from_menu(void *private_data)
{
	auto *controller = static_cast<PluginUiController *>(private_data);
	if (controller) {
		controller->show_settings_dialog();
	}
}

void PluginUiController::show_settings_dialog()
{
	PluginSettings settings = load_plugin_settings();

	QDialog dialog(dock_widget_);
	dialog.setWindowTitle("OBS Duel Recorder Settings");

	auto *layout = new QVBoxLayout(&dialog);
	auto *form = new QFormLayout;

	auto *host_input = new QLineEdit(qstr_wide(settings.endpoint.host));
	auto *port_input = new QSpinBox;
	port_input->setRange(1, 65535);
	port_input->setValue(settings.endpoint.port);
	auto *user_data_input = new QLineEdit(qstr_wide(settings.user_data_dir));
	auto *settings_path = new QLabel(qstr_wide(settings.settings_path));
	settings_path->setTextInteractionFlags(Qt::TextSelectableByMouse);
	settings_path->setWordWrap(true);

	form->addRow("Host", host_input);
	form->addRow("Port", port_input);
	form->addRow("User data dir", user_data_input);
	form->addRow("Settings file", settings_path);
	layout->addLayout(form);

	auto *note = new QLabel("Saving restarts the Worker with the persisted settings.");
	note->setWordWrap(true);
	layout->addWidget(note);

	auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
	QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
	QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
	layout->addWidget(buttons);

	if (dialog.exec() != QDialog::Accepted) {
		return;
	}

	settings.endpoint.host = host_input->text().toStdWString();
	settings.endpoint.port = static_cast<uint16_t>(port_input->value());
	settings.user_data_dir = user_data_input->text().toStdWString();
	settings.restart_worker_on_change = true;
	save_settings_and_restart(settings);
}

void PluginUiController::save_settings_and_restart(const PluginSettings &settings)
{
	if (!save_plugin_settings(settings)) {
		return;
	}

	blog(LOG_INFO, "%s settings saved path=%s host=%s port=%u user_data_dir=%s apply=worker_restart",
	     kLogPrefix, to_utf8(settings.settings_path).c_str(), to_utf8(settings.endpoint.host).c_str(),
	     settings.endpoint.port, to_utf8(settings.user_data_dir).c_str());

	log_overlay_source_result(ensure_overlay_text_sources(settings.overlay));
	overlay_state_applied_ = false;
	worker_manager_.stop();
	worker_manager_.start_async(make_launch_config(settings));
	refresh();
}

void PluginUiController::request_manual_start()
{
	RecordingCommandResult start = worker_manager_.send_recording_command("start", "manual");
	log_recording_command_result("start", start);
	if (!start.accepted()) {
		refresh();
		return;
	}

	if (obs_frontend_recording_active()) {
		RecordingCommandResult confirm = worker_manager_.send_recording_command("confirm_started", "manual");
		log_recording_command_result("confirm_started", confirm);
	} else {
		obs_frontend_recording_start();
		blog(LOG_INFO, "%s recording manual_start requested_obs_start", kLogPrefix);
	}
	refresh();
}

void PluginUiController::request_manual_stop()
{
	RecordingCommandResult stop = worker_manager_.send_recording_command("stop", "manual");
	log_recording_command_result("stop", stop);
	if (!stop.accepted()) {
		refresh();
		return;
	}

	if (obs_frontend_recording_active()) {
		obs_frontend_recording_stop();
		blog(LOG_INFO, "%s recording manual_stop requested_obs_stop", kLogPrefix);
	} else {
		RecordingCommandResult confirm = worker_manager_.send_recording_command("confirm_stopped", "manual");
		log_recording_command_result("confirm_stopped", confirm);
	}
	refresh();
}

void PluginUiController::log_recording_command_result(const char *action, const RecordingCommandResult &result)
{
	if (result.accepted()) {
		blog(LOG_INFO,
		     "%s recording command=%s accepted state=%s session_id=%s source=%s last_action=%s",
		     kLogPrefix, action, result.state.state.c_str(), result.state.session_id.c_str(),
		     result.state.command_source.c_str(), result.state.last_action.c_str());
		return;
	}

	blog(LOG_WARNING, "%s recording command=%s failed status=%d http=%lu error=%s",
	     kLogPrefix, action, static_cast<int>(result.status), result.http_status, result.error.c_str());
}

} // namespace odr::plugin
