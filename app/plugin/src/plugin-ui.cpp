#include "plugin-ui.hpp"

#include "overlay-sources.hpp"
#include "plugin-settings.hpp"

#include <obs-frontend-api.h>
#include <obs-module.h>

#include <QByteArray>
#include <QCheckBox>
#include <QCoreApplication>
#include <QDateTime>
#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QComboBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QFrame>
#include <QGroupBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QMetaObject>
#include <QPixmap>
#include <QPointer>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QTabWidget>
#include <QTextEdit>
#include <QThread>
#include <QSpinBox>
#include <QStyle>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>
#include <QWidget>

#include <sstream>
#include <string>
#include <thread>
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

std::string take_frontend_string(char *value)
{
	if (!value) {
		return {};
	}
	std::string result(value);
	bfree(value);
	return result;
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

std::string youtube_summary(const WorkerStatusSnapshot &snapshot)
{
	if (!snapshot.upload_status_available) {
		if (!snapshot.upload_status.error.empty()) {
			return "upload API not available: " + snapshot.upload_status.error;
		}
		return "upload API not available";
	}

	std::ostringstream out;
	if (!snapshot.upload_status.readiness_state.empty()) {
		out << snapshot.upload_status.readiness_state;
		if (!snapshot.upload_status.readiness_next_action.empty()) {
			out << " / " << snapshot.upload_status.readiness_next_action;
		}
		return out.str();
	}
	out << "upload API ready / quota waiting " << snapshot.upload_status.quota_waiting;
	return out.str();
}

std::string localized_upload_state(const std::string &language, const std::string &state)
{
	if (!language_is_japanese(language)) {
		return state.empty() ? "unknown" : state;
	}
	if (state == "ready") {
		return "準備完了";
	}
	if (state == "client_secret_missing") {
		return "client_secret.json未設定";
	}
	if (state == "token_missing") {
		return "YouTube認証が必要";
	}
	if (state == "token_invalid") {
		return "YouTube再認証が必要";
	}
	if (state == "token_expired_refreshable") {
		return "トークン更新が必要";
	}
	if (state == "google_dependencies_missing") {
		return "Google連携ライブラリ不足";
	}
	if (state == "quota_waiting") {
		return "クォータ待ち";
	}
	if (state == "manual_review_required") {
		return "手動確認が必要";
	}
	if (state == "ready_upload") {
		return "アップロード待ち";
	}
	if (state == "upload_failed") {
		return "アップロード失敗";
	}
	if (state == "need_manual_review") {
		return "手動確認待ち";
	}
	if (state == "uploaded") {
		return "アップロード済み";
	}
	if (state == "discarded") {
		return "破棄済み";
	}
	if (state == "uploading") {
		return "アップロード中";
	}
	return state.empty() ? "不明" : state;
}

std::string localized_blocking_reason(const std::string &language, const std::string &reason)
{
	if (!language_is_japanese(language)) {
		return reason;
	}
	if (reason == "local_video_missing") {
		return "ローカル動画ファイルが見つかりません";
	}
	if (reason == "video_path_missing") {
		return "動画パスが未設定です";
	}
	if (reason == "upload_title_missing") {
		return "アップロードタイトルが空です";
	}
	if (reason == "upload_description_missing") {
		return "説明文が空です";
	}
	if (reason == "upload_tags_missing") {
		return "タグが空です";
	}
	if (reason == "youtube_video_id_already_present") {
		return "YouTube動画IDが既に保存されています";
	}
	if (reason == "metadata_unavailable") {
		return "メタデータが紐づいていません";
	}
	if (reason == "metadata_deck_name_missing") {
		return "自分のデッキが未入力です";
	}
	if (reason == "metadata_opponent_deck_missing") {
		return "相手のデッキが未入力です";
	}
	if (reason == "metadata_result_missing") {
		return "結果が未入力です";
	}
	if (reason == "metadata_rank_missing") {
		return "ランクが未入力です";
	}
	if (reason == "metadata_dp_missing") {
		return "DPが未入力です";
	}
	if (reason.find("queue_state_") == 0) {
		return "現在のキュー状態ではアップロードできません";
	}
	return reason;
}

std::string localized_blocking_reasons(const std::string &language, const std::string &reasons)
{
	if (reasons.empty()) {
		return language_is_japanese(language) ? "なし" : "none";
	}
	std::stringstream input(reasons);
	std::ostringstream out;
	std::string reason;
	while (std::getline(input, reason, ',')) {
		while (!reason.empty() && reason.front() == ' ') {
			reason.erase(reason.begin());
		}
		if (out.tellp() > 0) {
			out << ", ";
		}
		out << localized_blocking_reason(language, reason);
	}
	return out.str();
}

std::string display_video_name(const std::string &video_path, const std::string &resolved_video_path)
{
	const std::string path = video_path.empty() ? resolved_video_path : video_path;
	if (path.empty()) {
		return {};
	}
	const size_t slash = path.find_last_of("\\/");
	return slash == std::string::npos ? path : path.substr(slash + 1);
}

std::string upload_target_summary(const WorkerStatusSnapshot &snapshot, const std::string &language)
{
	if (!snapshot.upload_next_target_available) {
		return language_is_japanese(language) ? "Workerから取得できません" : "not available from Worker";
	}
	if (!snapshot.upload_next_target.found) {
		return language_is_japanese(language) ? "対象なし" : "no upload target";
	}
	const UploadTargetPayload &target = snapshot.upload_next_target.target;
	std::ostringstream out;
	out << "#" << target.queue_item_id << " / "
	    << localized_upload_state(language, target.state);
	if (target.match_id > 0) {
		out << " / match #" << target.match_id;
	}
	const std::string video_name = display_video_name(target.video_path, target.resolved_video_path);
	if (!video_name.empty()) {
		out << "\nVideo: " << video_name;
	}
	if (!target.upload_metadata.privacy_status.empty()) {
		out << "\nprivacy: " << target.upload_metadata.privacy_status;
	}
	return out.str();
}

std::string upload_text_summary(const WorkerStatusSnapshot &snapshot, const std::string &language)
{
	if (!snapshot.upload_next_target_available || !snapshot.upload_next_target.found) {
		return language_is_japanese(language) ? "プレビュー対象なし" : "no preview target";
	}
	const UploadMetadataPreviewPayload &preview = snapshot.upload_next_target.target.upload_metadata;
	std::ostringstream out;
	out << "Title: " << (preview.title.empty() ? "(empty)" : preview.title) << "\n"
	    << "Tags: " << (preview.tags.empty() ? "(empty)" : preview.tags);
	if (!preview.description.empty()) {
		out << "\n" << preview.description;
	}
	return out.str();
}

std::string upload_blocking_summary(const WorkerStatusSnapshot &snapshot, const std::string &language)
{
	if (!snapshot.upload_next_target_available || !snapshot.upload_next_target.found) {
		return language_is_japanese(language) ? "対象なし" : "no target";
	}
	const UploadTargetPayload &target = snapshot.upload_next_target.target;
	if (target.can_upload) {
		return language_is_japanese(language) ? "アップロード可能" : "uploadable";
	}
	return localized_blocking_reasons(language, target.blocking_reasons);
}

std::string upload_queue_item_label(const UploadTargetPayload &target, const std::string &language)
{
	std::ostringstream out;
	const char *icon = "○";
	if (target.can_upload) {
		icon = "✓";
	} else if (target.state == "upload_failed" || target.state == "need_manual_review") {
		icon = "!";
	} else if (target.state == "uploaded") {
		icon = "✓";
	}
	out << icon << " #" << target.queue_item_id << " "
	    << localized_upload_state(language, target.state);
	if (target.match_id > 0) {
		out << " / match #" << target.match_id;
	}
	if (!target.item.created_at.empty()) {
		out << " / " << target.item.created_at;
	}
	const std::string video_name = display_video_name(target.video_path, target.resolved_video_path);
	if (!video_name.empty()) {
		out << " / " << video_name;
	}
	return out.str();
}

std::string localized_upload_next_action(const std::string &language, const std::string &state,
					 const std::string &fallback)
{
	if (!language_is_japanese(language)) {
		return fallback.empty() ? "No action required." : fallback;
	}
	if (state == "client_secret_missing") {
		return "Google OAuthクライアントJSONを認証ファイルとして選択してください。保存先フォルダを開いて手動配置することもできます。";
	}
	if (state == "token_missing") {
		return "YouTube認証を実行してください。";
	}
	if (state == "token_invalid") {
		return "YouTubeを再認証し、トークンを作り直してください。";
	}
	if (state == "token_expired_refreshable") {
		return "トークン更新を実行してください。";
	}
	if (state == "google_dependencies_missing" || state == "dependencies_missing") {
		return "Googleアップロード依存ライブラリを含む配布版を使用してください。";
	}
	if (state == "quota_waiting") {
		return "YouTubeクォータのリセット後に再試行してください。";
	}
	if (state == "manual_review_required") {
		return "YouTube側を確認し、再試行・破棄・アップロード済み設定を選んでください。";
	}
	if (state == "ready") {
		return "アップロード可能です。";
	}
	return fallback.empty() ? "状態を確認してください。" : fallback;
}

std::string upload_readiness_summary(const WorkerStatusSnapshot &snapshot, const std::string &language)
{
	if (!snapshot.upload_status_available) {
		if (!snapshot.upload_status.error.empty()) {
			return (language_is_japanese(language) ? "Upload API取得不可: " : "upload API not available: ") +
			       snapshot.upload_status.error;
		}
		return language_is_japanese(language) ? "Upload API取得不可" : "upload API not available";
	}

	const UploadStatusResult &status = snapshot.upload_status;
	std::ostringstream out;
	out << (language_is_japanese(language) ? "状態: " : "State: ")
	    << localized_upload_state(language, status.readiness_state) << "\n"
	    << (language_is_japanese(language) ? "次の操作: " : "Next action: ")
	    << localized_upload_next_action(language, status.readiness_state, status.readiness_next_action);
	if (!status.privacy_status.empty()) {
		out << "\nPrivacy: " << status.privacy_status;
	}
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
		"classic", "#f6fbf7", "#1b5e20", "#1b5e20", "#edf7ed", "#c8e6c9", "#f1f8e9",
		"#a5d6a7", "#e8f5e9", "#81c784", "#f7fbf4", "#c5e1a5", "#f3f8f2", "#c8d6c5",
		"#2e7d32", "#ffffff", "#43a047", "#ffffff", "#c62828", "#ffffff", "#388e3c",
		"#ffffff", "#607d66", "#ffffff", "#2e7d32", "#ffffff"};
	static const DockThemePalette forest{
		"forest", "#f4faf3", "#2f5d37", "#2f5d37", "#eef7ec", "#b7d7b6", "#f3faef",
		"#9fcb9e", "#e6f3e7", "#79b882", "#f7fbf3", "#bdd7a7", "#f1f6f0", "#c5d4c2",
		"#3f7d4a", "#ffffff", "#4f9b57", "#ffffff", "#b23b3b", "#ffffff", "#4c8f53",
		"#ffffff", "#667a66", "#ffffff", "#3f7d4a", "#ffffff"};
	static const DockThemePalette bright{
		"bright", "#f8fff8", "#246b2a", "#246b2a", "#effbed", "#cdecc7", "#f5fce8",
		"#b9e4a6", "#e8f8ea", "#97d39d", "#f6fcf0", "#d3edb8", "#f4faf3", "#d5e5d2",
		"#2f8f3a", "#ffffff", "#55ad5b", "#ffffff", "#d04747", "#ffffff", "#3d9b46",
		"#ffffff", "#6b806b", "#ffffff", "#2f8f3a", "#ffffff"};

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

void prepare_dock_action_button(QPushButton *button)
{
	if (!button) {
		return;
	}
	button->setMinimumWidth(0);
	button->setMinimumHeight(28);
	button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

QGridLayout *make_dock_action_grid()
{
	auto *layout = new QGridLayout;
	layout->setContentsMargins(0, 0, 0, 0);
	layout->setHorizontalSpacing(6);
	layout->setVerticalSpacing(6);
	layout->setColumnStretch(0, 1);
	layout->setColumnStretch(1, 1);
	return layout;
}

void add_dock_action_button(QGridLayout *layout, QPushButton *button, int row, int column)
{
	prepare_dock_action_button(button);
	layout->addWidget(button, row, column);
}

void decorate_frame_button(QPushButton *button, const QString &text, const QString &tooltip)
{
	if (!button) {
		return;
	}
	button->setText(text);
	button->setIcon(QIcon());
	button->setToolTip(tooltip);
	prepare_dock_action_button(button);
}

void decorate_button(QPushButton *button, QStyle::StandardPixmap icon, const QString &tooltip)
{
	if (!button) {
		return;
	}
	button->setIcon(button->style()->standardIcon(icon));
	button->setToolTip(tooltip);
	prepare_dock_action_button(button);
}

QString state_badge_style(WorkerDiagnosticState state)
{
	const char *background = "#d8dee7";
	const char *foreground = "#1f2933";
	switch (state) {
	case WorkerDiagnosticState::running:
		background = "#2e7d32";
		foreground = "#ffffff";
		break;
	case WorkerDiagnosticState::starting:
		background = "#c5e1a5";
		foreground = "#1b5e20";
		break;
	case WorkerDiagnosticState::not_started:
		background = "#e8f5e9";
		foreground = "#1b5e20";
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

	header_card_ = make_card("#1b5e20", "#1b5e20");
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

	auto *automatic_tab = new QWidget;
	auto *automatic_tab_layout = new QVBoxLayout(automatic_tab);
	automatic_tab_layout->setContentsMargins(0, 0, 0, 0);
	automatic_tab_layout->setSpacing(10);

	auto *template_tab = new QWidget;
	auto *template_tab_layout = new QVBoxLayout(template_tab);
	template_tab_layout->setContentsMargins(0, 0, 0, 0);
	template_tab_layout->setSpacing(10);

	setup_card_ = make_card("#e8f7f4", "#84d9cf");
	auto *setup_layout = qobject_cast<QVBoxLayout *>(setup_card_->layout());
	setup_title_ = add_card_title(setup_layout, language_is_japanese(ui_language_) ? "セットアップ" : "Setup", "#1b5e20");
	setup_value_ = add_card_value(setup_layout, language_is_japanese(ui_language_) ? "準備状態" : "Readiness",
				      &setup_label_);
	action_value_ = add_card_value(setup_layout, language_is_japanese(ui_language_) ? "次の操作" : "Next action",
				       &action_label_);
	setup_tab_layout->addWidget(setup_card_);
	automatic_setup_button_ = new QPushButton(ui_text(ui_language_, "Open Automatic Setup", "自動録画セットアップを開く"));
	QObject::connect(automatic_setup_button_, &QPushButton::clicked, [this]() { request_automatic_setup(); });
	style_button(automatic_setup_button_, "#388e3c", "#ffffff");
	decorate_button(automatic_setup_button_, QStyle::SP_ComputerIcon,
			ui_text(ui_language_, "Open guided automatic recording setup.", "自動録画の設定ガイドを開きます。"));

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
	settings_theme_input_->addItem("Light Green", QString::fromUtf8("classic"));
	settings_theme_input_->addItem("Forest Green", QString::fromUtf8("forest"));
	settings_theme_input_->addItem("Mint Green", QString::fromUtf8("bright"));
	settings_theme_input_->setCurrentIndex(std::max(0, settings_theme_input_->findData(QString::fromUtf8(settings.dock_theme.c_str()))));
	settings_language_input_ = new QComboBox;
	settings_language_input_->addItem("English", QString::fromUtf8("en"));
	settings_language_input_->addItem(QString::fromUtf8("日本語"), QString::fromUtf8("ja"));
	settings_language_input_->setCurrentIndex(std::max(0, settings_language_input_->findData(QString::fromUtf8(settings.ui_language.c_str()))));
	automatic_detection_enabled_input_ = new QCheckBox(ui_text(ui_language_, "Enable automatic detection frame feed",
								   "自動検出フレーム送信を有効にする"));
	automatic_detection_enabled_input_->setChecked(settings.automatic_detection_enabled);
	automatic_detection_interval_input_ = new QSpinBox;
	automatic_detection_interval_input_->setRange(1000, 60000);
	automatic_detection_interval_input_->setSingleStep(1000);
	automatic_detection_interval_input_->setSuffix(QString::fromUtf8(" ms"));
	automatic_detection_interval_input_->setValue(settings.automatic_detection_interval_ms);
	inline_settings_form->addRow(ui_text(ui_language_, "Host", "ホスト"), settings_host_input_);
	inline_settings_form->addRow(ui_text(ui_language_, "Port", "ポート"), settings_port_input_);
	inline_settings_form->addRow(ui_text(ui_language_, "User data dir", "ユーザーデータ保存先"), settings_user_data_input_);
	inline_settings_form->addRow(ui_text(ui_language_, "Dock theme", "Dock テーマ"), settings_theme_input_);
	inline_settings_form->addRow(ui_text(ui_language_, "Language", "言語"), settings_language_input_);
	inline_settings_form->addRow(ui_text(ui_language_, "Automatic detection", "自動検出"), automatic_detection_enabled_input_);
	inline_settings_form->addRow(ui_text(ui_language_, "Frame interval", "送信間隔"), automatic_detection_interval_input_);
	setup_tab_layout->addLayout(inline_settings_form);
	save_inline_settings_button_ = new QPushButton(ui_text(ui_language_, "Save Settings", "設定を保存"));
	QObject::connect(save_inline_settings_button_, &QPushButton::clicked, [this]() { save_inline_settings(); });
	style_button(save_inline_settings_button_, "#2e7d32", "#ffffff");
	decorate_button(save_inline_settings_button_, QStyle::SP_DialogSaveButton,
			ui_text(ui_language_, "Save Dock settings and restart Worker if needed.", "Dock設定を保存し、必要に応じてWorkerを再起動します。"));
	setup_tab_layout->addWidget(save_inline_settings_button_);

	automatic_note_ = new QLabel(ui_text(
		ui_language_,
		"Register local start/end templates and test detection before relying on automatic recording.",
		"自動録画を使う前に、ローカルの開始/終了テンプレートを登録して検出テストを実行します。"));
	automatic_note_->setWordWrap(true);
	setup_tab_layout->addWidget(automatic_note_);
	setup_tab_layout->addWidget(automatic_setup_button_);

	recording_card_ = make_card("#f1f8e9", "#a5d6a7");
	auto *recording_layout = qobject_cast<QVBoxLayout *>(recording_card_->layout());
	recording_title_ = add_card_title(recording_layout, language_is_japanese(ui_language_) ? "録画" : "Recording",
					  "#1b5e20");
	recording_value_ = add_card_value(recording_layout, language_is_japanese(ui_language_) ? "状態" : "State",
					  &recording_label_);
	output_value_ = add_card_value(recording_layout, language_is_japanese(ui_language_) ? "出力" : "Output",
				       &output_label_);

	auto *recording_controls = new QHBoxLayout;
	start_button_ = new QPushButton(ui_text(ui_language_, "Start Recording", "録画開始"));
	stop_button_ = new QPushButton(ui_text(ui_language_, "Stop Recording", "録画停止"));
	QObject::connect(start_button_, &QPushButton::clicked, [this]() { request_manual_start(); });
	QObject::connect(stop_button_, &QPushButton::clicked, [this]() { request_manual_stop(); });
	style_button(start_button_, "#43a047", "#ffffff");
	style_button(stop_button_, "#f05d5e", "#ffffff");
	decorate_button(start_button_, QStyle::SP_MediaPlay,
			ui_text(ui_language_, "Start manual OBS recording.", "手動でOBS録画を開始します。"));
	decorate_button(stop_button_, QStyle::SP_MediaStop,
			ui_text(ui_language_, "Stop manual OBS recording.", "手動でOBS録画を停止します。"));
	recording_controls->addWidget(start_button_);
	recording_controls->addWidget(stop_button_);
	recording_layout->addLayout(recording_controls);
	recording_tab_layout->addWidget(recording_card_);

	upload_card_ = make_card("#e8f5e9", "#81c784");
	auto *upload_layout = qobject_cast<QVBoxLayout *>(upload_card_->layout());
	upload_title_ = add_card_title(upload_layout, language_is_japanese(ui_language_) ? "アップロードキュー情報" : "Upload Queue",
				       "#1b5e20");
	queue_value_ = add_card_value(upload_layout, language_is_japanese(ui_language_) ? "キュー" : "Queue", &queue_label_);
	review_item_value_ = add_card_value(upload_layout, language_is_japanese(ui_language_) ? "確認項目" : "Review item",
					    &review_item_label_);
	youtube_value_ = add_card_value(upload_layout, language_is_japanese(ui_language_) ? "YouTube連携" : "YouTube link",
					&youtube_label_);
	upload_queue_input_ = new QComboBox;
	QObject::connect(upload_queue_input_, QOverload<int>::of(&QComboBox::currentIndexChanged), [this](int index) {
		if (!upload_queue_input_ || index < 0) {
			return;
		}
		selected_upload_queue_item_id_ = upload_queue_input_->itemData(index).toInt();
		load_upload_video_preview(upload_preview_frame_index_);
		refresh();
	});
	upload_layout->addWidget(upload_queue_input_);

	upload_video_preview_image_ = new QLabel(ui_text(ui_language_, "No upload target preview.", "アップロード対象プレビューはありません。"));
	upload_video_preview_image_->setAlignment(Qt::AlignCenter);
	upload_video_preview_image_->setMinimumHeight(120);
	upload_video_preview_image_->setMaximumHeight(180);
	upload_video_preview_image_->setStyleSheet(
		"background: #f3f8f2; border: 1px solid #c8d6c5; color: #35543a; padding: 8px;");
	upload_layout->addWidget(upload_video_preview_image_);
	auto *upload_preview_controls = new QHBoxLayout;
	upload_preview_frame_1_button_ = new QPushButton("1");
	upload_preview_frame_2_button_ = new QPushButton("2");
	upload_preview_frame_3_button_ = new QPushButton("3");
	QObject::connect(upload_preview_frame_1_button_, &QPushButton::clicked, [this]() { load_upload_video_preview(1); });
	QObject::connect(upload_preview_frame_2_button_, &QPushButton::clicked, [this]() { load_upload_video_preview(2); });
	QObject::connect(upload_preview_frame_3_button_, &QPushButton::clicked, [this]() { load_upload_video_preview(3); });
	decorate_frame_button(upload_preview_frame_1_button_, ui_text(ui_language_, "1", "1枚目"),
			      ui_text(ui_language_, "Show the first representative frame.", "1枚目の代表フレームを表示します。"));
	decorate_frame_button(upload_preview_frame_2_button_, ui_text(ui_language_, "2", "2枚目"),
			      ui_text(ui_language_, "Show the middle representative frame.", "中央の代表フレームを表示します。"));
	decorate_frame_button(upload_preview_frame_3_button_, ui_text(ui_language_, "3", "3枚目"),
			      ui_text(ui_language_, "Show the last representative frame.", "3枚目の代表フレームを表示します。"));
	style_button(upload_preview_frame_1_button_, "#6b7280", "#ffffff");
	style_button(upload_preview_frame_2_button_, "#1b5e20", "#ffffff");
	style_button(upload_preview_frame_3_button_, "#6b7280", "#ffffff");
	upload_preview_controls->addWidget(upload_preview_frame_1_button_);
	upload_preview_controls->addWidget(upload_preview_frame_2_button_);
	upload_preview_controls->addWidget(upload_preview_frame_3_button_);
	upload_layout->addLayout(upload_preview_controls);

	upload_target_value_ = add_card_value(upload_layout,
					      language_is_japanese(ui_language_) ? "アップロード対象" : "Upload target",
					      &upload_target_label_);
	upload_text_value_ = add_card_value(upload_layout,
					    language_is_japanese(ui_language_) ? "アップロード文面" : "Upload text",
					    &upload_text_label_);
	upload_blocking_value_ = add_card_value(upload_layout,
						language_is_japanese(ui_language_) ? "実行可否" : "Readiness",
						&upload_blocking_label_);
	upload_result_value_ = add_card_value(upload_layout,
					      language_is_japanese(ui_language_) ? "直近の結果" : "Latest result",
					      &upload_result_label_);

	auto *privacy_controls = new QHBoxLayout;
	upload_privacy_input_ = new QComboBox;
	upload_privacy_input_->addItem(ui_text(ui_language_, "Private", "非公開"), QString::fromUtf8("private"));
	upload_privacy_input_->addItem(ui_text(ui_language_, "Unlisted", "限定公開"), QString::fromUtf8("unlisted"));
	save_upload_privacy_button_ = new QPushButton(ui_text(ui_language_, "Save Privacy", "公開範囲を保存"));
	QObject::connect(save_upload_privacy_button_, &QPushButton::clicked, [this]() { request_save_upload_privacy(); });
	style_button(save_upload_privacy_button_, "#388e3c", "#ffffff");
	decorate_button(save_upload_privacy_button_, QStyle::SP_DialogSaveButton,
			ui_text(ui_language_, "Save the privacy status used by YouTube uploads.",
				"YouTubeアップロードで使う公開範囲を保存します。"));
	privacy_controls->addWidget(upload_privacy_input_);
	privacy_controls->addWidget(save_upload_privacy_button_);
	upload_layout->addLayout(privacy_controls);

	auto *upload_controls = make_dock_action_grid();
	upload_to_youtube_button_ = new QPushButton(ui_text(ui_language_, "Upload", "アップロード"));
	open_youtube_button_ = new QPushButton(ui_text(ui_language_, "YouTube", "YouTube"));
	edit_upload_metadata_button_ = new QPushButton(ui_text(ui_language_, "Metadata", "メタデータ"));
	retry_upload_button_ = new QPushButton(ui_text(ui_language_, "Retry", "再試行"));
	discard_upload_button_ = new QPushButton(ui_text(ui_language_, "Discard", "破棄"));
	mark_uploaded_button_ = new QPushButton(ui_text(ui_language_, "Mark Done", "済みにする"));
	QObject::connect(upload_to_youtube_button_, &QPushButton::clicked, [this]() { request_upload_to_youtube(); });
	QObject::connect(edit_upload_metadata_button_, &QPushButton::clicked, [this]() {
		const WorkerStatusSnapshot snapshot = worker_manager_.status_snapshot();
		if (!has_selected_upload_target(snapshot)) {
			return;
		}
		const UploadTargetPayload target = selected_upload_target(snapshot);
		if (target.match_id <= 0) {
			return;
		}
		if (dock_tabs_) {
			dock_tabs_->setCurrentIndex(0);
		}
		load_match_metadata_into_dock(target.match_id);
	});
	QObject::connect(retry_upload_button_, &QPushButton::clicked, [this]() { request_upload_retry(); });
	QObject::connect(discard_upload_button_, &QPushButton::clicked, [this]() { request_upload_discard(); });
	QObject::connect(mark_uploaded_button_, &QPushButton::clicked, [this]() { request_upload_mark_uploaded(); });
	QObject::connect(open_youtube_button_, &QPushButton::clicked, [this]() { request_open_uploaded_youtube(); });
	style_button(upload_to_youtube_button_, "#1b5e20", "#ffffff");
	style_button(edit_upload_metadata_button_, "#2e7d32", "#ffffff");
	style_button(retry_upload_button_, "#388e3c", "#ffffff");
	style_button(discard_upload_button_, "#6b7280", "#ffffff");
	style_button(mark_uploaded_button_, "#2e7d32", "#ffffff");
	style_button(open_youtube_button_, "#2e7d32", "#ffffff");
	decorate_button(upload_to_youtube_button_, QStyle::SP_ArrowUp,
			ui_text(ui_language_, "Upload the selected queue item through the Google provider.",
				"選択中のキュー項目をGoogle連携でアップロードします。"));
	decorate_button(edit_upload_metadata_button_, QStyle::SP_FileDialogDetailedView,
			ui_text(ui_language_, "Open the selected queue item's metadata in the Record tab.",
				"選択中のキュー項目のメタデータを録画タブで編集します。"));
	decorate_button(retry_upload_button_, QStyle::SP_BrowserReload,
			ui_text(ui_language_, "Retry the current failed or manual-review upload item.", "現在の失敗または確認待ちアップロードを再試行します。"));
	decorate_button(discard_upload_button_, QStyle::SP_DialogDiscardButton,
			ui_text(ui_language_, "Discard the current upload queue item.", "現在のアップロードキュー項目を破棄します。"));
	decorate_button(mark_uploaded_button_, QStyle::SP_DialogApplyButton,
			ui_text(ui_language_, "Mark the current item uploaded after entering a YouTube video ID.", "YouTube動画IDを入力してアップロード済みにします。"));
	decorate_button(open_youtube_button_, QStyle::SP_DialogOpenButton,
			ui_text(ui_language_, "Open the last uploaded YouTube URL.",
				"直近のアップロード済みURLを開きます。"));
	add_dock_action_button(upload_controls, upload_to_youtube_button_, 0, 0);
	add_dock_action_button(upload_controls, edit_upload_metadata_button_, 0, 1);
	add_dock_action_button(upload_controls, retry_upload_button_, 1, 0);
	add_dock_action_button(upload_controls, discard_upload_button_, 1, 1);
	add_dock_action_button(upload_controls, mark_uploaded_button_, 2, 0);
	add_dock_action_button(upload_controls, open_youtube_button_, 2, 1);
	upload_layout->addLayout(upload_controls);

	auto *oauth_controls = make_dock_action_grid();
	oauth_authorize_button_ = new QPushButton(ui_text(ui_language_, "Authorize", "YouTube認証"));
	select_oauth_client_secret_button_ = new QPushButton(ui_text(ui_language_, "Auth File", "認証ファイル"));
	open_oauth_secrets_folder_button_ = new QPushButton(ui_text(ui_language_, "Auth Folder", "保存先"));
	oauth_refresh_button_ = new QPushButton(ui_text(ui_language_, "Refresh", "トークン更新"));
	oauth_help_button_ = new QPushButton(ui_text(ui_language_, "OAuth Help", "OAuthヘルプ"));
	QObject::connect(oauth_authorize_button_, &QPushButton::clicked,
			 [this]() { request_upload_oauth_authorization(); });
	QObject::connect(select_oauth_client_secret_button_, &QPushButton::clicked,
			 [this]() { request_select_upload_client_secret(); });
	QObject::connect(open_oauth_secrets_folder_button_, &QPushButton::clicked,
			 [this]() { request_open_upload_secrets_folder(); });
	QObject::connect(oauth_refresh_button_, &QPushButton::clicked, [this]() { request_upload_oauth_refresh(); });
	QObject::connect(oauth_help_button_, &QPushButton::clicked, [this]() { request_upload_oauth_help(); });
	style_button(oauth_authorize_button_, "#1b5e20", "#ffffff");
	style_button(select_oauth_client_secret_button_, "#2e7d32", "#ffffff");
	style_button(open_oauth_secrets_folder_button_, "#6b7280", "#ffffff");
	style_button(oauth_refresh_button_, "#388e3c", "#ffffff");
	style_button(oauth_help_button_, "#6b7280", "#ffffff");
	decorate_button(oauth_authorize_button_, QStyle::SP_ComputerIcon,
			ui_text(ui_language_, "Open the YouTube OAuth authorization URL in your browser.",
				"YouTube OAuth認証URLをブラウザで開きます。"));
	decorate_button(select_oauth_client_secret_button_, QStyle::SP_DialogOpenButton,
			ui_text(ui_language_,
				"Copy a Google OAuth client JSON into the local OBS Duel Recorder secrets folder.",
				"Google OAuthクライアントJSONをローカルの認証ファイル保存先へコピーします。"));
	decorate_button(open_oauth_secrets_folder_button_, QStyle::SP_DirOpenIcon,
			ui_text(ui_language_, "Open the local folder that stores YouTube OAuth files.",
				"YouTube OAuthファイルのローカル保存先フォルダを開きます。"));
	decorate_button(oauth_refresh_button_, QStyle::SP_BrowserReload,
			ui_text(ui_language_, "Refresh the stored YouTube OAuth token.",
				"保存済みのYouTube OAuthトークンを更新します。"));
	decorate_button(oauth_help_button_, QStyle::SP_DialogHelpButton,
			ui_text(ui_language_, "Open the YouTube OAuth setup guide.",
				"YouTube OAuth設定ガイドを開きます。"));
	add_dock_action_button(oauth_controls, oauth_authorize_button_, 0, 0);
	add_dock_action_button(oauth_controls, select_oauth_client_secret_button_, 0, 1);
	add_dock_action_button(oauth_controls, open_oauth_secrets_folder_button_, 1, 0);
	add_dock_action_button(oauth_controls, oauth_refresh_button_, 1, 1);
	add_dock_action_button(oauth_controls, oauth_help_button_, 2, 0);
	upload_layout->addLayout(oauth_controls);
	automatic_tab_layout->addWidget(upload_card_);

	metadata_card_ = make_card("#f7fbf4", "#c5e1a5");
	auto *metadata_layout = qobject_cast<QVBoxLayout *>(metadata_card_->layout());
	metadata_title_ = add_card_title(metadata_layout, language_is_japanese(ui_language_) ? "メタデータ" : "Metadata",
					 "#1b5e20");
	metadata_note_ = new QLabel(ui_text(
		ui_language_,
		"Enter or confirm match metadata for the latest recording.",
		"最新録画のメタデータを入力または確認します。"));
	metadata_note_->setWordWrap(true);
	metadata_note_->setStyleSheet("color: #35543a; font-size: 12px;");
	metadata_layout->addWidget(metadata_note_);

	metadata_match_label_ = make_value_label();
	metadata_video_label_ = make_value_label();
	metadata_status_label_ = make_value_label();
	metadata_layout->addWidget(metadata_match_label_);
	metadata_layout->addWidget(metadata_video_label_);
	metadata_video_preview_image_ = new QLabel(ui_text(ui_language_, "No target video preview.", "対象動画プレビューはありません。"));
	metadata_video_preview_image_->setAlignment(Qt::AlignCenter);
	metadata_video_preview_image_->setMinimumHeight(120);
	metadata_video_preview_image_->setMaximumHeight(180);
	metadata_video_preview_image_->setStyleSheet(
		"background: #f3f8f2; border: 1px solid #c8d6c5; color: #35543a; padding: 8px;");
	metadata_layout->addWidget(metadata_video_preview_image_);
	auto *video_preview_controls = new QHBoxLayout;
	video_preview_frame_1_button_ = new QPushButton("1");
	video_preview_frame_2_button_ = new QPushButton("2");
	video_preview_frame_3_button_ = new QPushButton("3");
	QObject::connect(video_preview_frame_1_button_, &QPushButton::clicked, [this]() { load_target_video_preview(1); });
	QObject::connect(video_preview_frame_2_button_, &QPushButton::clicked, [this]() { load_target_video_preview(2); });
	QObject::connect(video_preview_frame_3_button_, &QPushButton::clicked, [this]() { load_target_video_preview(3); });
	decorate_frame_button(video_preview_frame_1_button_, ui_text(ui_language_, "1", "1枚目"),
			      ui_text(ui_language_, "Show the first representative frame.", "1枚目の代表フレームを表示します。"));
	decorate_frame_button(video_preview_frame_2_button_, ui_text(ui_language_, "2", "2枚目"),
			      ui_text(ui_language_, "Show the middle representative frame.", "中央の代表フレームを表示します。"));
	decorate_frame_button(video_preview_frame_3_button_, ui_text(ui_language_, "3", "3枚目"),
			      ui_text(ui_language_, "Show the last representative frame.", "3枚目の代表フレームを表示します。"));
	style_button(video_preview_frame_1_button_, "#6b7280", "#ffffff");
	style_button(video_preview_frame_2_button_, "#1b5e20", "#ffffff");
	style_button(video_preview_frame_3_button_, "#6b7280", "#ffffff");
	video_preview_controls->addWidget(video_preview_frame_1_button_);
	video_preview_controls->addWidget(video_preview_frame_2_button_);
	video_preview_controls->addWidget(video_preview_frame_3_button_);
	metadata_layout->addLayout(video_preview_controls);

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

	go_upload_button_ = new QPushButton(ui_text(ui_language_, "Go to Upload", "アップロードへ"));

	edit_metadata_button_ = new QPushButton(ui_text(ui_language_, "Edit Metadata", "メタデータ編集"));
	reload_metadata_button_ = new QPushButton(ui_text(ui_language_, "Reload", "再読込"));
	save_metadata_button_ = new QPushButton(ui_text(ui_language_, "Save", "保存"));
	QObject::connect(edit_metadata_button_, &QPushButton::clicked, [this]() { request_edit_metadata(); });
	QObject::connect(go_upload_button_, &QPushButton::clicked, [this]() {
		const WorkerStatusSnapshot snapshot = worker_manager_.status_snapshot();
		if (current_match_id_ > 0 && snapshot.upload_items_available) {
			for (const UploadTargetPayload &target : snapshot.upload_items.items) {
				if (target.match_id == current_match_id_) {
					selected_upload_queue_item_id_ = target.queue_item_id;
					break;
				}
			}
		}
		if (dock_tabs_) {
			dock_tabs_->setCurrentIndex(1);
		}
		refresh();
	});
	QObject::connect(reload_metadata_button_, &QPushButton::clicked, [this]() { load_latest_metadata_into_dock(); });
	QObject::connect(save_metadata_button_, &QPushButton::clicked, [this]() { save_metadata_from_dock(); });
	style_button(edit_metadata_button_, "#2e7d32", "#ffffff");
	style_button(go_upload_button_, "#2e7d32", "#ffffff");
	style_button(reload_metadata_button_, "#6b7280", "#ffffff");
	style_button(save_metadata_button_, "#2e7d32", "#ffffff");
	decorate_button(edit_metadata_button_, QStyle::SP_FileDialogDetailedView,
			ui_text(ui_language_, "Open metadata editing dialog.", "メタデータ編集ダイアログを開きます。"));
	decorate_button(go_upload_button_, QStyle::SP_ArrowForward,
			ui_text(ui_language_, "Move to the Upload tab.", "アップロードタブへ移動します。"));
	decorate_button(reload_metadata_button_, QStyle::SP_BrowserReload,
			ui_text(ui_language_, "Reload the latest metadata target.", "最新のメタデータ編集対象を再読み込みします。"));
	decorate_button(save_metadata_button_, QStyle::SP_DialogSaveButton,
			ui_text(ui_language_, "Save metadata and upload text templates.", "メタデータとアップロード文面テンプレートを保存します。"));
	auto *metadata_controls = make_dock_action_grid();
	add_dock_action_button(metadata_controls, reload_metadata_button_, 0, 0);
	add_dock_action_button(metadata_controls, save_metadata_button_, 0, 1);
	add_dock_action_button(metadata_controls, edit_metadata_button_, 1, 0);
	add_dock_action_button(metadata_controls, go_upload_button_, 1, 1);
	metadata_layout->addLayout(metadata_controls);
	metadata_layout->addWidget(metadata_status_label_);
	recording_tab_layout->addWidget(metadata_card_);
	recording_tab_layout->addStretch(1);

	upload_template_card_ = make_card("#f7fbf4", "#c5e1a5");
	auto *upload_template_layout = qobject_cast<QVBoxLayout *>(upload_template_card_->layout());
	upload_preview_title_label_ = add_card_title(upload_template_layout,
						     language_is_japanese(ui_language_) ? "アップロード文面テンプレート" : "Upload Templates",
						     "#1b5e20");
	auto *template_form = new QFormLayout;
	upload_title_template_input_ = new QLineEdit(qstr_utf8(settings.upload_title_template));
	upload_description_template_input_ = new QTextEdit(qstr_utf8(settings.upload_description_template));
	upload_description_template_input_->setMaximumHeight(110);
	upload_tags_template_input_ = new QLineEdit(qstr_utf8(settings.upload_tags_template));
	QObject::connect(upload_title_template_input_, &QLineEdit::textChanged,
			 [this]() { render_upload_preview_from_dock(); });
	QObject::connect(upload_description_template_input_, &QTextEdit::textChanged,
			 [this]() { render_upload_preview_from_dock(); });
	QObject::connect(upload_tags_template_input_, &QLineEdit::textChanged,
			 [this]() { render_upload_preview_from_dock(); });
	template_form->addRow(ui_text(ui_language_, "Title template", "タイトルテンプレート"), upload_title_template_input_);
	template_form->addRow(ui_text(ui_language_, "Description template", "説明文テンプレート"), upload_description_template_input_);
	template_form->addRow(ui_text(ui_language_, "Tags template", "タグテンプレート"), upload_tags_template_input_);
	upload_template_layout->addLayout(template_form);
	render_upload_preview_button_ = new QPushButton(ui_text(ui_language_, "Preview", "プレビュー"));
	QObject::connect(render_upload_preview_button_, &QPushButton::clicked, [this]() { render_upload_preview_from_dock(); });
	style_button(render_upload_preview_button_, "#1b5e20", "#ffffff");
	decorate_button(render_upload_preview_button_, QStyle::SP_FileDialogInfoView,
			ui_text(ui_language_, "Render the YouTube upload text preview.", "YouTubeアップロード文面のプレビューを生成します。"));
	upload_template_layout->addWidget(render_upload_preview_button_);
	upload_preview_description_ = new QTextEdit;
	upload_preview_description_->setReadOnly(true);
	upload_preview_description_->setMaximumHeight(130);
	upload_preview_warning_label_ = make_value_label();
	upload_template_layout->addWidget(upload_preview_description_);
	upload_template_layout->addWidget(upload_preview_warning_label_);
	template_tab_layout->addWidget(upload_template_card_);
	template_tab_layout->addStretch(1);

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
	decorate_button(diagnostics_details_button_, QStyle::SP_FileDialogDetailedView,
			ui_text(ui_language_, "Show or hide diagnostic details.", "診断の詳細表示を切り替えます。"));
	setup_tab_layout->addWidget(diagnostics_details_button_);
	setup_tab_layout->addWidget(diagnostics_group_);
	diagnostics_group_->setVisible(false);
	setup_tab_layout->addStretch(1);

	dock_tabs_->addTab(recording_tab, ui_text(ui_language_, "Record", "録画"));
	dock_tabs_->addTab(automatic_tab, ui_text(ui_language_, "Upload", "アップロード"));
	dock_tabs_->addTab(template_tab, ui_text(ui_language_, "Template", "テンプレート"));
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
	if (upload_to_youtube_button_) {
		style_button(upload_to_youtube_button_, palette.header_bg, "#ffffff");
	}
	if (edit_upload_metadata_button_) {
		style_button(edit_upload_metadata_button_, palette.metadata_bg_button, palette.metadata_fg_button);
	}
	if (save_upload_privacy_button_) {
		style_button(save_upload_privacy_button_, palette.settings_bg, palette.settings_fg);
	}
	if (open_youtube_button_) {
		style_button(open_youtube_button_, palette.settings_bg, palette.settings_fg);
	}
	if (select_oauth_client_secret_button_) {
		style_button(select_oauth_client_secret_button_, palette.header_bg, "#ffffff");
	}
	if (open_oauth_secrets_folder_button_) {
		style_button(open_oauth_secrets_folder_button_, palette.secondary_bg, palette.secondary_fg);
	}
	if (edit_metadata_button_) {
		style_button(edit_metadata_button_, palette.metadata_bg_button, palette.metadata_fg_button);
	}
	if (go_upload_button_) {
		style_button(go_upload_button_, palette.metadata_bg_button, palette.metadata_fg_button);
	}
	if (video_preview_frame_1_button_) {
		style_button(video_preview_frame_1_button_, palette.secondary_bg, palette.secondary_fg);
	}
	if (video_preview_frame_2_button_) {
		style_button(video_preview_frame_2_button_, palette.header_bg, "#ffffff");
	}
	if (video_preview_frame_3_button_) {
		style_button(video_preview_frame_3_button_, palette.secondary_bg, palette.secondary_fg);
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
		dock_tabs_->setTabText(1, ui_text(ui_language_, "Upload", "アップロード"));
		dock_tabs_->setTabText(2, ui_text(ui_language_, "Template", "テンプレート"));
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
		upload_title_->setText(ui_text(ui_language_, "Upload Queue", "アップロードキュー情報"));
	}
	if (queue_label_) {
		queue_label_->setText(ui_text(ui_language_, "Queue", "キュー"));
	}
	if (review_item_label_) {
		review_item_label_->setText(ui_text(ui_language_, "Review item", "確認項目"));
	}
	if (youtube_label_) {
		youtube_label_->setText(ui_text(ui_language_, "YouTube link", "YouTube連携"));
	}
	if (metadata_title_) {
		metadata_title_->setText(ui_text(ui_language_, "Metadata", "メタデータ"));
	}
	if (metadata_note_) {
		metadata_note_->setText(ui_text(
			ui_language_,
			"Enter or confirm match metadata for the latest recording.",
			"最新録画のメタデータを入力または確認します。"));
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
	if (help_button_) {
		help_button_->setText(ui_text(ui_language_, "Open Help", "ヘルプを開く"));
	}
	if (automatic_setup_button_) {
		automatic_setup_button_->setText(ui_text(ui_language_, "Open Automatic Setup", "自動録画セットアップを開く"));
	}
	if (automatic_detection_enabled_input_) {
		automatic_detection_enabled_input_->setText(ui_text(ui_language_, "Enable automatic detection frame feed",
								    "自動検出フレーム送信を有効にする"));
	}
	if (start_button_) {
		start_button_->setText(ui_text(ui_language_, "Start Recording", "録画開始"));
	}
	if (stop_button_) {
		stop_button_->setText(ui_text(ui_language_, "Stop Recording", "録画停止"));
	}
	if (retry_upload_button_) {
		retry_upload_button_->setText(ui_text(ui_language_, "Retry", "再試行"));
	}
	if (discard_upload_button_) {
		discard_upload_button_->setText(ui_text(ui_language_, "Discard", "破棄"));
	}
	if (mark_uploaded_button_) {
		mark_uploaded_button_->setText(ui_text(ui_language_, "Mark Done", "済みにする"));
	}
	if (upload_to_youtube_button_) {
		upload_to_youtube_button_->setText(ui_text(ui_language_, "Upload", "アップロード"));
	}
	if (open_youtube_button_) {
		open_youtube_button_->setText(ui_text(ui_language_, "YouTube", "YouTube"));
	}
	if (save_upload_privacy_button_) {
		save_upload_privacy_button_->setText(ui_text(ui_language_, "Save Privacy", "公開範囲を保存"));
	}
	if (edit_upload_metadata_button_) {
		edit_upload_metadata_button_->setText(ui_text(ui_language_, "Metadata", "メタデータ"));
	}
	if (oauth_authorize_button_) {
		oauth_authorize_button_->setText(ui_text(ui_language_, "Authorize", "YouTube認証"));
	}
	if (select_oauth_client_secret_button_) {
		select_oauth_client_secret_button_->setText(ui_text(ui_language_, "Auth File", "認証ファイル"));
	}
	if (open_oauth_secrets_folder_button_) {
		open_oauth_secrets_folder_button_->setText(ui_text(ui_language_, "Auth Folder", "保存先"));
	}
	if (oauth_refresh_button_) {
		oauth_refresh_button_->setText(ui_text(ui_language_, "Refresh", "トークン更新"));
	}
	if (oauth_help_button_) {
		oauth_help_button_->setText(ui_text(ui_language_, "OAuth Help", "OAuthヘルプ"));
	}
	if (edit_metadata_button_) {
		edit_metadata_button_->setText(ui_text(ui_language_, "Edit Metadata", "メタデータ編集"));
	}
	if (go_upload_button_) {
		go_upload_button_->setText(ui_text(ui_language_, "Go to Upload", "アップロードへ"));
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
		upload_preview_title_label_->setText(ui_text(ui_language_, "Upload Templates", "アップロード文面テンプレート"));
	}
	decorate_frame_button(upload_preview_frame_1_button_, ui_text(ui_language_, "1", "1枚目"),
			      ui_text(ui_language_, "Show the first representative frame.", "1枚目の代表フレームを表示します。"));
	decorate_frame_button(upload_preview_frame_2_button_, ui_text(ui_language_, "2", "2枚目"),
			      ui_text(ui_language_, "Show the middle representative frame.", "中央の代表フレームを表示します。"));
	decorate_frame_button(upload_preview_frame_3_button_, ui_text(ui_language_, "3", "3枚目"),
			      ui_text(ui_language_, "Show the last representative frame.", "3枚目の代表フレームを表示します。"));
	decorate_frame_button(video_preview_frame_1_button_, ui_text(ui_language_, "1", "1枚目"),
			      ui_text(ui_language_, "Show the first representative frame.", "1枚目の代表フレームを表示します。"));
	decorate_frame_button(video_preview_frame_2_button_, ui_text(ui_language_, "2", "2枚目"),
			      ui_text(ui_language_, "Show the middle representative frame.", "中央の代表フレームを表示します。"));
	decorate_frame_button(video_preview_frame_3_button_, ui_text(ui_language_, "3", "3枚目"),
			      ui_text(ui_language_, "Show the last representative frame.", "3枚目の代表フレームを表示します。"));
	decorate_button(automatic_setup_button_, QStyle::SP_ComputerIcon,
			ui_text(ui_language_, "Open guided automatic recording setup.", "自動録画の設定ガイドを開きます。"));
	decorate_button(start_button_, QStyle::SP_MediaPlay,
			ui_text(ui_language_, "Start manual OBS recording.", "手動でOBS録画を開始します。"));
	decorate_button(stop_button_, QStyle::SP_MediaStop,
			ui_text(ui_language_, "Stop manual OBS recording.", "手動でOBS録画を停止します。"));
	decorate_button(retry_upload_button_, QStyle::SP_BrowserReload,
			ui_text(ui_language_, "Retry the current failed or manual-review upload item.", "現在の失敗または確認待ちアップロードを再試行します。"));
	decorate_button(discard_upload_button_, QStyle::SP_DialogDiscardButton,
			ui_text(ui_language_, "Discard the current upload queue item.", "現在のアップロードキュー項目を破棄します。"));
	decorate_button(mark_uploaded_button_, QStyle::SP_DialogApplyButton,
			ui_text(ui_language_, "Mark the current item uploaded after entering a YouTube video ID.", "YouTube動画IDを入力してアップロード済みにします。"));
	decorate_button(oauth_authorize_button_, QStyle::SP_ComputerIcon,
			ui_text(ui_language_, "Open the YouTube OAuth authorization URL in your browser.",
				"YouTube OAuth認証URLをブラウザで開きます。"));
	decorate_button(select_oauth_client_secret_button_, QStyle::SP_DialogOpenButton,
			ui_text(ui_language_,
				"Copy a Google OAuth client JSON into the local OBS Duel Recorder secrets folder.",
				"Google OAuthクライアントJSONをローカルの認証ファイル保存先へコピーします。"));
	decorate_button(open_oauth_secrets_folder_button_, QStyle::SP_DirOpenIcon,
			ui_text(ui_language_, "Open the local folder that stores YouTube OAuth files.",
				"YouTube OAuthファイルのローカル保存先フォルダを開きます。"));
	decorate_button(oauth_refresh_button_, QStyle::SP_BrowserReload,
			ui_text(ui_language_, "Refresh the stored YouTube OAuth token.",
				"保存済みのYouTube OAuthトークンを更新します。"));
	decorate_button(oauth_help_button_, QStyle::SP_DialogHelpButton,
			ui_text(ui_language_, "Open the YouTube OAuth setup guide.",
				"YouTube OAuth設定ガイドを開きます。"));
	decorate_button(edit_metadata_button_, QStyle::SP_FileDialogDetailedView,
			ui_text(ui_language_, "Open metadata editing dialog.", "メタデータ編集ダイアログを開きます。"));
	decorate_button(reload_metadata_button_, QStyle::SP_BrowserReload,
			ui_text(ui_language_, "Reload the latest metadata target.", "最新のメタデータ編集対象を再読み込みします。"));
	decorate_button(save_metadata_button_, QStyle::SP_DialogSaveButton,
			ui_text(ui_language_, "Save metadata and upload text templates.", "メタデータとアップロード文面テンプレートを保存します。"));
	decorate_button(render_upload_preview_button_, QStyle::SP_FileDialogInfoView,
			ui_text(ui_language_, "Render the YouTube upload text preview.", "YouTubeアップロード文面のプレビューを生成します。"));
	decorate_button(save_inline_settings_button_, QStyle::SP_DialogSaveButton,
			ui_text(ui_language_, "Save Dock settings and restart Worker if needed.", "Dock設定を保存し、必要に応じてWorkerを再起動します。"));
	decorate_button(diagnostics_details_button_, QStyle::SP_FileDialogDetailedView,
			ui_text(ui_language_, "Show or hide diagnostic details.", "診断の詳細表示を切り替えます。"));
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
	refresh_upload_queue_selector(snapshot);
	const bool selected_target_available = has_selected_upload_target(snapshot);
	const UploadTargetPayload selected_target = selected_upload_target(snapshot);
	if (youtube_value_) {
		youtube_value_->setText(qstr_utf8(upload_readiness_summary(snapshot, ui_language_)));
	}
	if (upload_target_value_) {
		if (selected_target_available) {
			std::ostringstream out;
			out << "#" << selected_target.queue_item_id << " / "
			    << localized_upload_state(ui_language_, selected_target.state);
			if (selected_target.match_id > 0) {
				out << " / match #" << selected_target.match_id;
			}
			const std::string video_name =
				display_video_name(selected_target.video_path, selected_target.resolved_video_path);
			if (!video_name.empty()) {
				out << "\nVideo: " << video_name;
			}
			out << "\n" << (ui_language_ == "ja" ? "メタデータ: " : "Metadata: ")
			    << (selected_target.metadata_confirmed ? (ui_language_ == "ja" ? "確認済み" : "confirmed") :
								     (ui_language_ == "ja" ? "未確認" : "missing"));
			if (!selected_target.upload_metadata.deck_name.empty()) {
				out << "\nDeck: " << selected_target.upload_metadata.deck_name;
			}
			if (!selected_target.upload_metadata.opponent_deck.empty()) {
				out << "\nOpponent: " << selected_target.upload_metadata.opponent_deck;
			}
			if (!selected_target.upload_metadata.result.empty() || !selected_target.upload_metadata.rank.empty() ||
			    !selected_target.upload_metadata.dp.empty()) {
				out << "\nResult/Rank/DP: "
				    << (selected_target.upload_metadata.result.empty() ? "-" : selected_target.upload_metadata.result)
				    << " / "
				    << (selected_target.upload_metadata.rank.empty() ? "-" : selected_target.upload_metadata.rank)
				    << " / "
				    << (selected_target.upload_metadata.dp.empty() ? "-" : selected_target.upload_metadata.dp);
			}
			upload_target_value_->setText(qstr_utf8(out.str()));
		} else {
			upload_target_value_->setText(qstr_utf8(upload_target_summary(snapshot, ui_language_)));
		}
	}
	if (upload_text_value_) {
		if (selected_target_available) {
			std::ostringstream out;
			out << "Title: "
			    << (selected_target.upload_metadata.title.empty() ? "(empty)" :
									 selected_target.upload_metadata.title)
			    << "\nTags: "
			    << (selected_target.upload_metadata.tags.empty() ? "(empty)" :
									selected_target.upload_metadata.tags);
			if (!selected_target.upload_metadata.description.empty()) {
				out << "\n" << selected_target.upload_metadata.description;
			}
			upload_text_value_->setText(qstr_utf8(out.str()));
		} else {
			upload_text_value_->setText(qstr_utf8(upload_text_summary(snapshot, ui_language_)));
		}
	}
	if (upload_blocking_value_) {
		upload_blocking_value_->setText(
			selected_target_available ?
				qstr_utf8(selected_target.can_upload ?
						  (ui_language_ == "ja" ? "✓ アップロード可能" : "✓ uploadable") :
						  localized_blocking_reasons(ui_language_, selected_target.blocking_reasons)) :
				qstr_utf8(upload_blocking_summary(snapshot, ui_language_)));
	}
	if (upload_result_value_ && upload_result_value_->text().isEmpty()) {
		upload_result_value_->setText(ui_text(ui_language_, "No upload has run from this Dock yet.",
						      "このDockからのアップロード実行結果はまだありません。"));
	}
	if (upload_privacy_input_ && selected_target_available) {
		const std::string privacy = selected_target.upload_metadata.privacy_status;
		if (!privacy.empty()) {
			const int index = upload_privacy_input_->findData(qstr_utf8(privacy));
			if (index >= 0) {
				upload_privacy_input_->setCurrentIndex(index);
			}
		}
	}
	endpoint_value_->setText(qstr_utf8(endpoint));
	user_data_value_->setText(snapshot.user_data_dir.empty() ? QString::fromUtf8("not configured") : qstr_wide(snapshot.user_data_dir));
	worker_path_value_->setText(worker_path.empty() ? QString::fromUtf8("not available") : qstr_wide(worker_path));
	logs_value_->setText(logs.empty() ? QString::fromUtf8("not available") : qstr_utf8(logs));
	ownership_value_->setText(QString::fromUtf8(ownership_name(snapshot.ownership)));
	detail_value_->setText(qstr_utf8(snapshot.error.empty() ? snapshot.last_probe_summary : snapshot.error));
	action_value_->setText(QString::fromUtf8(recommended_action(snapshot.state)));
	const bool worker_running = snapshot.state == WorkerDiagnosticState::running;
	const bool currently_recording = snapshot.recording_state_available && snapshot.recording_state.state == "recording";
	if (start_button_) {
		start_button_->setEnabled(worker_running);
		start_button_->setVisible(!currently_recording);
	}
	if (stop_button_) {
		stop_button_->setEnabled(worker_running);
		stop_button_->setVisible(currently_recording);
	}
	const bool queue_action_available = worker_running && selected_target_available &&
					    selected_target.state != "uploaded" &&
					    selected_target.state != "discarded";
	const bool upload_target_available = worker_running && selected_target_available &&
					     selected_target.can_upload &&
					     !upload_request_in_progress_;
	if (upload_to_youtube_button_) {
		upload_to_youtube_button_->setEnabled(upload_target_available);
	}
	if (edit_upload_metadata_button_) {
		edit_upload_metadata_button_->setEnabled(worker_running && selected_target_available && selected_target.match_id > 0);
	}
	if (go_upload_button_) {
		go_upload_button_->setEnabled(worker_running && current_match_id_ > 0);
	}
	if (save_upload_privacy_button_) {
		save_upload_privacy_button_->setEnabled(worker_running);
	}
	if (open_youtube_button_) {
		open_youtube_button_->setEnabled(!last_uploaded_url_.empty());
	}
	if (retry_upload_button_) {
		retry_upload_button_->setEnabled(queue_action_available);
	}
	if (discard_upload_button_) {
		discard_upload_button_->setEnabled(queue_action_available);
	}
	if (mark_uploaded_button_) {
		mark_uploaded_button_->setEnabled(queue_action_available &&
						  selected_target.state == "need_manual_review");
	}
	if (oauth_authorize_button_) {
		oauth_authorize_button_->setEnabled(worker_running && snapshot.upload_status.client_secret_configured);
		const bool primary = snapshot.upload_status.readiness_state == "token_missing" ||
				     snapshot.upload_status.readiness_state == "token_invalid";
		style_button(oauth_authorize_button_, primary ? "#1b5e20" : "#6b7280", "#ffffff");
	}
	if (select_oauth_client_secret_button_) {
		const bool client_secret_needed = snapshot.upload_status.readiness_state == "client_secret_missing" ||
						  !snapshot.upload_status.client_secret_configured;
		select_oauth_client_secret_button_->setEnabled(worker_running && !snapshot.user_data_dir.empty());
		style_button(select_oauth_client_secret_button_, client_secret_needed ? "#1b5e20" : "#2e7d32",
			     "#ffffff");
	}
	if (open_oauth_secrets_folder_button_) {
		open_oauth_secrets_folder_button_->setEnabled(!snapshot.user_data_dir.empty());
	}
	if (oauth_refresh_button_) {
		const bool refresh_needed = snapshot.upload_status.readiness_state == "token_expired_refreshable" ||
					    snapshot.upload_status.token_refreshable;
		oauth_refresh_button_->setEnabled(worker_running && refresh_needed);
		style_button(oauth_refresh_button_, snapshot.upload_status.readiness_state == "token_expired_refreshable" ?
						   "#1b5e20" : "#6b7280",
			     "#ffffff");
	}
	if (oauth_help_button_) {
		oauth_help_button_->setEnabled(true);
	}
	if (edit_metadata_button_) {
		edit_metadata_button_->setEnabled(worker_running);
	}
	if (video_preview_frame_1_button_) {
		video_preview_frame_1_button_->setEnabled(worker_running && current_match_id_ > 0);
	}
	if (video_preview_frame_2_button_) {
		video_preview_frame_2_button_->setEnabled(worker_running && current_match_id_ > 0);
	}
	if (video_preview_frame_3_button_) {
		video_preview_frame_3_button_->setEnabled(worker_running && current_match_id_ > 0);
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

	if (worker_running) {
		if (!metadata_loaded_after_worker_ready_) {
			metadata_loaded_after_worker_ready_ = true;
			load_latest_metadata_into_dock();
		}
	} else {
		metadata_loaded_after_worker_ready_ = false;
	}

	handle_automatic_recording(snapshot);
	handle_automatic_detection_feed(snapshot);

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
	auto *automatic_detection_enabled = new QCheckBox(ui_text(ui_language_, "Enable automatic detection frame feed",
								 "自動検出フレーム送信を有効にする"));
	automatic_detection_enabled->setChecked(settings.automatic_detection_enabled);
	auto *automatic_detection_interval = new QSpinBox;
	automatic_detection_interval->setRange(1000, 60000);
	automatic_detection_interval->setSingleStep(1000);
	automatic_detection_interval->setSuffix(QString::fromUtf8(" ms"));
	automatic_detection_interval->setValue(settings.automatic_detection_interval_ms);
	auto *settings_path = new QLabel(qstr_wide(settings.settings_path));
	settings_path->setTextInteractionFlags(Qt::TextSelectableByMouse);
	settings_path->setWordWrap(true);

	form->addRow(ui_text(ui_language_, "Host", "ホスト"), host_input);
	form->addRow(ui_text(ui_language_, "Port", "ポート"), port_input);
	form->addRow(ui_text(ui_language_, "User data dir", "ユーザーデータ保存先"), user_data_input);
	form->addRow(ui_text(ui_language_, "Dock theme", "Dock テーマ"), theme_input);
	form->addRow(ui_text(ui_language_, "Language", "言語"), language_input);
	form->addRow(ui_text(ui_language_, "Automatic detection", "自動検出"), automatic_detection_enabled);
	form->addRow(ui_text(ui_language_, "Frame interval", "送信間隔"), automatic_detection_interval);
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
	settings.automatic_detection_enabled = automatic_detection_enabled->isChecked();
	settings.automatic_detection_interval_ms = automatic_detection_interval->value();
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
	if (!has_selected_upload_target(snapshot)) {
		return;
	}
	const QueueActionItemPayload item = selected_upload_target(snapshot).item;
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
	if (!has_selected_upload_target(snapshot)) {
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
	const QueueCommandResult result = worker_manager_.send_queue_command(selected_upload_target(snapshot).queue_item_id, "discard");
	log_queue_command_result("discard", result);
	refresh();
}

void PluginUiController::request_upload_mark_uploaded()
{
	const WorkerStatusSnapshot snapshot = worker_manager_.status_snapshot();
	if (!has_selected_upload_target(snapshot)) {
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
	const QString url = QString::fromUtf8("https://youtu.be/") + video_id.trimmed();
	const auto answer = QMessageBox::question(
		dock_widget_,
		ui_text(ui_language_, "Mark uploaded", "アップロード済みにする"),
		ui_text(ui_language_, "Store this YouTube URL for the selected queue item?\n\n",
			"選択中のキュー項目に次のYouTube URLを保存しますか？\n\n") +
			url,
		QMessageBox::Yes | QMessageBox::No,
		QMessageBox::No);
	if (answer != QMessageBox::Yes) {
		return;
	}
	const QueueCommandResult result = worker_manager_.send_queue_command(
		selected_upload_target(snapshot).queue_item_id,
		"mark_uploaded",
		video_id.trimmed().toStdString());
	log_queue_command_result("mark_uploaded", result);
	refresh();
}

void PluginUiController::request_save_upload_privacy()
{
	if (!upload_privacy_input_) {
		return;
	}
	const std::string privacy_status = utf8_string(upload_privacy_input_->currentData().toString());
	const UploadSettingsResult result = worker_manager_.update_upload_settings(privacy_status);
	if (!result.accepted()) {
		QMessageBox::warning(
			dock_widget_,
			ui_text(ui_language_, "Upload privacy", "アップロード公開範囲"),
			qstr_utf8(result.error.empty() ? "Worker could not save upload privacy settings." :
							result.error));
		return;
	}
	if (upload_result_value_) {
		upload_result_value_->setText(ui_text(ui_language_, "Upload privacy setting was saved.",
						      "アップロード公開範囲を保存しました。"));
	}
	refresh();
}

void PluginUiController::request_open_uploaded_youtube()
{
	if (last_uploaded_url_.empty()) {
		return;
	}
	QDesktopServices::openUrl(QUrl(qstr_utf8(last_uploaded_url_)));
}

void PluginUiController::request_upload_to_youtube()
{
	const WorkerStatusSnapshot snapshot = worker_manager_.status_snapshot();
	if (!has_selected_upload_target(snapshot)) {
		return;
	}
	const UploadTargetPayload target = selected_upload_target(snapshot);
	if (!target.can_upload) {
		QMessageBox::warning(
			dock_widget_,
			ui_text(ui_language_, "Upload blocked", "アップロード不可"),
			qstr_utf8(localized_blocking_reasons(ui_language_, target.blocking_reasons)));
		return;
	}
	std::string selected_privacy = target.upload_metadata.privacy_status;
	if (upload_privacy_input_) {
		const std::string privacy_status = utf8_string(upload_privacy_input_->currentData().toString());
		const UploadSettingsResult settings = worker_manager_.update_upload_settings(privacy_status);
		if (!settings.accepted()) {
			QMessageBox::warning(
				dock_widget_,
				ui_text(ui_language_, "Upload privacy", "アップロード公開範囲"),
				qstr_utf8(settings.error.empty() ? "Worker could not save upload privacy settings." :
							 settings.error));
			return;
		}
		selected_privacy = privacy_status;
	}

	std::ostringstream message;
	message << "Upload queue item #" << target.queue_item_id << " to YouTube using the Google provider?\n\n"
		<< "Video: " << target.video_path << "\n"
		<< "Title: " << target.upload_metadata.title << "\n"
		<< "Privacy: " << selected_privacy;
	const auto answer = QMessageBox::question(
		dock_widget_,
		ui_text(ui_language_, "Confirm YouTube upload", "YouTubeアップロード確認"),
		qstr_utf8(message.str()),
		QMessageBox::Yes | QMessageBox::No,
		QMessageBox::No);
	if (answer != QMessageBox::Yes) {
		return;
	}

	upload_request_in_progress_ = true;
	if (upload_result_value_) {
		upload_result_value_->setText(ui_text(ui_language_, "Uploading to YouTube...",
						      "YouTubeへアップロード中です..."));
	}
	refresh();

	const int item_id = target.queue_item_id;
	QPointer<QWidget> dock = dock_widget_;
	std::thread([this, dock, item_id]() {
		const UploadProcessResult result = worker_manager_.process_upload_item(item_id, "google");
		QWidget *dock_widget = dock.data();
		if (!dock_widget) {
			return;
		}
		QMetaObject::invokeMethod(
			dock_widget,
			[this, result]() {
				upload_request_in_progress_ = false;
				std::ostringstream text;
				if (result.accepted()) {
					text << "processed=" << (result.processed ? "true" : "false");
					if (!result.outcome.empty()) {
						text << " outcome=" << result.outcome;
					}
					if (!result.reason.empty()) {
						text << " reason=" << result.reason;
					}
					if (!result.youtube_video_id.empty()) {
						text << "\nyoutube_video_id=" << result.youtube_video_id;
					}
					if (!result.youtube_url.empty()) {
						last_uploaded_url_ = result.youtube_url;
						text << "\n" << result.youtube_url;
					}
				} else {
					text << "upload failed: "
					     << (result.error.empty() ? "Worker did not accept upload." : result.error);
				}
				if (upload_result_value_) {
					upload_result_value_->setText(qstr_utf8(text.str()));
				}
				refresh();
			},
			Qt::QueuedConnection);
	}).detach();
}

bool PluginUiController::ensure_upload_secrets_dir(QString &secrets_dir)
{
	const WorkerStatusSnapshot snapshot = worker_manager_.status_snapshot();
	if (snapshot.user_data_dir.empty()) {
		QMessageBox::warning(dock_widget_,
				     ui_text(ui_language_, "YouTube OAuth setup", "YouTube OAuth設定"),
				     ui_text(ui_language_,
					     "user_data_dir is not configured. Open the Manage tab, set the user data folder, then save.",
					     "user_data_dirが未設定です。管理タブでユーザーデータフォルダを指定して保存してください。"));
		return false;
	}

	secrets_dir = QDir(qstr_wide(snapshot.user_data_dir)).filePath(QStringLiteral("config/secrets"));
	if (QDir(secrets_dir).exists()) {
		return true;
	}
	if (QDir().mkpath(secrets_dir)) {
		return true;
	}

	QMessageBox::warning(
		dock_widget_,
		ui_text(ui_language_, "YouTube OAuth setup", "YouTube OAuth設定"),
		ui_text(ui_language_, "Could not create the YouTube OAuth secrets folder:\n\n",
			"YouTube OAuth認証ファイルの保存先フォルダを作成できませんでした:\n\n") +
			QDir::toNativeSeparators(secrets_dir));
	return false;
}

void PluginUiController::request_select_upload_client_secret()
{
	QString secrets_dir;
	if (!ensure_upload_secrets_dir(secrets_dir)) {
		return;
	}

	const QString source = QFileDialog::getOpenFileName(
		dock_widget_,
		ui_text(ui_language_, "Select Google OAuth client JSON", "Google OAuthクライアントJSONを選択"),
		QString(),
		ui_text(ui_language_, "JSON files (*.json);;All files (*)", "JSONファイル (*.json);;すべてのファイル (*)"));
	if (source.isEmpty()) {
		return;
	}

	const QFileInfo source_info(source);
	if (!source_info.isFile()) {
		QMessageBox::warning(dock_widget_,
				     ui_text(ui_language_, "YouTube OAuth setup", "YouTube OAuth設定"),
				     ui_text(ui_language_, "Select a JSON file downloaded from Google Cloud Console.",
					     "Google Cloud Consoleから取得したJSONファイルを選択してください。"));
		return;
	}

	const QString destination = QDir(secrets_dir).filePath(QStringLiteral("youtube-client-secret.json"));
	const QFileInfo destination_info(destination);
	if (source_info.absoluteFilePath() == destination_info.absoluteFilePath()) {
		if (upload_result_value_) {
			upload_result_value_->setText(ui_text(ui_language_,
							      "YouTube auth file is already configured.",
							      "YouTube認証ファイルは既に設定されています。"));
		}
		refresh();
		return;
	}

	if (destination_info.exists()) {
		const QMessageBox::StandardButton answer = QMessageBox::question(
			dock_widget_,
			ui_text(ui_language_, "Overwrite YouTube auth file?", "YouTube認証ファイルを上書きしますか？"),
			ui_text(ui_language_,
				"youtube-client-secret.json already exists. Replace it with the selected file?",
				"youtube-client-secret.jsonは既に存在します。選択したファイルで置き換えますか？"),
			QMessageBox::Yes | QMessageBox::No,
			QMessageBox::No);
		if (answer != QMessageBox::Yes) {
			return;
		}
		if (!QFile::remove(destination)) {
			QMessageBox::warning(
				dock_widget_,
				ui_text(ui_language_, "YouTube OAuth setup", "YouTube OAuth設定"),
				ui_text(ui_language_, "Could not replace the existing YouTube auth file.",
					"既存のYouTube認証ファイルを置き換えられませんでした。"));
			return;
		}
	}

	if (!QFile::copy(source, destination)) {
		QMessageBox::warning(
			dock_widget_,
			ui_text(ui_language_, "YouTube OAuth setup", "YouTube OAuth設定"),
			ui_text(ui_language_, "Could not copy the selected JSON file into the secrets folder.",
				"選択したJSONファイルを認証ファイル保存先へコピーできませんでした。"));
		return;
	}

	if (upload_result_value_) {
		upload_result_value_->setText(ui_text(ui_language_,
						      "YouTube auth file was saved as youtube-client-secret.json.",
						      "YouTube認証ファイルをyoutube-client-secret.jsonとして保存しました。"));
	}
	refresh();
}

void PluginUiController::request_open_upload_secrets_folder()
{
	QString secrets_dir;
	if (!ensure_upload_secrets_dir(secrets_dir)) {
		return;
	}
	if (!QDesktopServices::openUrl(QUrl::fromLocalFile(secrets_dir))) {
		QMessageBox::information(
			dock_widget_,
			ui_text(ui_language_, "YouTube OAuth folder", "YouTube OAuthフォルダ"),
			ui_text(ui_language_, "Open this folder manually:\n\n", "次のフォルダを手動で開いてください:\n\n") +
				QDir::toNativeSeparators(secrets_dir));
	}
}

void PluginUiController::request_upload_oauth_authorization()
{
	const OAuthAuthorizationUrlResult result = worker_manager_.request_upload_oauth_authorization_url();
	if (!result.accepted()) {
		if (upload_result_value_) {
			upload_result_value_->setText(
				qstr_utf8(result.error.empty() ? "Worker could not create a YouTube OAuth authorization URL." :
							 result.error));
		}
		QMessageBox::warning(
			dock_widget_,
			ui_text(ui_language_, "YouTube authorization", "YouTube認証"),
			qstr_utf8(result.error.empty() ? "Worker could not create a YouTube OAuth authorization URL." :
							result.error));
		blog(LOG_WARNING, "%s upload oauth authorization-url failed status=%d http=%lu error=%s",
		     kLogPrefix,
		     static_cast<int>(result.status),
		     result.http_status,
		     result.error.c_str());
		refresh();
		return;
	}

	const QUrl authorization_url(qstr_utf8(result.authorization_url));
	if (!QDesktopServices::openUrl(authorization_url)) {
		if (upload_result_value_) {
			upload_result_value_->setText(qstr_utf8("Unable to open the browser for YouTube authorization."));
		}
		QMessageBox::warning(
			dock_widget_,
			ui_text(ui_language_, "YouTube authorization", "YouTube認証"),
			ui_text(ui_language_,
				"Unable to open the browser. Copy this authorization URL:\n\n",
				"ブラウザを開けませんでした。次の認証URLをコピーしてください:\n\n") +
				qstr_utf8(result.authorization_url));
		refresh();
		return;
	}

	QString message = ui_text(ui_language_,
				  "The authorization page was opened in your browser. Complete Google sign-in, then return to OBS and refresh the upload status.",
				  "ブラウザで認証ページを開きました。Googleへのログインを完了してからOBSへ戻り、アップロード状態を確認してください。");
	if (!result.redirect_uri.empty()) {
		message += QString::fromUtf8("\n\nRedirect URI: ") + qstr_utf8(result.redirect_uri);
	}
	QMessageBox::information(
		dock_widget_,
		ui_text(ui_language_, "YouTube authorization", "YouTube認証"),
		message);
	refresh();
}

void PluginUiController::request_upload_oauth_refresh()
{
	const WorkerActionResult result = worker_manager_.refresh_upload_oauth_token();
	if (!result.accepted()) {
		if (upload_result_value_) {
			upload_result_value_->setText(
				qstr_utf8(result.error.empty() ? "Worker could not refresh the YouTube OAuth token." :
							 result.error));
		}
		QMessageBox::warning(
			dock_widget_,
			ui_text(ui_language_, "Refresh YouTube token", "YouTubeトークン更新"),
			qstr_utf8(result.error.empty() ? "Worker could not refresh the YouTube OAuth token." :
							result.error));
		blog(LOG_WARNING, "%s upload oauth refresh failed status=%d http=%lu error=%s",
		     kLogPrefix,
		     static_cast<int>(result.status),
		     result.http_status,
		     result.error.c_str());
		refresh();
		return;
	}

	QMessageBox::information(
		dock_widget_,
		ui_text(ui_language_, "Refresh YouTube token", "YouTubeトークン更新"),
		ui_text(ui_language_, "The stored YouTube OAuth token was refreshed.",
			"保存済みのYouTube OAuthトークンを更新しました。"));
	refresh();
}

void PluginUiController::request_upload_oauth_help()
{
	const QString docs_url = QString::fromUtf8(
		"https://github.com/Tao-pyth/obs-duel-recorder/blob/main/docs/user/youtube-oauth.md");
	if (!QDesktopServices::openUrl(QUrl(docs_url))) {
		QMessageBox::information(
			dock_widget_,
			ui_text(ui_language_, "YouTube OAuth help", "YouTube OAuthヘルプ"),
			ui_text(ui_language_, "Open this setup guide:\n\n", "次の設定ガイドを開いてください:\n\n") +
				docs_url);
	}
}

bool PluginUiController::has_selected_upload_target(const WorkerStatusSnapshot &snapshot) const
{
	if (snapshot.upload_items_available && selected_upload_queue_item_id_ > 0) {
		for (const UploadTargetPayload &target : snapshot.upload_items.items) {
			if (target.queue_item_id == selected_upload_queue_item_id_) {
				return true;
			}
		}
	}
	return snapshot.upload_next_target_available && snapshot.upload_next_target.found;
}

UploadTargetPayload PluginUiController::selected_upload_target(const WorkerStatusSnapshot &snapshot) const
{
	if (snapshot.upload_items_available && selected_upload_queue_item_id_ > 0) {
		for (const UploadTargetPayload &target : snapshot.upload_items.items) {
			if (target.queue_item_id == selected_upload_queue_item_id_) {
				return target;
			}
		}
	}
	return snapshot.upload_next_target.target;
}

void PluginUiController::refresh_upload_queue_selector(const WorkerStatusSnapshot &snapshot)
{
	if (!upload_queue_input_) {
		return;
	}
	QSignalBlocker blocker(upload_queue_input_);
	upload_queue_input_->clear();
	if (!snapshot.upload_items_available || snapshot.upload_items.items.empty()) {
		upload_queue_input_->addItem(ui_text(ui_language_, "No upload queue items", "アップロードキュー項目なし"), 0);
		selected_upload_queue_item_id_ = 0;
		return;
	}

	bool selected_seen = false;
	for (const UploadTargetPayload &target : snapshot.upload_items.items) {
		upload_queue_input_->addItem(qstr_utf8(upload_queue_item_label(target, ui_language_)), target.queue_item_id);
		if (target.queue_item_id == selected_upload_queue_item_id_) {
			selected_seen = true;
			upload_queue_input_->setCurrentIndex(upload_queue_input_->count() - 1);
		}
	}
	if (!selected_seen) {
		selected_upload_queue_item_id_ = snapshot.upload_items.items.front().queue_item_id;
		upload_queue_input_->setCurrentIndex(0);
	}
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
	auto *privacy_preview = new QLineEdit(qstr_utf8(preview.preview.privacy_status.empty() ? "private" :
							      preview.preview.privacy_status));
	privacy_preview->setReadOnly(true);
	form->addRow(ui_text(ui_language_, "Title", "タイトル"), title_preview);
	form->addRow(ui_text(ui_language_, "Description", "説明文"), description_preview);
	form->addRow(ui_text(ui_language_, "Tags", "タグ"), tags_preview);
	form->addRow(ui_text(ui_language_, "Privacy", "公開設定"), privacy_preview);
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
		if (metadata_video_preview_image_) {
			metadata_video_preview_image_->clear();
			metadata_video_preview_image_->setText(ui_text(ui_language_, "No target video preview.",
									 "対象動画プレビューはありません。"));
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
	load_target_video_preview(metadata_preview_frame_index_);
}

void PluginUiController::load_match_metadata_into_dock(int match_id)
{
	const MatchFetchResult fetched = worker_manager_.fetch_match(match_id);
	PluginSettings settings = load_plugin_settings();
	if (!fetched.reachable()) {
		if (metadata_status_label_) {
			metadata_status_label_->setText(qstr_utf8(fetched.error.empty() ? "Selected match metadata is unavailable." :
									 fetched.error));
		}
		return;
	}

	current_match_id_ = fetched.match.id;
	if (metadata_match_label_) {
		metadata_match_label_->setText(QString::fromUtf8("Match #%1").arg(fetched.match.id));
	}
	const WorkerStatusSnapshot snapshot = worker_manager_.status_snapshot();
	std::string video;
	if (snapshot.upload_items_available) {
		for (const UploadTargetPayload &target : snapshot.upload_items.items) {
			if (target.match_id == fetched.match.id) {
				video = target.video_path.empty() ? target.resolved_video_path : target.video_path;
				break;
			}
		}
	}
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
		metadata_status_label_->setText(ui_text(ui_language_, "Loaded selected upload target metadata.",
							"選択中のアップロード対象メタデータを読み込みました。"));
	}
	load_target_video_preview(metadata_preview_frame_index_);
}

void PluginUiController::load_target_video_preview(int frame_index)
{
	if (frame_index < 1) {
		frame_index = 1;
	}
	if (frame_index > 3) {
		frame_index = 3;
	}
	metadata_preview_frame_index_ = frame_index;
	if (!metadata_video_preview_image_) {
		return;
	}
	if (current_match_id_ <= 0) {
		metadata_video_preview_image_->clear();
		metadata_video_preview_image_->setText(ui_text(ui_language_, "No target video preview.",
							       "対象動画プレビューはありません。"));
		return;
	}

	const VideoPreviewResult result = worker_manager_.fetch_match_video_preview(current_match_id_, frame_index);
	if (!result.reachable()) {
		metadata_video_preview_image_->clear();
		metadata_video_preview_image_->setText(qstr_utf8(result.error.empty() ?
								  "Target video preview is unavailable." :
								  result.error));
		return;
	}
	if (metadata_video_label_ && !result.preview.video_path.empty()) {
		metadata_video_label_->setText(ui_text(ui_language_, "Preview source: ", "確認対象: ") +
					       qstr_utf8(result.preview.video_path));
	}
	if (!result.preview.available) {
		metadata_video_preview_image_->clear();
		const std::string reason = result.preview.reason.empty() ? "preview_unavailable" : result.preview.reason;
		metadata_video_preview_image_->setText(
			qstr_utf8((ui_language_ == "ja" ? "プレビューを表示できません: " : "Preview unavailable: ") + reason));
		return;
	}

	const QByteArray encoded(result.preview.content_base64.data(),
				 static_cast<int>(result.preview.content_base64.size()));
	const QByteArray bytes = QByteArray::fromBase64(encoded);
	QPixmap pixmap;
	if (bytes.isEmpty() || !pixmap.loadFromData(bytes, "PNG")) {
		metadata_video_preview_image_->clear();
		metadata_video_preview_image_->setText(
			ui_text(ui_language_, "Preview image could not be decoded.", "プレビュー画像をデコードできませんでした。"));
		return;
	}

	metadata_video_preview_image_->setText(QString());
	metadata_video_preview_image_->setPixmap(
		pixmap.scaled(metadata_video_preview_image_->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

void PluginUiController::load_upload_video_preview(int frame_index)
{
	if (frame_index < 1) {
		frame_index = 1;
	}
	if (frame_index > 3) {
		frame_index = 3;
	}
	upload_preview_frame_index_ = frame_index;
	if (!upload_video_preview_image_) {
		return;
	}
	const WorkerStatusSnapshot snapshot = worker_manager_.status_snapshot();
	if (!has_selected_upload_target(snapshot)) {
		upload_video_preview_image_->clear();
		upload_video_preview_image_->setText(ui_text(ui_language_, "No upload target preview.",
							    "アップロード対象プレビューはありません。"));
		return;
	}
	const UploadTargetPayload target = selected_upload_target(snapshot);
	if (target.match_id <= 0) {
		upload_video_preview_image_->clear();
		upload_video_preview_image_->setText(ui_text(ui_language_, "Upload target has no match metadata.",
							    "アップロード対象に対戦メタデータがありません。"));
		return;
	}
	const VideoPreviewResult result = worker_manager_.fetch_match_video_preview(target.match_id, frame_index);
	if (!result.reachable() || !result.preview.available) {
		upload_video_preview_image_->clear();
		const std::string reason = !result.error.empty() ? result.error :
					   (result.preview.reason.empty() ? "preview_unavailable" : result.preview.reason);
		upload_video_preview_image_->setText(
			qstr_utf8((ui_language_ == "ja" ? "プレビューを表示できません: " : "Preview unavailable: ") + reason));
		return;
	}
	const QByteArray encoded(result.preview.content_base64.data(),
				 static_cast<int>(result.preview.content_base64.size()));
	const QByteArray bytes = QByteArray::fromBase64(encoded);
	QPixmap pixmap;
	if (bytes.isEmpty() || !pixmap.loadFromData(bytes, "PNG")) {
		upload_video_preview_image_->clear();
		upload_video_preview_image_->setText(
			ui_text(ui_language_, "Preview image could not be decoded.", "プレビュー画像をデコードできませんでした。"));
		return;
	}
	upload_video_preview_image_->setText(QString());
	upload_video_preview_image_->setPixmap(
		pixmap.scaled(upload_video_preview_image_->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
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
	settings.automatic_detection_enabled = automatic_detection_enabled_input_ ?
						       automatic_detection_enabled_input_->isChecked() :
						       settings.automatic_detection_enabled;
	settings.automatic_detection_interval_ms = automatic_detection_interval_input_ ?
							automatic_detection_interval_input_->value() :
							settings.automatic_detection_interval_ms;
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

bool PluginUiController::capture_current_screenshot_base64(std::string &frame_base64,
							   std::string &screenshot_path,
							   std::string &error,
							   bool delete_after_read)
{
	const std::string previous = take_frontend_string(obs_frontend_get_last_screenshot());
	obs_frontend_take_screenshot();

	QElapsedTimer timer;
	timer.start();
	while (timer.elapsed() < 5000) {
		QCoreApplication::processEvents(QEventLoop::AllEvents, 100);
		const std::string current = take_frontend_string(obs_frontend_get_last_screenshot());
		if (!current.empty() && current != previous) {
			QFileInfo info(qstr_utf8(current));
			if (info.suffix().compare(QString::fromUtf8("png"), Qt::CaseInsensitive) != 0) {
				error = "OBS saved a non-PNG screenshot; automatic setup requires PNG output.";
				return false;
			}

			QFile file(info.absoluteFilePath());
			if (!file.open(QIODevice::ReadOnly)) {
				error = "Captured screenshot could not be opened.";
				return false;
			}
			const QByteArray bytes = file.readAll();
			if (bytes.isEmpty()) {
				error = "Captured screenshot was empty.";
				return false;
			}
			const QByteArray encoded = bytes.toBase64();
			frame_base64 = std::string(encoded.constData(), static_cast<size_t>(encoded.size()));
			screenshot_path = current;
			if (delete_after_read) {
				file.close();
				QFile::remove(info.absoluteFilePath());
			}
			return true;
		}
		QThread::msleep(50);
	}

	error = "Timed out waiting for OBS screenshot capture.";
	return false;
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

	auto *capture_buttons = new QHBoxLayout;
	auto *capture_start_button = new QPushButton(ui_text(ui_language_, "Capture Start Screen", "開始画面として取得"));
	auto *capture_end_button = new QPushButton(ui_text(ui_language_, "Capture End Screen", "終了画面として取得"));
	auto *test_current_button = new QPushButton(ui_text(ui_language_, "Test Current Screen", "現在画面でテスト"));
	decorate_button(capture_start_button, QStyle::SP_ComputerIcon,
			ui_text(ui_language_, "Capture the current OBS Program screen as the start template.", "現在のOBS Program画面を開始テンプレートとして取得します。"));
	decorate_button(capture_end_button, QStyle::SP_ComputerIcon,
			ui_text(ui_language_, "Capture the current OBS Program screen as the end template.", "現在のOBS Program画面を終了テンプレートとして取得します。"));
	decorate_button(test_current_button, QStyle::SP_FileDialogInfoView,
			ui_text(ui_language_, "Test the current OBS Program screen against start and end templates.", "現在のOBS Program画面を開始/終了テンプレートの両方でテストします。"));
	capture_buttons->addWidget(capture_start_button);
	capture_buttons->addWidget(capture_end_button);
	capture_buttons->addWidget(test_current_button);
	layout->addLayout(capture_buttons);

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
	decorate_button(validate_button, QStyle::SP_DialogApplyButton,
			ui_text(ui_language_, "Validate setup readiness without changing templates.", "テンプレートを変更せずにセットアップ状態を検証します。"));
	decorate_button(register_button, QStyle::SP_DialogSaveButton,
			ui_text(ui_language_, "Register the advanced template path shown above.", "上の詳細テンプレートパスを登録します。"));
	decorate_button(test_button, QStyle::SP_FileDialogInfoView,
			ui_text(ui_language_, "Run the advanced text/fixture detection test.", "詳細入力の文字列/フィクスチャで検出テストを実行します。"));
	decorate_button(close_button, QStyle::SP_DialogCloseButton,
			ui_text(ui_language_, "Close automatic recording setup.", "自動録画セットアップを閉じます。"));
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
	auto capture_template = [this, threshold_input, confirmations_input, status, result_text](const std::string &kind) {
		std::string frame_base64;
		std::string screenshot_path;
		std::string error;
		if (!capture_current_screenshot_base64(frame_base64, screenshot_path, error)) {
			status->setPlainText(qstr_utf8("Current screen capture\nerror=" + error));
			return;
		}
		const WorkerActionResult result = worker_manager_.capture_detection_template(
			kind, frame_base64, threshold_input->value(), confirmations_input->value());
		QString text = result_text(kind == "start" ? "Captured start template" : "Captured end template", result);
		text.append(QString::fromUtf8("\nscreenshot="));
		text.append(qstr_utf8(screenshot_path));
		status->setPlainText(text);
	};
	QObject::connect(capture_start_button, &QPushButton::clicked, [capture_template]() {
		capture_template("start");
	});
	QObject::connect(capture_end_button, &QPushButton::clicked, [capture_template]() {
		capture_template("end");
	});
	QObject::connect(test_current_button, &QPushButton::clicked,
			 [this, status, result_text]() {
				 std::string frame_base64;
				 std::string screenshot_path;
				 std::string error;
				 if (!capture_current_screenshot_base64(frame_base64, screenshot_path, error)) {
					 status->setPlainText(qstr_utf8("Current screen test\nerror=" + error));
					 return;
				 }
				 const WorkerActionResult start_result =
					 worker_manager_.test_detection_template_base64("start", frame_base64);
				 const WorkerActionResult end_result =
					 worker_manager_.test_detection_template_base64("end", frame_base64);
				 QString text = result_text("Current screen start test", start_result);
				 text.append(QString::fromUtf8("\n\n"));
				 text.append(result_text("Current screen end test", end_result));
				 text.append(QString::fromUtf8("\nscreenshot="));
				 text.append(qstr_utf8(screenshot_path));
				 status->setPlainText(text);
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

void PluginUiController::handle_automatic_detection_feed(const WorkerStatusSnapshot &snapshot)
{
	const PluginSettings settings = load_plugin_settings();
	if (!settings.automatic_detection_enabled) {
		return;
	}
	if (snapshot.state != WorkerDiagnosticState::running || !snapshot.recording_state_available) {
		return;
	}
	if (snapshot.recording_state.command_source == "manual" && snapshot.recording_state.state != "idle") {
		return;
	}

	const qint64 now = QDateTime::currentMSecsSinceEpoch();
	const int interval_ms = std::max(1000, settings.automatic_detection_interval_ms);
	if (last_detection_frame_sent_ms_ > 0 && now - last_detection_frame_sent_ms_ < interval_ms) {
		return;
	}
	last_detection_frame_sent_ms_ = now;

	std::string frame_base64;
	std::string screenshot_path;
	std::string error;
	if (!capture_current_screenshot_base64(frame_base64, screenshot_path, error, true)) {
		blog(LOG_WARNING, "%s detection_feed capture_failed error=%s", kLogPrefix, error.c_str());
		return;
	}

	const WorkerActionResult result = worker_manager_.send_detection_frame_base64(frame_base64);
	if (result.accepted()) {
		blog(LOG_INFO, "%s detection_feed sent interval_ms=%d", kLogPrefix, interval_ms);
		return;
	}
	blog(LOG_WARNING, "%s detection_feed failed status=%d http=%lu error=%s",
	     kLogPrefix, static_cast<int>(result.status), result.http_status, result.error.c_str());
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
