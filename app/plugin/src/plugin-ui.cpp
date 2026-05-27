#include "plugin-ui.hpp"

#include "overlay-sources.hpp"
#include "plugin-settings.hpp"

#include <obs-frontend-api.h>
#include <obs-module.h>

#include <QByteArray>
#include <QDialog>
#include <QDialogButtonBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QTabWidget>
#include <QTextEdit>
#include <QSpinBox>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

#include <sstream>
#include <string>
#include <vector>

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

bool language_is_japanese(const std::string &language)
{
	return language == "ja";
}

QString ui_text(const std::string &language, const char *english, const char *japanese)
{
	return QString::fromUtf8(language_is_japanese(language) ? japanese : english);
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
		return "Error";
	}
	if (!snapshot.recording_state_available) {
		return "Unavailable";
	}

	const RecordingStatePayload &state = snapshot.recording_state;
	if (state.state == "recording") {
		return "Recording";
	}
	if (state.state == "completed") {
		return "Metadata needed";
	}
	if (state.state == "idle") {
		return "Idle";
	}
	return state.state.empty() ? "Unknown" : state.state;
}

std::string recording_output_summary(const WorkerStatusSnapshot &snapshot)
{
	if (!snapshot.recording_output_path.empty()) {
		return "Linked";
	}
	if (!snapshot.recording_output_evidence.empty()) {
		return "Waiting for OBS output";
	}
	return "Not linked";
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
	out << "Ready " << queue.ready_upload << " / Review " << queue.need_manual_review
	    << " / Failed " << queue.upload_failed;
	return out.str();
}

std::string review_item_summary(const WorkerStatusSnapshot &snapshot)
{
	if (!snapshot.queue_action_item_available) {
		return "none";
	}
	const QueueActionItemPayload &item = snapshot.queue_action_item.item;
	std::ostringstream out;
	out << "#" << item.id << " " << item.state;
	return out.str();
}

QLabel *add_row(QFormLayout *layout, const char *name, QLabel **name_label = nullptr)
{
	auto *label = new QLabel(QString::fromUtf8(name));
	if (name_label != nullptr) {
		*name_label = label;
	}
	auto *value = new QLabel;
	value->setTextInteractionFlags(Qt::TextSelectableByMouse);
	value->setWordWrap(true);
	value->setStyleSheet("color: #1f2933;");
	layout->addRow(label, value);
	return value;
}

struct DockThemePalette {
	const char *name;
	const char *window_bg;
	const char *header_bg;
	const char *header_border;
	const char *setup_bg;
	const char *setup_border;
	const char *recording_bg;
	const char *recording_border;
	const char *upload_bg;
	const char *upload_border;
	const char *metadata_bg;
	const char *metadata_border;
	const char *diagnostics_bg;
	const char *diagnostics_border;
	const char *settings_bg;
	const char *settings_fg;
	const char *start_bg;
	const char *start_fg;
	const char *stop_bg;
	const char *stop_fg;
	const char *upload_bg_button;
	const char *upload_fg_button;
	const char *secondary_bg;
	const char *secondary_fg;
	const char *metadata_bg_button;
	const char *metadata_fg_button;
};

const DockThemePalette &palette_for_theme(const std::string &theme)
{
	static const DockThemePalette classic{
		"classic", "#f7f9fc", "#274c77", "#274c77", "#e8f7f4", "#84d9cf", "#fff3df",
		"#ffd08a", "#edf2ff", "#b6c7ff", "#fff7fb", "#ffb3d1", "#f2f6f8", "#cdd8df",
		"#2ec4b6", "#073b3a", "#ff9f1c", "#3d2400", "#f05d5e", "#ffffff", "#5b7cfa",
		"#ffffff", "#6b7280", "#ffffff", "#d45087", "#ffffff"};
	static const DockThemePalette forest{
		"forest", "#f5f8f4", "#354f52", "#354f52", "#ecf6ef", "#84a98c", "#fff4df",
		"#f4a261", "#edf7f6", "#86c5b8", "#f7eff6", "#c9a0c8", "#eef3ef", "#cad2c5",
		"#84a98c", "#16332b", "#f4a261", "#3a2508", "#bc4749", "#ffffff", "#52796f",
		"#ffffff", "#6b705c", "#ffffff", "#9d4edd", "#ffffff"};
	static const DockThemePalette bright{
		"bright", "#fbf8ff", "#5b3c88", "#5b3c88", "#eef7ff", "#bde0fe", "#fff8df",
		"#ffd166", "#f4f0ff", "#cdb4db", "#fff0f6", "#ffafcc", "#f6f2fb", "#d7c7ef",
		"#00b4d8", "#073b4c", "#ffd166", "#3a2a00", "#ef476f", "#ffffff", "#7b2cbf",
		"#ffffff", "#6c757d", "#ffffff", "#ff5d8f", "#ffffff"};

	if (theme == "forest") {
		return forest;
	}
	if (theme == "bright") {
		return bright;
	}
	return classic;
}

void style_card(QFrame *card, const char *background, const char *border)
{
	if (!card) {
		return;
	}
	card->setStyleSheet(QString::fromUtf8(
		"QFrame { background: %1; border: 1px solid %2; border-radius: 8px; } "
		"QLabel { border: 0; background: transparent; }")
				    .arg(QString::fromUtf8(background), QString::fromUtf8(border)));
}

QLabel *make_value_label()
{
	auto *value = new QLabel;
	value->setTextInteractionFlags(Qt::TextSelectableByMouse);
	value->setWordWrap(true);
	value->setStyleSheet("color: #1f2933; font-size: 12px;");
	return value;
}

QLabel *add_card_value(QVBoxLayout *layout, const char *name, QLabel **name_label = nullptr)
{
	auto *label = new QLabel(QString::fromUtf8(name));
	if (name_label != nullptr) {
		*name_label = label;
	}
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
	style_card(card, background, border);
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

void add_unique_candidate(std::vector<std::string> &values, const std::string &value)
{
	if (value.empty()) {
		return;
	}
	if (std::find(values.begin(), values.end(), value) != values.end()) {
		return;
	}
	values.insert(values.begin(), value);
	if (values.size() > 30) {
		values.resize(30);
	}
}

std::string combo_text(QComboBox *combo)
{
	return combo ? utf8_string(combo->currentText().trimmed()) : std::string{};
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
	PluginSettings settings = load_plugin_settings();
	dock_theme_ = settings.dock_theme;
	ui_language_ = settings.ui_language;

	auto *root = new QVBoxLayout(dock_widget_);
	root->setContentsMargins(12, 12, 12, 12);
	root->setSpacing(10);

	header_card_ = make_card("#274c77", "#274c77");
	auto *header_layout = qobject_cast<QVBoxLayout *>(header_card_->layout());
	auto *title = new QLabel("OBS Duel Recorder");
	title->setStyleSheet("color: #ffffff; font-size: 18px; font-weight: 700;");
	header_layout->addWidget(title);
	state_value_ = new QLabel;
	state_value_->setTextInteractionFlags(Qt::TextSelectableByMouse);
	header_layout->addWidget(state_value_);
	root->addWidget(header_card_);

	dock_tabs_ = new QTabWidget;
	dock_tabs_->setDocumentMode(true);
	root->addWidget(dock_tabs_);

	auto *recording_tab = new QWidget;
	auto *recording_tab_layout = new QVBoxLayout(recording_tab);
	recording_tab_layout->setContentsMargins(0, 0, 0, 0);
	recording_tab_layout->setSpacing(10);

	auto *setup_tab = new QWidget;
	auto *setup_tab_layout = new QVBoxLayout(setup_tab);
	setup_tab_layout->setContentsMargins(0, 0, 0, 0);
	setup_tab_layout->setSpacing(10);

	auto *diagnostics_tab = new QWidget;
	auto *diagnostics_tab_layout = new QVBoxLayout(diagnostics_tab);
	diagnostics_tab_layout->setContentsMargins(0, 0, 0, 0);
	diagnostics_tab_layout->setSpacing(10);

	auto *settings_tab = new QWidget;
	auto *settings_tab_layout = new QVBoxLayout(settings_tab);
	settings_tab_layout->setContentsMargins(0, 0, 0, 0);
	settings_tab_layout->setSpacing(10);

	auto *help_tab = new QWidget;
	auto *help_tab_layout = new QVBoxLayout(help_tab);
	help_tab_layout->setContentsMargins(0, 0, 0, 0);
	help_tab_layout->setSpacing(10);

	auto *automatic_tab = new QWidget;
	auto *automatic_tab_layout = new QVBoxLayout(automatic_tab);
	automatic_tab_layout->setContentsMargins(0, 0, 0, 0);
	automatic_tab_layout->setSpacing(10);

	setup_card_ = make_card("#e8f7f4", "#84d9cf");
	auto *setup_layout = qobject_cast<QVBoxLayout *>(setup_card_->layout());
	setup_title_ = add_card_title(setup_layout, language_is_japanese(ui_language_) ? "セットアップ" : "Setup", "#0f4c5c");
	setup_value_ = add_card_value(setup_layout, language_is_japanese(ui_language_) ? "準備状態" : "Readiness",
				      &setup_label_);
	action_value_ = add_card_value(setup_layout, language_is_japanese(ui_language_) ? "次の操作" : "Next action",
				       &action_label_);
	setup_tab_layout->addWidget(setup_card_);
	settings_button_ = new QPushButton(ui_text(ui_language_, "Open Settings", "設定を開く"));
	help_button_ = new QPushButton(ui_text(ui_language_, "Open Help", "ヘルプを開く"));
	automatic_setup_button_ = new QPushButton(ui_text(ui_language_, "Open Automatic Setup", "自動録画セットアップを開く"));
	QObject::connect(settings_button_, &QPushButton::clicked, [this]() { show_settings_dialog(); });
	QObject::connect(help_button_, &QPushButton::clicked, [this]() { request_show_help(); });
	QObject::connect(automatic_setup_button_, &QPushButton::clicked, [this]() { request_automatic_setup(); });
	style_button(settings_button_, "#2ec4b6", "#073b3a");
	style_button(help_button_, "#52796f", "#ffffff");
	style_button(automatic_setup_button_, "#52796f", "#ffffff");

	settings_note_ = new QLabel(ui_text(
		ui_language_,
		"Change runtime path, theme, and language. Saving restarts the Worker.",
		"実行データ保存先、テーマ、言語を変更します。保存すると Worker を再起動します。"));
	settings_note_->setWordWrap(true);
	setup_tab_layout->addWidget(settings_note_);

	auto *inline_settings_form = new QFormLayout;
	settings_host_input_ = new QLineEdit(qstr_wide(settings.endpoint.host));
	settings_port_input_ = new QLineEdit(QString::number(settings.endpoint.port));
	settings_user_data_input_ = new QLineEdit(qstr_wide(settings.user_data_dir));
	settings_theme_input_ = new QComboBox;
	settings_theme_input_->addItem("Classic", QString::fromUtf8("classic"));
	settings_theme_input_->addItem("Forest", QString::fromUtf8("forest"));
	settings_theme_input_->addItem("Bright", QString::fromUtf8("bright"));
	settings_theme_input_->setCurrentIndex(std::max(0, settings_theme_input_->findData(QString::fromUtf8(settings.dock_theme.c_str()))));
	settings_language_input_ = new QComboBox;
	settings_language_input_->addItem("English", QString::fromUtf8("en"));
	settings_language_input_->addItem(QString::fromUtf8("日本語"), QString::fromUtf8("ja"));
	settings_language_input_->setCurrentIndex(std::max(0, settings_language_input_->findData(QString::fromUtf8(settings.ui_language.c_str()))));
	inline_settings_form->addRow(ui_text(ui_language_, "Host", "ホスト"), settings_host_input_);
	inline_settings_form->addRow(ui_text(ui_language_, "Port", "ポート"), settings_port_input_);
	inline_settings_form->addRow(ui_text(ui_language_, "User data dir", "ユーザーデータ保存先"), settings_user_data_input_);
	inline_settings_form->addRow(ui_text(ui_language_, "Dock theme", "Dock テーマ"), settings_theme_input_);
	inline_settings_form->addRow(ui_text(ui_language_, "Language", "言語"), settings_language_input_);
	setup_tab_layout->addLayout(inline_settings_form);
	save_inline_settings_button_ = new QPushButton(ui_text(ui_language_, "Save Settings", "設定を保存"));
	QObject::connect(save_inline_settings_button_, &QPushButton::clicked, [this]() { save_inline_settings(); });
	style_button(save_inline_settings_button_, "#2ec4b6", "#073b3a");
	setup_tab_layout->addWidget(save_inline_settings_button_);
	setup_tab_layout->addWidget(settings_button_);

	help_note_ = new QLabel(ui_text(
		ui_language_,
		"Open task-focused help for setup, recording, upload review, diagnostics, and language recovery.",
		"セットアップ、録画、アップロード確認、診断、言語復旧のヘルプを開きます。"));
	help_note_->setWordWrap(true);
	setup_tab_layout->addWidget(help_note_);
	setup_tab_layout->addWidget(help_button_);

	automatic_note_ = new QLabel(ui_text(
		ui_language_,
		"Register local start/end templates and test detection before relying on automatic recording.",
		"自動録画を使う前に、ローカルの開始/終了テンプレートを登録して検出テストを実行します。"));
	automatic_note_->setWordWrap(true);
	setup_tab_layout->addWidget(automatic_note_);
	setup_tab_layout->addWidget(automatic_setup_button_);

	recording_card_ = make_card("#fff3df", "#ffd08a");
	auto *recording_layout = qobject_cast<QVBoxLayout *>(recording_card_->layout());
	recording_title_ = add_card_title(recording_layout, language_is_japanese(ui_language_) ? "録画" : "Recording",
					  "#7c4700");
	recording_value_ = add_card_value(recording_layout, language_is_japanese(ui_language_) ? "状態" : "State",
					  &recording_label_);
	output_value_ = add_card_value(recording_layout, language_is_japanese(ui_language_) ? "出力" : "Output",
				       &output_label_);

	auto *recording_controls = new QHBoxLayout;
	start_button_ = new QPushButton(ui_text(ui_language_, "Start Recording", "録画開始"));
	stop_button_ = new QPushButton(ui_text(ui_language_, "Stop Recording", "録画停止"));
	QObject::connect(start_button_, &QPushButton::clicked, [this]() { request_manual_start(); });
	QObject::connect(stop_button_, &QPushButton::clicked, [this]() { request_manual_stop(); });
	style_button(start_button_, "#ff9f1c", "#3d2400");
	style_button(stop_button_, "#f05d5e", "#ffffff");
	recording_controls->addWidget(start_button_);
	recording_controls->addWidget(stop_button_);
	recording_layout->addLayout(recording_controls);
	recording_tab_layout->addWidget(recording_card_);

	upload_card_ = make_card("#edf2ff", "#b6c7ff");
	auto *upload_layout = qobject_cast<QVBoxLayout *>(upload_card_->layout());
	upload_title_ = add_card_title(upload_layout, language_is_japanese(ui_language_) ? "アップロード確認" : "Upload Review",
				       "#304c89");
	queue_value_ = add_card_value(upload_layout, language_is_japanese(ui_language_) ? "キュー" : "Queue", &queue_label_);
	review_item_value_ = add_card_value(upload_layout, language_is_japanese(ui_language_) ? "確認項目" : "Review item",
					    &review_item_label_);

	auto *upload_controls = new QHBoxLayout;
	retry_upload_button_ = new QPushButton(ui_text(ui_language_, "Retry Upload", "再試行"));
	discard_upload_button_ = new QPushButton(ui_text(ui_language_, "Discard Upload", "破棄"));
	mark_uploaded_button_ = new QPushButton(ui_text(ui_language_, "Mark Uploaded", "アップロード済みにする"));
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
	automatic_tab_layout->addWidget(upload_card_);

	metadata_card_ = make_card("#fff7fb", "#ffb3d1");
	auto *metadata_layout = qobject_cast<QVBoxLayout *>(metadata_card_->layout());
	metadata_title_ = add_card_title(metadata_layout, language_is_japanese(ui_language_) ? "メタデータ" : "Metadata",
					 "#8a2d5d");
	metadata_note_ = new QLabel(ui_text(
		ui_language_,
		"Review match fields and generated upload text before publishing.",
		"公開前に対戦情報と生成されたアップロード文面を確認します。"));
	metadata_note_->setWordWrap(true);
	metadata_note_->setStyleSheet("color: #5c3650; font-size: 12px;");
	metadata_layout->addWidget(metadata_note_);

	metadata_match_label_ = make_value_label();
	metadata_video_label_ = make_value_label();
	metadata_status_label_ = make_value_label();
	metadata_layout->addWidget(metadata_match_label_);
	metadata_layout->addWidget(metadata_video_label_);

	auto *metadata_form = new QFormLayout;
	metadata_deck_input_ = new QComboBox;
	metadata_deck_input_->setEditable(true);
	for (const std::string &candidate : settings.deck_candidates) {
		metadata_deck_input_->addItem(qstr_utf8(candidate));
	}
	metadata_opponent_input_ = new QComboBox;
	metadata_opponent_input_->setEditable(true);
	for (const std::string &candidate : settings.opponent_deck_candidates) {
		metadata_opponent_input_->addItem(qstr_utf8(candidate));
	}
	metadata_result_input_ = new QLineEdit;
	metadata_rank_input_ = new QLineEdit(qstr_utf8(settings.last_rank));
	metadata_dp_input_ = new QLineEdit(qstr_utf8(settings.last_dp));
	metadata_memo_input_ = new QTextEdit;
	metadata_memo_input_->setMaximumHeight(80);
	metadata_form->addRow(ui_text(ui_language_, "Deck", "自分のデッキ"), metadata_deck_input_);
	metadata_form->addRow(ui_text(ui_language_, "Opponent deck", "相手のデッキ"), metadata_opponent_input_);
	metadata_form->addRow(ui_text(ui_language_, "Result", "結果"), metadata_result_input_);
	metadata_form->addRow(ui_text(ui_language_, "Rank", "ランク"), metadata_rank_input_);
	metadata_form->addRow(ui_text(ui_language_, "DP", "DP"), metadata_dp_input_);
	metadata_form->addRow(ui_text(ui_language_, "Memo", "メモ"), metadata_memo_input_);
	metadata_layout->addLayout(metadata_form);

	edit_metadata_button_ = new QPushButton(ui_text(ui_language_, "Edit Metadata", "メタデータ編集"));
	preview_metadata_button_ = new QPushButton(ui_text(ui_language_, "Preview Upload Metadata", "アップロード文面プレビュー"));
	reload_metadata_button_ = new QPushButton(ui_text(ui_language_, "Reload", "再読込"));
	save_metadata_button_ = new QPushButton(ui_text(ui_language_, "Save", "保存"));
	QObject::connect(edit_metadata_button_, &QPushButton::clicked, [this]() { request_edit_metadata(); });
	QObject::connect(preview_metadata_button_, &QPushButton::clicked, [this]() { request_preview_upload_metadata(); });
	QObject::connect(reload_metadata_button_, &QPushButton::clicked, [this]() { load_latest_metadata_into_dock(); });
	QObject::connect(save_metadata_button_, &QPushButton::clicked, [this]() { save_metadata_from_dock(); });
	style_button(edit_metadata_button_, "#d45087", "#ffffff");
	style_button(preview_metadata_button_, "#8a2d5d", "#ffffff");
	style_button(reload_metadata_button_, "#6b7280", "#ffffff");
	style_button(save_metadata_button_, "#d45087", "#ffffff");
	auto *metadata_controls = new QHBoxLayout;
	metadata_controls->addWidget(reload_metadata_button_);
	metadata_controls->addWidget(save_metadata_button_);
	metadata_controls->addWidget(edit_metadata_button_);
	metadata_controls->addWidget(preview_metadata_button_);
	metadata_layout->addLayout(metadata_controls);
	metadata_layout->addWidget(metadata_status_label_);
	settings_tab_layout->addWidget(metadata_card_);
	settings_tab_layout->addStretch(1);

	upload_template_card_ = make_card("#fff7fb", "#ffb3d1");
	auto *upload_template_layout = qobject_cast<QVBoxLayout *>(upload_template_card_->layout());
	upload_preview_title_label_ = add_card_title(upload_template_layout,
						     language_is_japanese(ui_language_) ? "アップロード文面" : "Upload text",
						     "#8a2d5d");
	auto *template_form = new QFormLayout;
	upload_title_template_input_ = new QLineEdit(qstr_utf8(settings.upload_title_template));
	upload_description_template_input_ = new QTextEdit(qstr_utf8(settings.upload_description_template));
	upload_description_template_input_->setMaximumHeight(110);
	upload_tags_template_input_ = new QLineEdit(qstr_utf8(settings.upload_tags_template));
	template_form->addRow(ui_text(ui_language_, "Title template", "タイトルテンプレート"), upload_title_template_input_);
	template_form->addRow(ui_text(ui_language_, "Description template", "説明文テンプレート"), upload_description_template_input_);
	template_form->addRow(ui_text(ui_language_, "Tags template", "タグテンプレート"), upload_tags_template_input_);
	upload_template_layout->addLayout(template_form);
	render_upload_preview_button_ = new QPushButton(ui_text(ui_language_, "Preview", "プレビュー"));
	QObject::connect(render_upload_preview_button_, &QPushButton::clicked, [this]() { render_upload_preview_from_dock(); });
	style_button(render_upload_preview_button_, "#8a2d5d", "#ffffff");
	upload_template_layout->addWidget(render_upload_preview_button_);
	upload_preview_description_ = new QTextEdit;
	upload_preview_description_->setReadOnly(true);
	upload_preview_description_->setMaximumHeight(130);
	upload_preview_warning_label_ = make_value_label();
	upload_template_layout->addWidget(upload_preview_description_);
	upload_template_layout->addWidget(upload_preview_warning_label_);
	automatic_tab_layout->addWidget(upload_template_card_);
	automatic_tab_layout->addStretch(1);

	diagnostics_group_ = new QGroupBox(ui_text(ui_language_, "Diagnostics", "診断"));
	auto *diagnostics_form = new QFormLayout(diagnostics_group_);
	diagnostics_form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
	endpoint_value_ = add_row(diagnostics_form, "Endpoint", &endpoint_label_);
	user_data_value_ = add_row(diagnostics_form, "User data", &user_data_label_);
	worker_path_value_ = add_row(diagnostics_form, "Worker path", &worker_path_label_);
	logs_value_ = add_row(diagnostics_form, "Logs", &logs_label_);
	ownership_value_ = add_row(diagnostics_form, "Ownership", &ownership_label_);
	detail_value_ = add_row(diagnostics_form, "Detail", &detail_label_);
	diagnostics_details_button_ = new QPushButton(ui_text(ui_language_, "Show Details", "詳細を表示"));
	QObject::connect(diagnostics_details_button_, &QPushButton::clicked, [this]() { toggle_diagnostics_details(); });
	style_button(diagnostics_details_button_, "#6b7280", "#ffffff");
	setup_tab_layout->addWidget(diagnostics_details_button_);
	setup_tab_layout->addWidget(diagnostics_group_);
	diagnostics_group_->setVisible(false);
	setup_tab_layout->addStretch(1);

	dock_tabs_->addTab(recording_tab, ui_text(ui_language_, "Record", "録画"));
	dock_tabs_->addTab(settings_tab, ui_text(ui_language_, "Metadata", "メタデータ"));
	dock_tabs_->addTab(automatic_tab, ui_text(ui_language_, "Upload", "アップロード"));
	dock_tabs_->addTab(setup_tab, ui_text(ui_language_, "Manage", "管理"));
	apply_ui_language();
	apply_dock_theme(dock_theme_);

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
	load_latest_metadata_into_dock();
	blog(LOG_INFO, "%s dock registered id=%s", kLogPrefix, kDockId);
}

void PluginUiController::apply_dock_theme(const std::string &theme)
{
	const DockThemePalette &palette = palette_for_theme(theme);
	dock_theme_ = palette.name;
	if (!dock_widget_) {
		return;
	}

	dock_widget_->setStyleSheet(QString::fromUtf8("QWidget { background: %1; }")
					    .arg(QString::fromUtf8(palette.window_bg)));
	style_card(header_card_, palette.header_bg, palette.header_border);
	style_card(setup_card_, palette.setup_bg, palette.setup_border);
	style_card(recording_card_, palette.recording_bg, palette.recording_border);
	style_card(upload_card_, palette.upload_bg, palette.upload_border);
	style_card(metadata_card_, palette.metadata_bg, palette.metadata_border);
	style_card(upload_template_card_, palette.metadata_bg, palette.metadata_border);
	if (diagnostics_group_) {
		diagnostics_group_->setStyleSheet(QString::fromUtf8(
			"QGroupBox { color: #354f52; font-weight: 700; border: 1px solid %1; "
			"border-radius: 8px; margin-top: 8px; padding: 8px; background: %2; } "
			"QGroupBox::title { subcontrol-origin: margin; left: 8px; padding: 0 4px; } "
			"QLabel { background: transparent; }")
							    .arg(QString::fromUtf8(palette.diagnostics_border),
								 QString::fromUtf8(palette.diagnostics_bg)));
	}

	if (settings_button_) {
		style_button(settings_button_, palette.settings_bg, palette.settings_fg);
	}
	if (help_button_) {
		style_button(help_button_, palette.secondary_bg, palette.secondary_fg);
	}
	if (automatic_setup_button_) {
		style_button(automatic_setup_button_, palette.upload_bg_button, palette.upload_fg_button);
	}
	if (start_button_) {
		style_button(start_button_, palette.start_bg, palette.start_fg);
	}
	if (stop_button_) {
		style_button(stop_button_, palette.stop_bg, palette.stop_fg);
	}
	if (retry_upload_button_) {
		style_button(retry_upload_button_, palette.upload_bg_button, palette.upload_fg_button);
	}
	if (discard_upload_button_) {
		style_button(discard_upload_button_, palette.secondary_bg, palette.secondary_fg);
	}
	if (mark_uploaded_button_) {
		style_button(mark_uploaded_button_, palette.settings_bg, palette.settings_fg);
	}
	if (edit_metadata_button_) {
		style_button(edit_metadata_button_, palette.metadata_bg_button, palette.metadata_fg_button);
	}
	if (preview_metadata_button_) {
		style_button(preview_metadata_button_, palette.header_bg, "#ffffff");
	}
	if (reload_metadata_button_) {
		style_button(reload_metadata_button_, palette.secondary_bg, palette.secondary_fg);
	}
	if (save_metadata_button_) {
		style_button(save_metadata_button_, palette.metadata_bg_button, palette.metadata_fg_button);
	}
	if (render_upload_preview_button_) {
		style_button(render_upload_preview_button_, palette.header_bg, "#ffffff");
	}
	if (save_inline_settings_button_) {
		style_button(save_inline_settings_button_, palette.settings_bg, palette.settings_fg);
	}
	if (diagnostics_details_button_) {
		style_button(diagnostics_details_button_, palette.secondary_bg, palette.secondary_fg);
	}
}

void PluginUiController::apply_ui_language()
{
	if (dock_tabs_) {
		dock_tabs_->setTabText(0, ui_text(ui_language_, "Record", "録画"));
		dock_tabs_->setTabText(1, ui_text(ui_language_, "Metadata", "メタデータ"));
		dock_tabs_->setTabText(2, ui_text(ui_language_, "Upload", "アップロード"));
		dock_tabs_->setTabText(3, ui_text(ui_language_, "Manage", "管理"));
	}
	if (setup_title_) {
		setup_title_->setText(ui_text(ui_language_, "Setup", "セットアップ"));
	}
	if (setup_label_) {
		setup_label_->setText(ui_text(ui_language_, "Readiness", "準備状態"));
	}
	if (action_label_) {
		action_label_->setText(ui_text(ui_language_, "Next action", "次の操作"));
	}
	if (recording_title_) {
		recording_title_->setText(ui_text(ui_language_, "Recording", "録画"));
	}
	if (recording_label_) {
		recording_label_->setText(ui_text(ui_language_, "State", "状態"));
	}
	if (output_label_) {
		output_label_->setText(ui_text(ui_language_, "Output", "出力"));
	}
	if (upload_title_) {
		upload_title_->setText(ui_text(ui_language_, "Upload Review", "アップロード確認"));
	}
	if (queue_label_) {
		queue_label_->setText(ui_text(ui_language_, "Queue", "キュー"));
	}
	if (review_item_label_) {
		review_item_label_->setText(ui_text(ui_language_, "Review item", "確認項目"));
	}
	if (metadata_title_) {
		metadata_title_->setText(ui_text(ui_language_, "Metadata", "メタデータ"));
	}
	if (metadata_note_) {
		metadata_note_->setText(ui_text(
			ui_language_,
			"Review match fields and generated upload text before publishing.",
			"公開前に対戦情報と生成されたアップロード文面を確認します。"));
	}
	if (settings_note_) {
		settings_note_->setText(ui_text(
			ui_language_,
			"Change runtime path, theme, and language. Saving restarts the Worker.",
			"実行データ保存先、テーマ、言語を変更します。保存すると Worker を再起動します。"));
	}
	if (help_note_) {
		help_note_->setText(ui_text(
			ui_language_,
			"Open task-focused help for setup, recording, upload review, diagnostics, and language recovery.",
			"セットアップ、録画、アップロード確認、診断、言語復旧のヘルプを開きます。"));
	}
	if (automatic_note_) {
		automatic_note_->setText(ui_text(
			ui_language_,
			"Register local start/end templates and test detection before relying on automatic recording.",
			"自動録画を使う前に、ローカルの開始/終了テンプレートを登録して検出テストを実行します。"));
	}
	if (diagnostics_group_) {
		diagnostics_group_->setTitle(ui_text(ui_language_, "Diagnostics", "診断"));
	}
	if (endpoint_label_) {
		endpoint_label_->setText(ui_text(ui_language_, "Endpoint", "エンドポイント"));
	}
	if (user_data_label_) {
		user_data_label_->setText(ui_text(ui_language_, "User data", "ユーザーデータ"));
	}
	if (worker_path_label_) {
		worker_path_label_->setText(ui_text(ui_language_, "Worker path", "Worker パス"));
	}
	if (logs_label_) {
		logs_label_->setText(ui_text(ui_language_, "Logs", "ログ"));
	}
	if (ownership_label_) {
		ownership_label_->setText(ui_text(ui_language_, "Ownership", "所有状態"));
	}
	if (detail_label_) {
		detail_label_->setText(ui_text(ui_language_, "Detail", "詳細"));
	}
	if (settings_button_) {
		settings_button_->setText(ui_text(ui_language_, "Open Settings", "設定を開く"));
	}
	if (help_button_) {
		help_button_->setText(ui_text(ui_language_, "Open Help", "ヘルプを開く"));
	}
	if (automatic_setup_button_) {
		automatic_setup_button_->setText(ui_text(ui_language_, "Open Automatic Setup", "自動録画セットアップを開く"));
	}
	if (start_button_) {
		start_button_->setText(ui_text(ui_language_, "Start Recording", "録画開始"));
	}
	if (stop_button_) {
		stop_button_->setText(ui_text(ui_language_, "Stop Recording", "録画停止"));
	}
	if (retry_upload_button_) {
		retry_upload_button_->setText(ui_text(ui_language_, "Retry Upload", "再試行"));
	}
	if (discard_upload_button_) {
		discard_upload_button_->setText(ui_text(ui_language_, "Discard Upload", "破棄"));
	}
	if (mark_uploaded_button_) {
		mark_uploaded_button_->setText(ui_text(ui_language_, "Mark Uploaded", "アップロード済みにする"));
	}
	if (edit_metadata_button_) {
		edit_metadata_button_->setText(ui_text(ui_language_, "Edit Metadata", "メタデータ編集"));
	}
	if (preview_metadata_button_) {
		preview_metadata_button_->setText(ui_text(ui_language_, "Preview Upload Metadata", "アップロード文面プレビュー"));
	}
	if (reload_metadata_button_) {
		reload_metadata_button_->setText(ui_text(ui_language_, "Reload", "再読込"));
	}
	if (save_metadata_button_) {
		save_metadata_button_->setText(ui_text(ui_language_, "Save", "保存"));
	}
	if (render_upload_preview_button_) {
		render_upload_preview_button_->setText(ui_text(ui_language_, "Preview", "プレビュー"));
	}
	if (save_inline_settings_button_) {
		save_inline_settings_button_->setText(ui_text(ui_language_, "Save Settings", "設定を保存"));
	}
	if (diagnostics_details_button_) {
		diagnostics_details_button_->setText(
			ui_text(ui_language_, diagnostics_details_visible_ ? "Hide Details" : "Show Details",
				diagnostics_details_visible_ ? "詳細を隠す" : "詳細を表示"));
	}
	if (upload_preview_title_label_) {
		upload_preview_title_label_->setText(ui_text(ui_language_, "Upload text", "アップロード文面"));
	}
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
	if (reload_metadata_button_) {
		reload_metadata_button_->setEnabled(worker_running);
	}
	if (save_metadata_button_) {
		save_metadata_button_->setEnabled(worker_running && current_match_id_ > 0);
	}
	if (render_upload_preview_button_) {
		render_upload_preview_button_->setEnabled(worker_running && current_match_id_ > 0);
	}
	if (automatic_setup_button_) {
		automatic_setup_button_->setEnabled(worker_running);
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
	dialog.setWindowTitle(ui_text(ui_language_, "OBS Duel Recorder Settings", "OBS Duel Recorder 設定"));

	auto *layout = new QVBoxLayout(&dialog);
	auto *form = new QFormLayout;

	auto *host_input = new QLineEdit(qstr_wide(settings.endpoint.host));
	auto *port_input = new QSpinBox;
	port_input->setRange(1, 65535);
	port_input->setValue(settings.endpoint.port);
	auto *user_data_input = new QLineEdit(qstr_wide(settings.user_data_dir));
	auto *theme_input = new QComboBox;
	theme_input->addItem("Classic", QString::fromUtf8("classic"));
	theme_input->addItem("Forest", QString::fromUtf8("forest"));
	theme_input->addItem("Bright", QString::fromUtf8("bright"));
	const int theme_index = theme_input->findData(QString::fromUtf8(settings.dock_theme.c_str()));
	theme_input->setCurrentIndex(theme_index >= 0 ? theme_index : 0);
	auto *language_input = new QComboBox;
	language_input->addItem("English", QString::fromUtf8("en"));
	language_input->addItem(QString::fromUtf8("日本語"), QString::fromUtf8("ja"));
	const int language_index = language_input->findData(QString::fromUtf8(settings.ui_language.c_str()));
	language_input->setCurrentIndex(language_index >= 0 ? language_index : 0);
	auto *settings_path = new QLabel(qstr_wide(settings.settings_path));
	settings_path->setTextInteractionFlags(Qt::TextSelectableByMouse);
	settings_path->setWordWrap(true);

	form->addRow(ui_text(ui_language_, "Host", "ホスト"), host_input);
	form->addRow(ui_text(ui_language_, "Port", "ポート"), port_input);
	form->addRow(ui_text(ui_language_, "User data dir", "ユーザーデータ保存先"), user_data_input);
	form->addRow(ui_text(ui_language_, "Dock theme", "Dock テーマ"), theme_input);
	form->addRow(ui_text(ui_language_, "Language", "言語"), language_input);
	form->addRow(ui_text(ui_language_, "Settings file", "設定ファイル"), settings_path);
	layout->addLayout(form);

	auto *note = new QLabel(ui_text(
		ui_language_,
		"Saving restarts the Worker with the persisted settings. Use Language here to switch English/Japanese.",
		"保存すると設定を反映して Worker を再起動します。ここにある言語から日本語/英語を切り替えます。"));
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
	settings.dock_theme = utf8_string(theme_input->currentData().toString());
	settings.ui_language = utf8_string(language_input->currentData().toString());
	settings.restart_worker_on_change = true;
	ui_language_ = settings.ui_language;
	apply_ui_language();
	apply_dock_theme(settings.dock_theme);
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
		QMessageBox::warning(dock_widget_, ui_text(ui_language_, "Edit metadata", "メタデータ編集"), message);
		return;
	}

	QDialog dialog(dock_widget_);
	dialog.setWindowTitle(ui_text(ui_language_, "Edit Match Metadata", "対戦メタデータ編集"));

	auto *layout = new QVBoxLayout(&dialog);
	auto *form = new QFormLayout;
	auto *deck_input = new QLineEdit(qstr_utf8(fetched.match.deck_name));
	auto *opponent_input = new QLineEdit(qstr_utf8(fetched.match.opponent_deck));
	auto *result_input = new QLineEdit(qstr_utf8(fetched.match.result));
	auto *rank_input = new QLineEdit(qstr_utf8(fetched.match.rank));
	auto *dp_input = new QLineEdit(qstr_utf8(fetched.match.dp));
	auto *memo_input = new QTextEdit(qstr_utf8(fetched.match.memo));
	memo_input->setAcceptRichText(false);

	form->addRow(ui_text(ui_language_, "Deck", "自分のデッキ"), deck_input);
	form->addRow(ui_text(ui_language_, "Opponent deck", "相手のデッキ"), opponent_input);
	form->addRow(ui_text(ui_language_, "Result", "結果"), result_input);
	form->addRow(ui_text(ui_language_, "Rank", "ランク"), rank_input);
	form->addRow(ui_text(ui_language_, "DP", "DP"), dp_input);
	form->addRow(ui_text(ui_language_, "Memo", "メモ"), memo_input);
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
			ui_text(ui_language_, "Edit metadata", "メタデータ編集"),
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
		QMessageBox::warning(dock_widget_, ui_text(ui_language_, "Preview upload metadata", "アップロード文面プレビュー"), message);
		return;
	}

	const UploadMetadataPreviewResult preview =
		worker_manager_.fetch_upload_metadata_preview(fetched.match.id);
	if (!preview.reachable()) {
		QMessageBox::warning(
			dock_widget_,
			ui_text(ui_language_, "Preview upload metadata", "アップロード文面プレビュー"),
			qstr_utf8(preview.error.empty() ? "Upload metadata preview is unavailable." : preview.error));
		blog(LOG_WARNING, "%s upload metadata preview failed status=%d http=%lu id=%d error=%s",
		     kLogPrefix, static_cast<int>(preview.status), preview.http_status, fetched.match.id,
		     preview.error.c_str());
		return;
	}

	QDialog dialog(dock_widget_);
	dialog.setWindowTitle(ui_text(ui_language_, "Upload Metadata Preview", "アップロード文面プレビュー"));

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

	auto *tags_preview = new QLineEdit(qstr_utf8(preview.preview.tags));
	tags_preview->setReadOnly(true);
	form->addRow(ui_text(ui_language_, "Title", "タイトル"), title_preview);
	form->addRow(ui_text(ui_language_, "Description", "説明文"), description_preview);
	form->addRow(ui_text(ui_language_, "Tags", "タグ"), tags_preview);
	layout->addLayout(form);
	layout->addWidget(warning);

	auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close);
	QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
	layout->addWidget(buttons);
	dialog.exec();

	blog(LOG_INFO, "%s upload metadata preview shown id=%d", kLogPrefix, preview.preview.match_id);
}

void PluginUiController::load_latest_metadata_into_dock()
{
	const MatchFetchResult fetched = worker_manager_.fetch_latest_match();
	PluginSettings settings = load_plugin_settings();
	if (!fetched.reachable()) {
		current_match_id_ = 0;
		if (metadata_match_label_) {
			metadata_match_label_->setText(ui_text(ui_language_, "No completed match is available yet.",
							       "完了した録画メタデータはまだありません。"));
		}
		if (metadata_status_label_) {
			metadata_status_label_->setText(qstr_utf8(fetched.error));
		}
		return;
	}

	current_match_id_ = fetched.match.id;
	if (metadata_match_label_) {
		metadata_match_label_->setText(QString::fromUtf8("Match #%1").arg(fetched.match.id));
	}
	const WorkerStatusSnapshot snapshot = worker_manager_.status_snapshot();
	const std::string video = !snapshot.recording_output_path.empty() ?
					  snapshot.recording_output_path :
					  (snapshot.queue_action_item_available ? snapshot.queue_action_item.item.video_path : "");
	if (metadata_video_label_) {
		metadata_video_label_->setText(video.empty() ?
						       ui_text(ui_language_, "Preview: video path is not linked yet.",
							       "プレビュー: 動画パスはまだ紐づいていません。") :
						       ui_text(ui_language_, "Preview source: ", "確認対象: ") + qstr_utf8(video));
	}
	if (metadata_deck_input_) {
		metadata_deck_input_->setCurrentText(qstr_utf8(fetched.match.deck_name.empty() ? settings.last_deck_name :
									 fetched.match.deck_name));
	}
	if (metadata_opponent_input_) {
		metadata_opponent_input_->setCurrentText(qstr_utf8(fetched.match.opponent_deck.empty() ?
									     settings.last_opponent_deck :
									     fetched.match.opponent_deck));
	}
	if (metadata_result_input_) {
		metadata_result_input_->setText(qstr_utf8(fetched.match.result));
	}
	if (metadata_rank_input_) {
		metadata_rank_input_->setText(qstr_utf8(fetched.match.rank.empty() ? settings.last_rank : fetched.match.rank));
	}
	if (metadata_dp_input_) {
		metadata_dp_input_->setText(qstr_utf8(fetched.match.dp.empty() ? settings.last_dp : fetched.match.dp));
	}
	if (metadata_memo_input_) {
		metadata_memo_input_->setPlainText(qstr_utf8(fetched.match.memo));
	}
	if (upload_title_template_input_) {
		upload_title_template_input_->setText(qstr_utf8(fetched.match.title_template.empty() ?
								 settings.upload_title_template :
								 fetched.match.title_template));
	}
	if (upload_description_template_input_) {
		upload_description_template_input_->setPlainText(qstr_utf8(fetched.match.description_template.empty() ?
									  settings.upload_description_template :
									  fetched.match.description_template));
	}
	if (upload_tags_template_input_) {
		upload_tags_template_input_->setText(qstr_utf8(fetched.match.tags_template.empty() ?
							       settings.upload_tags_template :
							       fetched.match.tags_template));
	}
	if (metadata_status_label_) {
		metadata_status_label_->setText(ui_text(ui_language_, "Loaded latest match.", "最新の対戦を読み込みました。"));
	}
}

void PluginUiController::save_metadata_from_dock()
{
	if (current_match_id_ <= 0) {
		load_latest_metadata_into_dock();
	}
	if (current_match_id_ <= 0) {
		return;
	}

	PluginSettings settings = load_plugin_settings();
	MatchMetadataPayload payload;
	payload.id = current_match_id_;
	payload.deck_name = combo_text(metadata_deck_input_);
	payload.opponent_deck = combo_text(metadata_opponent_input_);
	payload.result = metadata_result_input_ ? utf8_string(metadata_result_input_->text().trimmed()) : std::string{};
	payload.rank = metadata_rank_input_ ? utf8_string(metadata_rank_input_->text().trimmed()) : std::string{};
	payload.dp = metadata_dp_input_ ? utf8_string(metadata_dp_input_->text().trimmed()) : std::string{};
	payload.memo = metadata_memo_input_ ? utf8_string(metadata_memo_input_->toPlainText().trimmed()) : std::string{};
	payload.title_template = upload_title_template_input_ ? utf8_string(upload_title_template_input_->text()) :
								settings.upload_title_template;
	payload.description_template = upload_description_template_input_ ?
					       utf8_string(upload_description_template_input_->toPlainText()) :
					       settings.upload_description_template;
	payload.tags_template = upload_tags_template_input_ ? utf8_string(upload_tags_template_input_->text()) :
							      settings.upload_tags_template;

	add_unique_candidate(settings.deck_candidates, payload.deck_name);
	add_unique_candidate(settings.opponent_deck_candidates, payload.opponent_deck);
	settings.last_deck_name = payload.deck_name;
	settings.last_opponent_deck = payload.opponent_deck;
	settings.last_rank = payload.rank;
	settings.last_dp = payload.dp;
	settings.upload_title_template = payload.title_template;
	settings.upload_description_template = payload.description_template;
	settings.upload_tags_template = payload.tags_template;
	save_plugin_settings(settings);

	const MetadataUpdateResult result = worker_manager_.update_match_metadata(payload);
	if (!result.accepted()) {
		if (metadata_status_label_) {
			metadata_status_label_->setText(qstr_utf8(result.error.empty() ?
								  "Metadata was rejected by the Worker." :
								  result.error));
		}
		return;
	}
	if (metadata_status_label_) {
		metadata_status_label_->setText(ui_text(ui_language_, "Saved. Previous deck/rank/DP values were retained.",
							"保存しました。デッキ/ランク/DP は次回入力に引き継がれます。"));
	}
	render_upload_preview_from_dock();
}

void PluginUiController::render_upload_preview_from_dock()
{
	if (current_match_id_ <= 0) {
		load_latest_metadata_into_dock();
	}
	if (current_match_id_ <= 0) {
		return;
	}

	const UploadMetadataPreviewResult preview = worker_manager_.fetch_upload_metadata_preview(current_match_id_);
	if (!preview.reachable()) {
		if (upload_preview_warning_label_) {
			upload_preview_warning_label_->setText(qstr_utf8(preview.error));
		}
		return;
	}
	if (upload_preview_title_label_) {
		upload_preview_title_label_->setText(qstr_utf8(preview.preview.title));
	}
	if (upload_preview_description_) {
		QString text = qstr_utf8(preview.preview.description);
		if (!preview.preview.tags.empty()) {
			text += QString::fromUtf8("\n\nTags: ") + qstr_utf8(preview.preview.tags);
		}
		upload_preview_description_->setPlainText(text);
	}
	if (upload_preview_warning_label_) {
		upload_preview_warning_label_->setText(preview.preview.warning.empty() ?
							       ui_text(ui_language_, "No preview warnings.",
								       "プレビュー警告はありません。") :
							       qstr_utf8(preview.preview.warning));
	}
}

void PluginUiController::save_inline_settings()
{
	PluginSettings settings = load_plugin_settings();
	bool ok = false;
	const int port = settings_port_input_ ? settings_port_input_->text().toInt(&ok) : settings.endpoint.port;
	settings.endpoint.host = settings_host_input_ ? settings_host_input_->text().toStdWString() : settings.endpoint.host;
	settings.endpoint.port = static_cast<uint16_t>(ok && port > 0 && port <= 65535 ? port : settings.endpoint.port);
	settings.user_data_dir = settings_user_data_input_ ? settings_user_data_input_->text().toStdWString() :
							    settings.user_data_dir;
	settings.dock_theme = settings_theme_input_ ? utf8_string(settings_theme_input_->currentData().toString()) :
						      settings.dock_theme;
	settings.ui_language = settings_language_input_ ? utf8_string(settings_language_input_->currentData().toString()) :
							 settings.ui_language;
	settings.restart_worker_on_change = true;
	ui_language_ = settings.ui_language;
	apply_ui_language();
	apply_dock_theme(settings.dock_theme);
	save_settings_and_restart(settings);
}

void PluginUiController::toggle_diagnostics_details()
{
	diagnostics_details_visible_ = !diagnostics_details_visible_;
	if (diagnostics_group_) {
		diagnostics_group_->setVisible(diagnostics_details_visible_);
	}
	apply_ui_language();
}

void PluginUiController::request_show_help()
{
	const bool ja = language_is_japanese(ui_language_);
	const QString message = ja ?
		QString::fromUtf8(
			"セットアップ: 設定を開き、実行データ保存先を確認して保存します。Worker 状態が running になれば準備完了です。\n\n"
			"手動録画: Worker が running の状態で、録画開始と録画停止を使います。出力証跡は録画タブで確認します。\n\n"
			"自動録画: 自動タブからローカルの開始/終了テンプレートを登録し、検出テストを実行してから使います。\n\n"
			"メタデータ: 録画完了後にメタデータ編集を使います。認識結果は適用または編集するまで候補として扱います。\n\n"
			"アップロード確認: 実際の YouTube アップロード前に文面をプレビューします。再試行や破棄はキュー項目を確認してから使います。\n\n"
			"診断: エンドポイント、ユーザーデータ、Worker パス、ログ、所有状態、詳細は診断タブで確認します。ログは設定済みのユーザーデータ保存先に残ります。\n\n"
			"Language can be changed from Settings.")
		:
		QString::fromUtf8(
			"Setup: open Settings, confirm the runtime data directory, then save. Worker status should become running.\n\n"
			"Manual recording: use Start Recording and Stop Recording after Worker status is running. Check the Recording tab for output evidence.\n\n"
			"Automatic recording: open the Auto tab, register local start/end templates, run detection tests, and confirm threshold and confirmation count before enabling it.\n\n"
			"Metadata: use Edit Metadata after a completed recording. Recognition results are suggestions until you apply or edit them.\n\n"
			"Upload review: preview upload metadata before real YouTube upload. Use retry or discard only after checking the queue item.\n\n"
			"Diagnostics: use the Diagnostics tab for endpoint, user data, Worker path, logs, ownership, and last detail. Logs stay under the configured user data directory.\n\n"
			"言語は Settings から変更できます。");
	QMessageBox::information(
		dock_widget_,
		ui_text(ui_language_, "OBS Duel Recorder Help", "OBS Duel Recorder ヘルプ"),
		message);
}

void PluginUiController::request_automatic_setup()
{
	QDialog dialog(dock_widget_);
	dialog.setWindowTitle(ui_text(ui_language_, "Automatic Recording Setup", "自動録画セットアップ"));

	auto *layout = new QVBoxLayout(&dialog);
	auto *form = new QFormLayout;

	auto *kind_input = new QComboBox;
	kind_input->addItem(ui_text(ui_language_, "Start template", "開始テンプレート"), QString::fromUtf8("start"));
	kind_input->addItem(ui_text(ui_language_, "End template", "終了テンプレート"), QString::fromUtf8("end"));
	auto *path_input = new QLineEdit;
	path_input->setPlaceholderText("C:/path/to/user_data/templates/duel-start.tpl");
	auto *threshold_input = new QDoubleSpinBox;
	threshold_input->setRange(0.0, 1.0);
	threshold_input->setSingleStep(0.05);
	threshold_input->setDecimals(2);
	threshold_input->setValue(1.0);
	auto *confirmations_input = new QSpinBox;
	confirmations_input->setRange(1, 20);
	confirmations_input->setValue(2);
	auto *test_frame_input = new QTextEdit;
	test_frame_input->setAcceptRichText(false);
	test_frame_input->setPlaceholderText(ui_text(ui_language_,
						    "Paste a local fixture string or test frame text. Do not paste secrets.",
						    "ローカルのテスト文字列またはフレームテキストを貼り付けます。秘密情報は貼り付けないでください。"));
	test_frame_input->setFixedHeight(84);

	form->addRow(ui_text(ui_language_, "Template kind", "テンプレート種別"), kind_input);
	form->addRow(ui_text(ui_language_, "Template path", "テンプレートパス"), path_input);
	form->addRow(ui_text(ui_language_, "Threshold", "しきい値"), threshold_input);
	form->addRow(ui_text(ui_language_, "Confirmations", "確認回数"), confirmations_input);
	form->addRow(ui_text(ui_language_, "Test frame text", "テストフレーム文字列"), test_frame_input);
	layout->addLayout(form);

	auto *status = new QTextEdit;
	status->setReadOnly(true);
	status->setAcceptRichText(false);
	status->setPlaceholderText(ui_text(ui_language_,
					  "Validation, registration, and detection test results appear here.",
					  "検証、登録、検出テストの結果がここに表示されます。"));
	status->setMinimumHeight(150);
	layout->addWidget(status);

	auto *buttons = new QHBoxLayout;
	auto *validate_button = new QPushButton(ui_text(ui_language_, "Validate Setup", "セットアップ検証"));
	auto *register_button = new QPushButton(ui_text(ui_language_, "Register Template", "テンプレート登録"));
	auto *test_button = new QPushButton(ui_text(ui_language_, "Test Detection", "検出テスト"));
	auto *close_button = new QPushButton(ui_text(ui_language_, "Close", "閉じる"));
	buttons->addWidget(validate_button);
	buttons->addWidget(register_button);
	buttons->addWidget(test_button);
	buttons->addWidget(close_button);
	layout->addLayout(buttons);

	auto result_text = [](const char *title, const WorkerActionResult &result) {
		std::string text = std::string(title) + "\n";
		text += "http=" + std::to_string(result.http_status) + "\n";
		if (!result.error.empty()) {
			text += "error=" + result.error + "\n";
		}
		if (!result.body.empty()) {
			text += result.body;
		}
		return qstr_utf8(text);
	};

	QObject::connect(validate_button, &QPushButton::clicked, [this, status, result_text]() {
		const WorkerActionResult result = worker_manager_.fetch_setup_validation();
		status->setPlainText(result_text("Setup validation", result));
	});
	QObject::connect(register_button, &QPushButton::clicked,
			 [this, kind_input, path_input, threshold_input, confirmations_input, status, result_text]() {
				 const std::string kind = utf8_string(kind_input->currentData().toString());
				 const std::string path = utf8_string(path_input->text().trimmed());
				 const WorkerActionResult result = worker_manager_.register_detection_template(
					 kind, path, threshold_input->value(), confirmations_input->value());
				 status->setPlainText(result_text("Template registration", result));
			 });
	QObject::connect(test_button, &QPushButton::clicked,
			 [this, kind_input, test_frame_input, status, result_text]() {
				 const std::string kind = utf8_string(kind_input->currentData().toString());
				 const std::string frame_text = utf8_string(test_frame_input->toPlainText());
				 const WorkerActionResult result = worker_manager_.test_detection_template(kind, frame_text);
				 status->setPlainText(result_text("Detection test", result));
			 });
	QObject::connect(close_button, &QPushButton::clicked, &dialog, &QDialog::accept);

	const WorkerActionResult initial = worker_manager_.fetch_setup_validation();
	status->setPlainText(result_text(
		"Setup validation\nIf templates are action_required, register both start and end templates, then run Test Detection.",
		initial));
	dialog.exec();
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
