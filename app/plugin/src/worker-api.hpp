#pragma once

#include <cstdint>
#include <string>

namespace odr::plugin {

struct WorkerEndpoint {
	std::wstring host = L"127.0.0.1";
	uint16_t port = 8787;
};

enum class WorkerProbeStatus {
	reachable,
	unreachable,
	api_incompatible,
	runtime_root_mismatch,
	invalid_response,
};

struct WorkerProbeResult {
	WorkerProbeStatus status = WorkerProbeStatus::unreachable;
	unsigned long http_status = 0;
	std::string version;
	std::string api_version;
	std::string instance_id;
	std::string pid;
	std::string started_at;
	std::string user_data_dir;
	std::string error;
	std::string body;

	bool reusable() const
	{
		return status == WorkerProbeStatus::reachable;
	}
};

enum class OverlayFetchStatus {
	reachable,
	unavailable,
	invalid_response,
};

struct OverlayStatePayload {
	std::string deck_name;
	std::string sequence_number;
	std::string result;
	std::string opponent_deck;
	std::string recording_state;
};

struct OverlayFetchResult {
	OverlayFetchStatus status = OverlayFetchStatus::unavailable;
	unsigned long http_status = 0;
	OverlayStatePayload state;
	std::string error;
	std::string body;
};

enum class UploadStatusFetchStatus {
	reachable,
	unavailable,
	invalid_response,
};

struct UploadStatusResult {
	UploadStatusFetchStatus status = UploadStatusFetchStatus::unavailable;
	unsigned long http_status = 0;
	std::string error;
	std::string body;
	int ready_upload = 0;
	int uploading = 0;
	int uploaded = 0;
	int upload_failed = 0;
	int quota_waiting = 0;
	int need_manual_review = 0;
	int discarded = 0;

	bool reachable() const
	{
		return status == UploadStatusFetchStatus::reachable;
	}
};

enum class RecordingCommandStatus {
	accepted,
	unavailable,
	rejected,
	invalid_response,
};

enum class RecordingStateFetchStatus {
	reachable,
	unavailable,
	invalid_response,
};

enum class QueueActionFetchStatus {
	reachable,
	unavailable,
	invalid_response,
};

enum class QueueCommandStatus {
	accepted,
	unavailable,
	rejected,
	invalid_response,
};

enum class MatchFetchStatus {
	reachable,
	unavailable,
	not_found,
	invalid_response,
};

enum class MetadataUpdateStatus {
	accepted,
	unavailable,
	rejected,
	invalid_response,
};

enum class UploadMetadataPreviewStatus {
	reachable,
	unavailable,
	not_found,
	invalid_response,
};

struct RecordingStatePayload {
	std::string state;
	std::string session_id;
	std::string command_source;
	std::string last_action;
	std::string reason;
	std::string updated_at;
};

struct RecordingCommandResult {
	RecordingCommandStatus status = RecordingCommandStatus::unavailable;
	unsigned long http_status = 0;
	RecordingStatePayload state;
	std::string error;
	std::string body;

	bool accepted() const
	{
		return status == RecordingCommandStatus::accepted;
	}
};

struct RecordingStateFetchResult {
	RecordingStateFetchStatus status = RecordingStateFetchStatus::unavailable;
	unsigned long http_status = 0;
	RecordingStatePayload state;
	std::string error;
	std::string body;

	bool reachable() const
	{
		return status == RecordingStateFetchStatus::reachable;
	}
};

struct QueueActionItemPayload {
	int id = 0;
	std::string state;
	std::string video_path;
	std::string manual_review_reason;
	std::string last_error_code;
};

struct QueueActionFetchResult {
	QueueActionFetchStatus status = QueueActionFetchStatus::unavailable;
	unsigned long http_status = 0;
	QueueActionItemPayload item;
	std::string error;
	std::string body;

	bool reachable() const
	{
		return status == QueueActionFetchStatus::reachable;
	}
};

struct QueueCommandResult {
	QueueCommandStatus status = QueueCommandStatus::unavailable;
	unsigned long http_status = 0;
	QueueActionItemPayload item;
	std::string error;
	std::string body;

	bool accepted() const
	{
		return status == QueueCommandStatus::accepted;
	}
};

struct MatchMetadataPayload {
	int id = 0;
	std::string deck_name;
	std::string opponent_deck;
	std::string result;
	std::string rank;
	std::string dp;
	std::string memo;
};

struct MatchFetchResult {
	MatchFetchStatus status = MatchFetchStatus::unavailable;
	unsigned long http_status = 0;
	MatchMetadataPayload match;
	std::string error;
	std::string body;

	bool reachable() const
	{
		return status == MatchFetchStatus::reachable;
	}
};

struct MetadataUpdateResult {
	MetadataUpdateStatus status = MetadataUpdateStatus::unavailable;
	unsigned long http_status = 0;
	MatchMetadataPayload match;
	std::string error;
	std::string body;

	bool accepted() const
	{
		return status == MetadataUpdateStatus::accepted;
	}
};

struct UploadMetadataPreviewPayload {
	int match_id = 0;
	std::string title;
	std::string description;
	std::string notes;
	std::string warning;
};

struct UploadMetadataPreviewResult {
	UploadMetadataPreviewStatus status = UploadMetadataPreviewStatus::unavailable;
	unsigned long http_status = 0;
	UploadMetadataPreviewPayload preview;
	std::string error;
	std::string body;

	bool reachable() const
	{
		return status == UploadMetadataPreviewStatus::reachable;
	}
};

class LocalhostApiClient {
public:
	explicit LocalhostApiClient(std::string expected_api_version, std::string expected_worker_version);

	WorkerProbeResult probe_health(const WorkerEndpoint &endpoint, const std::wstring &expected_user_data_dir) const;
	OverlayFetchResult fetch_overlay_state(const WorkerEndpoint &endpoint) const;
	UploadStatusResult fetch_upload_status(const WorkerEndpoint &endpoint) const;
	RecordingStateFetchResult fetch_recording_state(const WorkerEndpoint &endpoint) const;
	QueueActionFetchResult fetch_queue_action_item(const WorkerEndpoint &endpoint) const;
	RecordingCommandResult send_recording_command(const WorkerEndpoint &endpoint, const std::string &action,
						      const std::string &source,
						      const std::string &video_path = {}) const;
	QueueCommandResult send_queue_command(const WorkerEndpoint &endpoint, int item_id, const std::string &action,
					      const std::string &youtube_video_id = {}) const;
	MatchFetchResult fetch_latest_match(const WorkerEndpoint &endpoint) const;
	MetadataUpdateResult update_match_metadata(const WorkerEndpoint &endpoint,
						   const MatchMetadataPayload &metadata) const;
	UploadMetadataPreviewResult fetch_upload_metadata_preview(const WorkerEndpoint &endpoint, int match_id) const;

private:
	std::string expected_api_version_;
	std::string expected_worker_version_;
};

std::string to_utf8(const std::wstring &value);
std::wstring to_wstring(uint16_t value);

} // namespace odr::plugin
