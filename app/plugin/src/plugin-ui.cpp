#include "plugin-ui.hpp"

#include "overlay-sources.hpp"
#include "plugin-settings.hpp"

#include <obs-frontend-api.h>
#include <obs-module.h>

#include <QByteArray>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QTextEdit>
#include <QSpinBox>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

#include <sstream>
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

std::string utf8_string(const QString &value)
{
	const QByteArray bytes = value.toUtf8();
	return std::string(bytes.constData(), static_cast<size_t>(bytes.size()));
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

std::string setup_summary(const WorkerStatusSnapshot &snapshot)
{
	if (snapshot.user_data_dir.empty()) {
		return "action_required: set user_data_dir in Settings, then save.";
	}

	switch (snapshot.state) {
	case WorkerDiagnosticState::running:
		return "ready: Worker running and API compatible.";
	case WorkerDiagnosticState::starting:
		return "validating: waiting for Worker health.";
	case WorkerDiagnosticState::runtime_dir_error:
		return "runtime_path action_required: " + snapshot.error;
	case WorkerDiagnosticState::config_error:
		return "worker_launch action_required: " + snapshot.error;
	case WorkerDiagnosticState::api_incompatible:
		return "api_incompatible action_required: update Plugin and Worker together.";
	case WorkerDiagnosticState::unhealthy:
		return "endpoint_or_worker action_required: " + snapshot.error;
	case WorkerDiagnosticState::crashed:
		return "worker_crashed action_required: " + snapshot.error;
	case WorkerDiagnosticState::not_started:
		return "not_started: open Settings and save to start Worker.";
	}
	return "unknown: check OBS and Worker logs.";
}

std::string recording_summary(const WorkerStatusSnapshot &snapshot)
{
	if (!snapshot.recording_error.empty()) {
		return "command failed: " + snapshot.recording_error;
	}
	if (!snapshot.recording_state_available) {
		return "not available";
	}

	const RecordingStatePayload &state = snapshot.recording_state;
	std::ostringstream out;
	out << "state=" << state.state;
	if (!state.session_id.empty()) {
		out << " session_id=" << state.session_id;
	}
	if (!state.command_source.empty()) {
		out << " source=" << state.command_source;
	}
	if (!state.last_action.empty()) {
		out << " last_action=" << state.last_action;
	}
	if (!state.reason.empty()) {
		out << " reason=" << state.reason;
	}
	if (!state.updated_at.empty()) {
		out << " updated_at=" << state.updated_at;
	}
	return out.str();
}

std::string recording_output_summary(const WorkerStatusSnapshot &snapshot)
{
	if (!snapshot.recording_output_path.empty()) {
		std::string summary = "path=" + snapshot.recording_output_path;
		if (!snapshot.recording_output_evidence.empty()) {
			summary += " evidence=" + snapshot.recording_output_evidence;
		}
		return summary;
	}
	if (!snapshot.recording_output_evidence.empty()) {
		return snapshot.recording_output_evidence;
	}
	return "not available";
}

std::string queue_summary(const WorkerStatusSnapshot &snapshot)
{
	if (!snapshot.upload_status_available) {
		if (!snapshot.upload_status.error.empty()) {
			return "not available: " + snapshot.upload_status.error;
		}
		return "not available";
	}

	const UploadStatusResult &queue = snapshot.upload_status;
	std::ostringstream out;
	out << "ready=" << queue.ready_upload
	    << " uploading=" << queue.uploading
	    << " uploaded=" << queue.uploaded
	    << " failed=" << queue.upload_failed
	    << " quota=" << queue.quota_waiting
	    << " review=" << queue.need_manual_review
	    << " discarded=" << queue.discarded;
	return out.str();
}

std::string review_item_summary(const WorkerStatusSnapshot &snapshot)
{
	if (!snapshot.queue_action_item_available) {
		return "none";
	}
	const QueueActionItemPayload &item = snapshot.queue_action_item.item;
	std::ostringstream out;
	out << "id=" << item.id << " state=" << item.state;
	if (!item.manual_review_reason.empty()) {
		out << " reason=" << item.manual_review_reason;
	}
	if (!item.last_error_code.empty()) {
		out << " error=" << item.last_error_code;
	}
	if (!item.video_path.empty()) {
		out << " video=" << item.video_path;
	}
	return out.str();
}

QLabel *add_row(QFormLayout *layout, const char *name)
{
	auto *value = new QLabel;
	value->setTextInteractionFlags(Qt::TextSelectableByMouse);
	value->setWordWrap(true);
	value->setStyleSheet("color: #1f2933;");
	layout->addRow(QString::fromUtf8(name), value);
	return value;
}

QLabel *make_value_label()
{
	auto *value = new QLabel;
	value->setTextInteractionFlags(Qt::TextSelectableByMouse);
	value->setWordWrap(true);
	value->setStyleSheet("color: #1f2933; font-size: 12px;");
	return value;
}

QLabel *add_card_value(QVBoxLayout *layout, const char *name)
{
	auto *label = new QLabel(QString::fromUtf8(name));
	label->setStyleSheet("color: #52616b; font-size: 11px; font-weight: 600;");
	auto *value = make_value_label();
	layout->addWidget(label);
	layout->addWidget(value);
	return value;
}

QFrame *make_card(const char *background, const char *border)
{
	auto *card = new QFrame;
	card->setFrameShape(QFrame::NoFrame);
	card->setStyleSheet(QString::fromUtf8(
		"QFrame { background: %1; border: 1px solid %2; border-radius: 8px; } "
		"QLabel { border: 0; background: transparent; }")
				    .arg(QString::fromUtf8(background), QString::fromUtf8(border)));
	auto *layout = new QVBoxLayout(card);
	layout->setContentsMargins(12, 10, 12, 10);
	layout->setSpacing(6);
	return card;
}

QLabel *add_card_title(QVBoxLayout *layout, const char *title, const char *color)
{
	auto *label = new QLabel(QString::fromUtf8(title));
	label->setStyleSheet(QString::fromUtf8("color: %1; font-size: 14px; font-weight: 700;")
				     .arg(QString::fromUtf8(color)));
	layout->addWidget(label);
	return label;
}

void style_button(QPushButton *button, const char *background, const char *foreground)
{
	button->setStyleSheet(QString::fromUtf8(
		"QPushButton { background: %1; color: %2; border: 0; border-radius: 6px; "
		"padding: 6px 10px; font-weight: 600; } "
		"QPushButton:disabled { background: #d8dee7; color: #7b8794; }")
				      .arg(QString::fromUtf8(background), QString::fromUtf8(foreground)));
}

QString state_badge_style(WorkerDiagnosticState state)
{
	const char *background = "#d8dee7";
	const char *foreground = "#1f2933";
	switch (state) {
	case WorkerDiagnosticState::running:
		background = "#2ec4b6";
		foreground = "#073b3a";
		break;
	case WorkerDiagnosticState::starting:
		background = "#ffd166";
		foreground = "#573d00";
		break;
	case WorkerDiagnosticState::not_started:
		background = "#edf2ff";
		foreground = "#304c89";
		break;
	case WorkerDiagnosticState::unhealthy:
	case WorkerDiagnosticState::config_error:
	case WorkerDiagnosticState::runtime_dir_error:
	case WorkerDiagnosticState::api_incompatible:
	case WorkerDiagnosticState::crashed:
		background = "#f05d5e";
		foreground = "#ffffff";
		break;
	}
	return QString::fromUtf8(
		       "background: %1; color: %2; border-radius: 10px; padding: 4px 8px; font-weight: 700;")
		.arg(QString::fromUtf8(background), QString::fromUtf8(foreground));
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
	dock_widget_->setStyleSheet("QWidget { background: #f7f9fc; }");

	auto *root = new QVBoxLayout(dock_widget_);
	root->setContentsMargins(12, 12, 12, 12);
	root->setSpacing(10);

	auto *header = make_card("#274c77", "#274c77");
	auto *header_layout = qobject_cast<QVBoxLayout *>(header->layout());
	auto *title = new QLabel("OBS Duel Recorder");
	title->setStyleSheet("color: #ffffff; font-size: 18px; font-weight: 700;");
	header_layout->addWidget(title);
	state_value_ = new QLabel;
	state_value_->setTextInteractionFlags(Qt::TextSelectableByMouse);
	header_layout->addWidget(state_value_);
	root->addWidget(header);

	auto *setup_card = make_card("#e8f7f4", "#84d9cf");
	auto *setup_layout = qobject_cast<QVBoxLayout *>(setup_card->layout());
	add_card_title(setup_layout, "Setup", "#0f4c5c");
	setup_value_ = add_card_value(setup_layout, "Readiness");
	action_value_ = add_card_value(setup_layout, "Next action");

	auto *settings_button = new QPushButton("Settings");
	QObject::connect(settings_button, &QPushButton::clicked, [this]() { show_settings_dialog(); });
	style_button(settings_button, "#2ec4b6", "#073b3a");
	setup_layout->addWidget(settings_button);
	root->addWidget(setup_card);

	auto *recording_card = make_card("#fff3df", "#ffd08a");
	auto *recording_layout = qobject_cast<QVBoxLayout *>(recording_card->layout());
	add_card_title(recording_layout, "Recording", "#7c4700");
	recording_value_ = add_card_value(recording_layout, "State");
	output_value_ = add_card_value(recording_layout, "Output");

	auto *recording_controls = new QHBoxLayout;
	start_button_ = new QPushButton("Start Recording");
	stop_button_ = new QPushButton("Stop Recording");
	QObject::connect(start_button_, &QPushButton::clicked, [this]() { request_manual_start(); });
	QObject::connect(stop_button_, &QPushButton::clicked, [this]() { request_manual_stop(); });
	style_button(start_button_, "#ff9f1c", "#3d2400");
	style_button(stop_button_, "#f05d5e", "#ffffff");
	recording_controls->addWidget(start_button_);
	recording_controls->addWidget(stop_button_);
	recording_layout->addLayout(recording_controls);
	root->addWidget(recording_card);

	auto *upload_card = make_card("#edf2ff", "#b6c7ff");
	auto *upload_layout = qobject_cast<QVBoxLayout *>(upload_card->layout());
	add_card_title(upload_layout, "Upload Review", "#304c89");
	queue_value_ = add_card_value(upload_layout, "Queue");
	review_item_value_ = add_card_value(upload_layout, "Review item");

	auto *upload_controls = new QHBoxLayout;
	retry_upload_button_ = new QPushButton("Retry Upload");
	discard_upload_button_ = new QPushButton("Discard Upload");
	mark_uploaded_button_ = new QPushButton("Mark Uploaded");
	QObject::connect(retry_upload_button_, &QPushButton::clicked, [this]() { request_upload_retry(); });
	QObject::connect(discard_upload_button_, &QPushButton::clicked, [this]() { request_upload_discard(); });
	QObject::connect(mark_uploaded_button_, &QPushButton::clicked, [this]() { request_upload_mark_uploaded(); });
	style_button(retry_upload_button_, "#5b7cfa", "#ffffff");
	style_button(discard_upload_button_, "#6b7280", "#ffffff");
	style_button(mark_uploaded_button_, "#2ec4b6", "#073b3a");
	upload_controls->addWidget(retry_upload_button_);
	upload_controls->addWidget(discard_upload_button_);
	upload_controls->addWidget(mark_uploaded_button_);
	upload_layout->addLayout(upload_controls);
	root->addWidget(upload_card);

	auto *metadata_card = make_card("#fff7fb", "#ffb3d1");
	auto *metadata_layout = qobject_cast<QVBoxLayout *>(metadata_card->layout());
	add_card_title(metadata_layout, "Metadata", "#8a2d5d");
	auto *metadata_note = new QLabel("Review match fields and generated upload text before publishing.");
	metadata_note->setWordWrap(true);
	metadata_note->setStyleSheet("color: #5c3650; font-size: 12px;");
	metadata_layout->addWidget(metadata_note);

	edit_metadata_button_ = new QPushButton("Edit Metadata");
	preview_metadata_button_ = new QPushButton("Preview Upload Metadata");
	QObject::connect(edit_metadata_button_, &QPushButton::clicked, [this]() { request_edit_metadata(); });
	QObject::connect(preview_metadata_button_, &QPushButton::clicked, [this]() { request_preview_upload_metadata(); });
	style_button(edit_metadata_button_, "#d45087", "#ffffff");
	style_button(preview_metadata_button_, "#8a2d5d", "#ffffff");
	auto *metadata_controls = new QHBoxLayout;
	metadata_controls->addWidget(edit_metadata_button_);
	metadata_controls->addWidget(preview_metadata_button_);
	metadata_layout->addLayout(metadata_controls);
	root->addWidget(metadata_card);

	auto *diagnostics = new QGroupBox("Diagnostics");
	diagnostics->setStyleSheet(
		"QGroupBox { color: #354f52; font-weight: 700; border: 1px solid #cdd8df; "
		"border-radius: 8px; margin-top: 8px; padding: 8px; background: #f2f6f8; } "
		"QGroupBox::title { subcontrol-origin: margin; left: 8px; padding: 0 4px; } "
		"QLabel { background: transparent; }");
	auto *diagnostics_form = new QFormLayout(diagnostics);
	diagnostics_form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
	endpoint_value_ = add_row(diagnostics_form, "Endpoint");
	user_data_value_ = add_row(diagnostics_form, "User data");
	worker_path_value_ = add_row(diagnostics_form, "Worker path");
	logs_value_ = add_row(diagnostics_form, "Logs");
	ownership_value_ = add_row(diagnostics_form, "Ownership");
	detail_value_ = add_row(diagnostics_form, "Detail");
	root->addWidget(diagnostics);
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
	state_value_->setStyleSheet(state_badge_style(snapshot.state));
	setup_value_->setText(qstr_utf8(setup_summary(snapshot)));
	recording_value_->setText(qstr_utf8(recording_summary(snapshot)));
	output_value_->setText(qstr_utf8(recording_output_summary(snapshot)));
	queue_value_->setText(qstr_utf8(queue_summary(snapshot)));
	review_item_value_->setText(qstr_utf8(review_item_summary(snapshot)));
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
	const bool queue_action_available = worker_running && snapshot.queue_action_item_available;
	if (retry_upload_button_) {
		retry_upload_button_->setEnabled(queue_action_available);
	}
	if (discard_upload_button_) {
		discard_upload_button_->setEnabled(queue_action_available);
	}
	if (mark_uploaded_button_) {
		mark_uploaded_button_->setEnabled(queue_action_available &&
						  snapshot.queue_action_item.item.state == "need_manual_review");
	}
	if (edit_metadata_button_) {
		edit_metadata_button_->setEnabled(worker_running);
	}
	if (preview_metadata_button_) {
		preview_metadata_button_->setEnabled(worker_running);
	}

	handle_automatic_recording(snapshot);

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

void PluginUiController::request_upload_retry()
{
	const WorkerStatusSnapshot snapshot = worker_manager_.status_snapshot();
	if (!snapshot.queue_action_item_available) {
		return;
	}
	const QueueActionItemPayload &item = snapshot.queue_action_item.item;
	if (item.state == "need_manual_review") {
		const auto answer = QMessageBox::warning(
			dock_widget_,
			"Retry upload",
			"Retrying a manual-review item can create a duplicate YouTube upload. Continue only after checking YouTube.",
			QMessageBox::Yes | QMessageBox::No,
			QMessageBox::No);
		if (answer != QMessageBox::Yes) {
			return;
		}
	}
	QueueCommandResult result = worker_manager_.send_queue_command(item.id, "retry");
	log_queue_command_result("retry", result);
	refresh();
}

void PluginUiController::request_upload_discard()
{
	const WorkerStatusSnapshot snapshot = worker_manager_.status_snapshot();
	if (!snapshot.queue_action_item_available) {
		return;
	}
	const auto answer = QMessageBox::question(
		dock_widget_,
		"Discard upload",
		"Discard this upload queue item? This keeps the local video file but removes it from upload work.",
		QMessageBox::Yes | QMessageBox::No,
		QMessageBox::No);
	if (answer != QMessageBox::Yes) {
		return;
	}
	const QueueCommandResult result = worker_manager_.send_queue_command(snapshot.queue_action_item.item.id, "discard");
	log_queue_command_result("discard", result);
	refresh();
}

void PluginUiController::request_upload_mark_uploaded()
{
	const WorkerStatusSnapshot snapshot = worker_manager_.status_snapshot();
	if (!snapshot.queue_action_item_available) {
		return;
	}
	bool ok = false;
	const QString video_id = QInputDialog::getText(
		dock_widget_,
		"Mark uploaded",
		"YouTube video id",
		QLineEdit::Normal,
		QString(),
		&ok);
	if (!ok || video_id.trimmed().isEmpty()) {
		return;
	}
	const QueueCommandResult result = worker_manager_.send_queue_command(
		snapshot.queue_action_item.item.id,
		"mark_uploaded",
		video_id.trimmed().toStdString());
	log_queue_command_result("mark_uploaded", result);
	refresh();
}

void PluginUiController::request_edit_metadata()
{
	const MatchFetchResult fetched = worker_manager_.fetch_latest_match();
	if (!fetched.reachable()) {
		const QString message = fetched.status == MatchFetchStatus::not_found ?
						QString::fromUtf8("No completed match metadata is available yet.") :
						qstr_utf8(fetched.error.empty() ? "Worker metadata API is unavailable." : fetched.error);
		QMessageBox::warning(dock_widget_, "Edit metadata", message);
		return;
	}

	QDialog dialog(dock_widget_);
	dialog.setWindowTitle(QString::fromUtf8("Edit Match Metadata"));

	auto *layout = new QVBoxLayout(&dialog);
	auto *form = new QFormLayout;
	auto *deck_input = new QLineEdit(qstr_utf8(fetched.match.deck_name));
	auto *opponent_input = new QLineEdit(qstr_utf8(fetched.match.opponent_deck));
	auto *result_input = new QLineEdit(qstr_utf8(fetched.match.result));
	auto *rank_input = new QLineEdit(qstr_utf8(fetched.match.rank));
	auto *dp_input = new QLineEdit(qstr_utf8(fetched.match.dp));
	auto *memo_input = new QTextEdit(qstr_utf8(fetched.match.memo));
	memo_input->setAcceptRichText(false);

	form->addRow("Deck", deck_input);
	form->addRow("Opponent deck", opponent_input);
	form->addRow("Result", result_input);
	form->addRow("Rank", rank_input);
	form->addRow("DP", dp_input);
	form->addRow("Memo", memo_input);
	layout->addLayout(form);

	auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
	QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
	QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
	layout->addWidget(buttons);

	if (dialog.exec() != QDialog::Accepted) {
		return;
	}

	MatchMetadataPayload updated = fetched.match;
	updated.deck_name = utf8_string(deck_input->text().trimmed());
	updated.opponent_deck = utf8_string(opponent_input->text().trimmed());
	updated.result = utf8_string(result_input->text().trimmed());
	updated.rank = utf8_string(rank_input->text().trimmed());
	updated.dp = utf8_string(dp_input->text().trimmed());
	updated.memo = utf8_string(memo_input->toPlainText().trimmed());

	const MetadataUpdateResult result = worker_manager_.update_match_metadata(updated);
	if (!result.accepted()) {
		QMessageBox::warning(
			dock_widget_,
			"Edit metadata",
			qstr_utf8(result.error.empty() ? "Metadata was rejected by the Worker." : result.error));
		blog(LOG_WARNING, "%s metadata update failed status=%d http=%lu id=%d error=%s",
		     kLogPrefix, static_cast<int>(result.status), result.http_status, updated.id, result.error.c_str());
		return;
	}

	blog(LOG_INFO, "%s metadata update accepted id=%d", kLogPrefix, result.match.id);
	refresh();
}

void PluginUiController::request_preview_upload_metadata()
{
	const MatchFetchResult fetched = worker_manager_.fetch_latest_match();
	if (!fetched.reachable()) {
		const QString message = fetched.status == MatchFetchStatus::not_found ?
						QString::fromUtf8("No completed match metadata is available yet.") :
						qstr_utf8(fetched.error.empty() ? "Worker metadata API is unavailable." : fetched.error);
		QMessageBox::warning(dock_widget_, "Preview upload metadata", message);
		return;
	}

	const UploadMetadataPreviewResult preview =
		worker_manager_.fetch_upload_metadata_preview(fetched.match.id);
	if (!preview.reachable()) {
		QMessageBox::warning(
			dock_widget_,
			"Preview upload metadata",
			qstr_utf8(preview.error.empty() ? "Upload metadata preview is unavailable." : preview.error));
		blog(LOG_WARNING, "%s upload metadata preview failed status=%d http=%lu id=%d error=%s",
		     kLogPrefix, static_cast<int>(preview.status), preview.http_status, fetched.match.id,
		     preview.error.c_str());
		return;
	}

	QDialog dialog(dock_widget_);
	dialog.setWindowTitle(QString::fromUtf8("Upload Metadata Preview"));

	auto *layout = new QVBoxLayout(&dialog);
	auto *form = new QFormLayout;
	auto *title_preview = new QLineEdit(qstr_utf8(preview.preview.title));
	title_preview->setReadOnly(true);
	auto *description_preview = new QTextEdit(qstr_utf8(preview.preview.description));
	description_preview->setReadOnly(true);
	description_preview->setAcceptRichText(false);
	auto *warning = new QLabel(qstr_utf8(
		preview.preview.warning.empty() ?
			std::string("Metadata is complete for the current preview.") :
			preview.preview.warning + ". Use Edit Metadata before uploading if needed."));
	warning->setWordWrap(true);

	form->addRow("Title", title_preview);
	form->addRow("Description", description_preview);
	layout->addLayout(form);
	layout->addWidget(warning);

	auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close);
	QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
	layout->addWidget(buttons);
	dialog.exec();

	blog(LOG_INFO, "%s upload metadata preview shown id=%d", kLogPrefix, preview.preview.match_id);
}

void PluginUiController::handle_automatic_recording(const WorkerStatusSnapshot &snapshot)
{
	if (!snapshot.recording_state_available ||
	    snapshot.recording_state.command_source != "automatic") {
		return;
	}

	const RecordingStatePayload &state = snapshot.recording_state;
	const std::string request_key = state.session_id + ":" + state.state + ":" + state.last_action;
	if (state.state == "starting") {
		if (obs_frontend_recording_active()) {
			RecordingCommandResult confirm = worker_manager_.send_recording_command("confirm_started", "automatic");
			log_recording_command_result("confirm_started", confirm);
			automatic_recording_request_key_.clear();
			return;
		}
		if (automatic_recording_request_key_ == request_key) {
			return;
		}
		automatic_recording_request_key_ = request_key;
		obs_frontend_recording_start();
		blog(LOG_INFO, "%s recording automatic_start requested_obs_start session_id=%s",
		     kLogPrefix, state.session_id.c_str());
		return;
	}

	if (state.state == "stopping") {
		if (obs_frontend_recording_active()) {
			if (automatic_recording_request_key_ == request_key) {
				return;
			}
			automatic_recording_request_key_ = request_key;
			obs_frontend_recording_stop();
			blog(LOG_INFO, "%s recording automatic_stop requested_obs_stop session_id=%s",
			     kLogPrefix, state.session_id.c_str());
			return;
		}
		RecordingCommandResult confirm = worker_manager_.send_recording_command("confirm_stopped", "automatic");
		log_recording_command_result("confirm_stopped", confirm);
		automatic_recording_request_key_.clear();
		return;
	}

	if (state.state == "recording" || state.state == "completed" || state.state == "idle") {
		automatic_recording_request_key_.clear();
	}
}

void PluginUiController::log_queue_command_result(const char *action, const QueueCommandResult &result)
{
	if (result.accepted()) {
		blog(LOG_INFO,
		     "%s queue command=%s accepted id=%d state=%s",
		     kLogPrefix, action, result.item.id, result.item.state.c_str());
		return;
	}

	blog(LOG_WARNING, "%s queue command=%s failed status=%d http=%lu error=%s",
	     kLogPrefix, action, static_cast<int>(result.status), result.http_status, result.error.c_str());
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
