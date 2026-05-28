#include "worker-api.hpp"

#ifdef _WIN32
#include <windows.h>
#include <winhttp.h>
#endif

#include <algorithm>
#include <cctype>
#include <regex>
#include <sstream>
#include <utility>
#include <vector>

namespace odr::plugin {
namespace {

std::string json_unescape(const std::string &value)
{
	std::ostringstream out;
	for (size_t i = 0; i < value.size(); ++i) {
		if (value[i] != '\\' || i + 1 >= value.size()) {
			out << value[i];
			continue;
		}
		const char escaped = value[++i];
		switch (escaped) {
		case '"':
			out << '"';
			break;
		case '\\':
			out << '\\';
			break;
		case '/':
			out << '/';
			break;
		case 'b':
			out << '\b';
			break;
		case 'f':
			out << '\f';
			break;
		case 'n':
			out << '\n';
			break;
		case 'r':
			out << '\r';
			break;
		case 't':
			out << '\t';
			break;
		default:
			out << '\\' << escaped;
			break;
		}
	}
	return out.str();
}

std::string extract_json_string(const std::string &body, const char *key)
{
	const std::regex pattern(std::string("\"") + key + "\"\\s*:\\s*\"((?:\\\\.|[^\"])*)\"");
	std::smatch match;
	if (!std::regex_search(body, match, pattern) || match.size() < 2) {
		return {};
	}
	return json_unescape(match[1].str());
}

std::string extract_json_number(const std::string &body, const char *key)
{
	const std::regex pattern(std::string("\"") + key + "\"\\s*:\\s*([0-9]+)");
	std::smatch match;
	if (!std::regex_search(body, match, pattern) || match.size() < 2) {
		return {};
	}
	return match[1].str();
}

bool extract_json_bool(const std::string &body, const char *key)
{
	const std::regex pattern(std::string("\"") + key + "\"\\s*:\\s*(true|false)");
	std::smatch match;
	if (!std::regex_search(body, match, pattern) || match.size() < 2) {
		return false;
	}
	return match[1].str() == "true";
}

std::string extract_json_string_array(const std::string &body, const char *key)
{
	const std::regex pattern(std::string("\"") + key + "\"\\s*:\\s*\\[(.*?)\\]");
	std::smatch match;
	if (!std::regex_search(body, match, pattern) || match.size() < 2) {
		return {};
	}
	const std::string array_body = match[1].str();
	const std::regex item_pattern("\"((?:\\\\.|[^\"])*)\"");
	std::ostringstream joined;
	for (std::sregex_iterator it(array_body.begin(), array_body.end(), item_pattern), end; it != end; ++it) {
		if (joined.tellp() > 0) {
			joined << ", ";
		}
		joined << json_unescape((*it)[1].str());
	}
	return joined.str();
}

bool response_has_items(const std::string &body)
{
	const std::regex pattern("\"items\"\\s*:\\s*\\[\\s*\\{");
	return std::regex_search(body, pattern);
}

std::string normalize_path(std::string value)
{
	std::replace(value.begin(), value.end(), '\\', '/');
	while (value.size() > 1 && value.back() == '/') {
		value.pop_back();
	}
	std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
		return static_cast<char>(std::tolower(ch));
	});
	return value;
}

std::string json_escape(const std::string &value)
{
	std::ostringstream out;
	for (const unsigned char ch : value) {
		switch (ch) {
		case '"':
			out << "\\\"";
			break;
		case '\\':
			out << "\\\\";
			break;
		case '\b':
			out << "\\b";
			break;
		case '\f':
			out << "\\f";
			break;
		case '\n':
			out << "\\n";
			break;
		case '\r':
			out << "\\r";
			break;
		case '\t':
			out << "\\t";
			break;
		default:
			if (ch < 0x20) {
				out << "\\u00";
				const char *hex = "0123456789abcdef";
				out << hex[(ch >> 4) & 0x0f] << hex[ch & 0x0f];
			} else {
				out << static_cast<char>(ch);
			}
			break;
		}
	}
	return out.str();
}

void populate_recording_state(RecordingStatePayload &state, const std::string &body)
{
	state.state = extract_json_string(body, "state");
	state.session_id = extract_json_string(body, "session_id");
	state.command_source = extract_json_string(body, "command_source");
	state.last_action = extract_json_string(body, "last_action");
	state.reason = extract_json_string(body, "reason");
	state.updated_at = extract_json_string(body, "updated_at");
}

void populate_queue_item(QueueActionItemPayload &item, const std::string &body)
{
	const std::string id = extract_json_number(body, "id");
	item.id = id.empty() ? 0 : std::stoi(id);
	item.state = extract_json_string(body, "state");
	item.video_path = extract_json_string(body, "video_path");
	item.manual_review_reason = extract_json_string(body, "manual_review_reason");
	item.last_error_code = extract_json_string(body, "last_error_code");
}

void populate_match_metadata(MatchMetadataPayload &match, const std::string &body)
{
	const std::string id = extract_json_number(body, "id");
	match.id = id.empty() ? 0 : std::stoi(id);
	match.deck_name = extract_json_string(body, "deck_name");
	match.opponent_deck = extract_json_string(body, "opponent_deck");
	match.result = extract_json_string(body, "result");
	match.rank = extract_json_string(body, "rank");
	match.dp = extract_json_string(body, "dp");
	match.memo = extract_json_string(body, "memo");
	match.title_template = extract_json_string(body, "title_template");
	match.description_template = extract_json_string(body, "description_template");
	match.tags_template = extract_json_string(body, "tags_template");
}

void populate_upload_metadata_preview(UploadMetadataPreviewPayload &preview, const std::string &body)
{
	const std::string id = extract_json_number(body, "match_id");
	preview.match_id = id.empty() ? 0 : std::stoi(id);
	preview.title = extract_json_string(body, "title");
	preview.description = extract_json_string(body, "description");
	preview.tags = extract_json_string_array(body, "tags");
	preview.notes = extract_json_string(body, "notes");
	preview.privacy_status = extract_json_string(body, "privacy_status");
	preview.warning = extract_json_string(body, "warning");
}

void populate_video_preview(VideoPreviewPayload &preview, const std::string &body)
{
	const std::string frame_index = extract_json_number(body, "frame_index");
	const std::string frame_count = extract_json_number(body, "frame_count");
	preview.available = extract_json_bool(body, "available");
	preview.frame_index = frame_index.empty() ? 1 : std::stoi(frame_index);
	preview.frame_count = frame_count.empty() ? 3 : std::stoi(frame_count);
	preview.reason = extract_json_string(body, "reason");
	preview.video_path = extract_json_string(body, "video_path");
	preview.content_type = extract_json_string(body, "content_type");
	preview.content_base64 = extract_json_string(body, "content_base64");
}

#ifdef _WIN32

struct HttpResponse {
	bool ok = false;
	unsigned long status = 0;
	std::string body;
	std::string error;
};

HttpResponse http_request(const WorkerEndpoint &endpoint, const wchar_t *method, const wchar_t *path,
			  const std::string &request_body);

HttpResponse http_get(const WorkerEndpoint &endpoint, const wchar_t *path)
{
	return http_request(endpoint, L"GET", path, {});
}

HttpResponse http_post_json(const WorkerEndpoint &endpoint, const wchar_t *path, const std::string &body)
{
	return http_request(endpoint, L"POST", path, body);
}

HttpResponse http_put_json(const WorkerEndpoint &endpoint, const wchar_t *path, const std::string &body)
{
	return http_request(endpoint, L"PUT", path, body);
}

HttpResponse http_request(const WorkerEndpoint &endpoint, const wchar_t *method, const wchar_t *path,
			  const std::string &request_body)
{
	HttpResponse response;
	HINTERNET session = WinHttpOpen(L"OBS Duel Recorder Plugin/0.4",
				       WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
				       WINHTTP_NO_PROXY_NAME,
				       WINHTTP_NO_PROXY_BYPASS, 0);
	if (!session) {
		response.error = "WinHttpOpen failed";
		return response;
	}

	WinHttpSetTimeouts(session, 1000, 1000, 3000, 5000);

	HINTERNET connection = WinHttpConnect(session, endpoint.host.c_str(), endpoint.port, 0);
	if (!connection) {
		response.error = "WinHttpConnect failed";
		WinHttpCloseHandle(session);
		return response;
	}

	HINTERNET request = WinHttpOpenRequest(connection, method, path, nullptr,
					      WINHTTP_NO_REFERER,
					      WINHTTP_DEFAULT_ACCEPT_TYPES, 0);
	if (!request) {
		response.error = "WinHttpOpenRequest failed";
		WinHttpCloseHandle(connection);
		WinHttpCloseHandle(session);
		return response;
	}

	const wchar_t *headers = request_body.empty() ? WINHTTP_NO_ADDITIONAL_HEADERS : L"Content-Type: application/json";
	const DWORD headers_length = request_body.empty() ? 0 : static_cast<DWORD>(-1L);
	LPVOID body_data = request_body.empty() ? WINHTTP_NO_REQUEST_DATA :
					    const_cast<char *>(request_body.data());
	const DWORD body_size = static_cast<DWORD>(request_body.size());

	if (!WinHttpSendRequest(request, headers, headers_length,
				body_data, body_size, body_size, 0) ||
	    !WinHttpReceiveResponse(request, nullptr)) {
		response.error = "Worker API did not respond";
		WinHttpCloseHandle(request);
		WinHttpCloseHandle(connection);
		WinHttpCloseHandle(session);
		return response;
	}

	DWORD status = 0;
	DWORD status_size = sizeof(status);
	if (WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
				WINHTTP_HEADER_NAME_BY_INDEX, &status, &status_size,
				WINHTTP_NO_HEADER_INDEX)) {
		response.status = status;
	}

	for (;;) {
		DWORD available = 0;
		if (!WinHttpQueryDataAvailable(request, &available) || available == 0) {
			break;
		}

		std::vector<char> buffer(static_cast<size_t>(available));
		DWORD read = 0;
		if (!WinHttpReadData(request, buffer.data(), available, &read) || read == 0) {
			break;
		}
		response.body.append(buffer.data(), buffer.data() + read);
	}

	response.ok = true;
	WinHttpCloseHandle(request);
	WinHttpCloseHandle(connection);
	WinHttpCloseHandle(session);
	return response;
}

#endif

} // namespace

std::string to_utf8(const std::wstring &value)
{
#ifdef _WIN32
	if (value.empty()) {
		return {};
	}
	const int size = WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, nullptr, 0, nullptr, nullptr);
	if (size <= 0) {
		return {};
	}
	std::string result(static_cast<size_t>(size - 1), '\0');
	WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, result.data(), size, nullptr, nullptr);
	return result;
#else
	return std::string(value.begin(), value.end());
#endif
}

std::wstring to_wstring(uint16_t value)
{
	std::wstringstream stream;
	stream << value;
	return stream.str();
}

LocalhostApiClient::LocalhostApiClient(std::string expected_api_version, std::string expected_worker_version)
	: expected_api_version_(std::move(expected_api_version)),
	  expected_worker_version_(std::move(expected_worker_version))
{
}

WorkerProbeResult LocalhostApiClient::probe_health(const WorkerEndpoint &endpoint,
						   const std::wstring &expected_user_data_dir) const
{
	WorkerProbeResult result;

#ifdef _WIN32
	const HttpResponse response = http_get(endpoint, L"/health");
	result.http_status = response.status;
	result.body = response.body;

	if (!response.ok) {
		result.status = WorkerProbeStatus::unreachable;
		result.error = response.error;
		return result;
	}

	if (response.status != 200) {
		result.status = WorkerProbeStatus::invalid_response;
		result.error = "GET /health returned non-200 status";
		return result;
	}

	result.version = extract_json_string(response.body, "version");
	result.api_version = extract_json_string(response.body, "api_version");
	result.instance_id = extract_json_string(response.body, "instance_id");
	result.pid = extract_json_number(response.body, "pid");
	result.started_at = extract_json_string(response.body, "started_at");
	result.user_data_dir = extract_json_string(response.body, "user_data_dir");

	if (result.api_version.empty()) {
		result.status = WorkerProbeStatus::invalid_response;
		result.error = "GET /health response is missing api_version";
		return result;
	}

	if (result.api_version != expected_api_version_) {
		result.status = WorkerProbeStatus::api_incompatible;
		result.error = "Worker API version is incompatible";
		return result;
	}

	const std::string expected_root = normalize_path(to_utf8(expected_user_data_dir));
	const std::string observed_root = normalize_path(result.user_data_dir);
	if (observed_root.empty() || observed_root != expected_root) {
		result.status = WorkerProbeStatus::runtime_root_mismatch;
		result.error = "Worker runtime root does not match ODR_USER_DATA_DIR";
		return result;
	}

	if (!expected_worker_version_.empty() && !result.version.empty() &&
	    result.version != expected_worker_version_) {
		result.status = WorkerProbeStatus::api_incompatible;
		result.error = "Worker version requires restart or manual confirmation";
		return result;
	}

	result.status = WorkerProbeStatus::reachable;
	return result;
#else
	result.status = WorkerProbeStatus::unreachable;
	result.error = "Worker probing is only implemented on Windows";
	return result;
#endif
}

OverlayFetchResult LocalhostApiClient::fetch_overlay_state(const WorkerEndpoint &endpoint) const
{
	OverlayFetchResult result;

#ifdef _WIN32
	const HttpResponse response = http_get(endpoint, L"/overlay/state");
	result.http_status = response.status;
	result.body = response.body;

	if (!response.ok) {
		result.status = OverlayFetchStatus::unavailable;
		result.error = response.error;
		return result;
	}
	if (response.status != 200) {
		result.status = OverlayFetchStatus::invalid_response;
		result.error = "GET /overlay/state returned non-200 status";
		return result;
	}

	result.state.deck_name = extract_json_string(response.body, "deck_name");
	result.state.sequence_number = extract_json_string(response.body, "sequence_number");
	result.state.result = extract_json_string(response.body, "result");
	result.state.opponent_deck = extract_json_string(response.body, "opponent_deck");
	result.state.recording_state = extract_json_string(response.body, "recording_state");

	if (result.state.result.empty() || result.state.opponent_deck.empty() ||
	    result.state.recording_state.empty()) {
		result.status = OverlayFetchStatus::invalid_response;
		result.error = "GET /overlay/state response is missing required fields";
		return result;
	}

	result.status = OverlayFetchStatus::reachable;
	return result;
#else
	result.status = OverlayFetchStatus::unavailable;
	result.error = "Overlay state probing is only implemented on Windows";
	return result;
#endif
}

UploadStatusResult LocalhostApiClient::fetch_upload_status(const WorkerEndpoint &endpoint) const
{
	UploadStatusResult result;

#ifdef _WIN32
	const HttpResponse response = http_get(endpoint, L"/upload/status");
	result.http_status = response.status;
	if (!response.ok) {
		result.status = UploadStatusFetchStatus::unavailable;
		result.error = response.error;
		return result;
	}
	if (response.status != 200) {
		result.status = UploadStatusFetchStatus::invalid_response;
		result.error = "GET /upload/status returned non-200 status";
		return result;
	}

	result.ready_upload = std::stoi(extract_json_number(response.body, "ready_upload").empty() ?
					       "0" : extract_json_number(response.body, "ready_upload"));
	result.uploading = std::stoi(extract_json_number(response.body, "uploading").empty() ?
					     "0" : extract_json_number(response.body, "uploading"));
	result.uploaded = std::stoi(extract_json_number(response.body, "uploaded").empty() ?
					    "0" : extract_json_number(response.body, "uploaded"));
	result.upload_failed = std::stoi(extract_json_number(response.body, "upload_failed").empty() ?
						 "0" : extract_json_number(response.body, "upload_failed"));
	result.quota_waiting = std::stoi(extract_json_number(response.body, "quota_waiting").empty() ?
						 "0" : extract_json_number(response.body, "quota_waiting"));
	result.need_manual_review = std::stoi(extract_json_number(response.body, "need_manual_review").empty() ?
						      "0" : extract_json_number(response.body, "need_manual_review"));
	result.discarded = std::stoi(extract_json_number(response.body, "discarded").empty() ?
					     "0" : extract_json_number(response.body, "discarded"));
	result.readiness_state = extract_json_string(response.body, "readiness_state");
	result.readiness_next_action = extract_json_string(response.body, "readiness_next_action");
	result.status = UploadStatusFetchStatus::reachable;
	result.body = response.body;
	return result;
#else
	result.status = UploadStatusFetchStatus::unavailable;
	result.error = "Upload status probing is only implemented on Windows";
	return result;
#endif
}

RecordingStateFetchResult LocalhostApiClient::fetch_recording_state(const WorkerEndpoint &endpoint) const
{
	RecordingStateFetchResult result;

#ifdef _WIN32
	const HttpResponse response = http_get(endpoint, L"/recording/state");
	result.http_status = response.status;
	result.body = response.body;

	if (!response.ok) {
		result.status = RecordingStateFetchStatus::unavailable;
		result.error = response.error;
		return result;
	}
	if (response.status != 200) {
		result.status = RecordingStateFetchStatus::invalid_response;
		result.error = "GET /recording/state returned non-200 status";
		return result;
	}

	populate_recording_state(result.state, response.body);
	if (result.state.state.empty() || result.state.updated_at.empty()) {
		result.status = RecordingStateFetchStatus::invalid_response;
		result.error = "GET /recording/state response is missing required fields";
		return result;
	}

	result.status = RecordingStateFetchStatus::reachable;
	return result;
#else
	result.status = RecordingStateFetchStatus::unavailable;
	result.error = "Recording state probing is only implemented on Windows";
	return result;
#endif
}

QueueActionFetchResult LocalhostApiClient::fetch_queue_action_item(const WorkerEndpoint &endpoint) const
{
	QueueActionFetchResult result;

#ifdef _WIN32
	const wchar_t *paths[] = {
		L"/queue/items?state=need_manual_review",
		L"/queue/items?state=upload_failed",
		L"/queue/items?state=quota_waiting",
	};
	for (const wchar_t *path : paths) {
		const HttpResponse response = http_get(endpoint, path);
		result.http_status = response.status;
		result.body = response.body;
		if (!response.ok) {
			result.status = QueueActionFetchStatus::unavailable;
			result.error = response.error;
			return result;
		}
		if (response.status != 200) {
			result.status = QueueActionFetchStatus::invalid_response;
			result.error = "GET /queue/items returned non-200 status";
			return result;
		}
		if (!response_has_items(response.body)) {
			continue;
		}
		populate_queue_item(result.item, response.body);
		if (result.item.id <= 0 || result.item.state.empty()) {
			result.status = QueueActionFetchStatus::invalid_response;
			result.error = "GET /queue/items response is missing queue item fields";
			return result;
		}
		result.status = QueueActionFetchStatus::reachable;
		return result;
	}
	result.status = QueueActionFetchStatus::unavailable;
	result.error = "No failed or manual-review upload items";
	return result;
#else
	result.status = QueueActionFetchStatus::unavailable;
	result.error = "Queue item probing is only implemented on Windows";
	return result;
#endif
}

RecordingCommandResult LocalhostApiClient::send_recording_command(const WorkerEndpoint &endpoint,
								  const std::string &action,
								  const std::string &source,
								  const std::string &video_path) const
{
	RecordingCommandResult result;

#ifdef _WIN32
	std::string body = "{\"action\":\"" + json_escape(action) + "\",\"source\":\"" + json_escape(source) + "\"";
	if (!video_path.empty()) {
		body += ",\"video_path\":\"" + json_escape(video_path) + "\"";
	}
	body += "}";
	const HttpResponse response = http_post_json(endpoint, L"/recording/command", body);
	result.http_status = response.status;
	result.body = response.body;

	if (!response.ok) {
		result.status = RecordingCommandStatus::unavailable;
		result.error = response.error;
		return result;
	}

	if (response.status == 400 || response.status == 409) {
		result.status = RecordingCommandStatus::rejected;
		result.error = extract_json_string(response.body, "message");
		if (result.error.empty()) {
			result.error = "POST /recording/command rejected command";
		}
		return result;
	}

	if (response.status != 200) {
		result.status = RecordingCommandStatus::invalid_response;
		result.error = "POST /recording/command returned non-200 status";
		return result;
	}

	populate_recording_state(result.state, response.body);

	if (result.state.state.empty() || result.state.updated_at.empty()) {
		result.status = RecordingCommandStatus::invalid_response;
		result.error = "POST /recording/command response is missing required fields";
		return result;
	}

	result.status = RecordingCommandStatus::accepted;
	return result;
#else
	result.status = RecordingCommandStatus::unavailable;
	result.error = "Recording commands are only implemented on Windows";
	return result;
#endif
}

QueueCommandResult LocalhostApiClient::send_queue_command(const WorkerEndpoint &endpoint, int item_id,
							  const std::string &action,
							  const std::string &youtube_video_id) const
{
	QueueCommandResult result;

#ifdef _WIN32
	std::string body = "{\"action\":\"" + json_escape(action) + "\"";
	if (!youtube_video_id.empty()) {
		body += ",\"youtube_video_id\":\"" + json_escape(youtube_video_id) + "\"";
	}
	body += "}";
	const std::wstring path = L"/queue/items/" + std::to_wstring(item_id) + L"/command";
	const HttpResponse response = http_post_json(endpoint, path.c_str(), body);
	result.http_status = response.status;
	result.body = response.body;

	if (!response.ok) {
		result.status = QueueCommandStatus::unavailable;
		result.error = response.error;
		return result;
	}
	if (response.status == 400 || response.status == 404 || response.status == 409) {
		result.status = QueueCommandStatus::rejected;
		result.error = extract_json_string(response.body, "message");
		if (result.error.empty()) {
			result.error = "POST /queue/items/{id}/command rejected command";
		}
		return result;
	}
	if (response.status != 200) {
		result.status = QueueCommandStatus::invalid_response;
		result.error = "POST /queue/items/{id}/command returned non-200 status";
		return result;
	}

	populate_queue_item(result.item, response.body);
	if (result.item.id <= 0 || result.item.state.empty()) {
		result.status = QueueCommandStatus::invalid_response;
		result.error = "POST /queue/items/{id}/command response is missing queue item fields";
		return result;
	}
	result.status = QueueCommandStatus::accepted;
	return result;
#else
	result.status = QueueCommandStatus::unavailable;
	result.error = "Queue commands are only implemented on Windows";
	return result;
#endif
}

MatchFetchResult LocalhostApiClient::fetch_latest_match(const WorkerEndpoint &endpoint) const
{
	MatchFetchResult result;

#ifdef _WIN32
	const HttpResponse response = http_get(endpoint, L"/matches/latest");
	result.http_status = response.status;
	result.body = response.body;

	if (!response.ok) {
		result.status = MatchFetchStatus::unavailable;
		result.error = response.error;
		return result;
	}
	if (response.status == 404) {
		result.status = MatchFetchStatus::not_found;
		result.error = extract_json_string(response.body, "message");
		if (result.error.empty()) {
			result.error = "No match metadata is available";
		}
		return result;
	}
	if (response.status != 200) {
		result.status = MatchFetchStatus::invalid_response;
		result.error = "GET /matches/latest returned non-200 status";
		return result;
	}

	populate_match_metadata(result.match, response.body);
	if (result.match.id <= 0) {
		result.status = MatchFetchStatus::invalid_response;
		result.error = "GET /matches/latest response is missing match id";
		return result;
	}

	result.status = MatchFetchStatus::reachable;
	return result;
#else
	result.status = MatchFetchStatus::unavailable;
	result.error = "Match metadata fetching is only implemented on Windows";
	return result;
#endif
}

MetadataUpdateResult LocalhostApiClient::update_match_metadata(const WorkerEndpoint &endpoint,
							      const MatchMetadataPayload &metadata) const
{
	MetadataUpdateResult result;

#ifdef _WIN32
	if (metadata.id <= 0) {
		result.status = MetadataUpdateStatus::rejected;
		result.error = "Match id is required";
		return result;
	}

	std::string body = "{\"deck_name\":\"" + json_escape(metadata.deck_name) +
			   "\",\"opponent_deck\":\"" + json_escape(metadata.opponent_deck) +
			   "\",\"result\":\"" + json_escape(metadata.result) +
			   "\",\"rank\":\"" + json_escape(metadata.rank) +
			   "\",\"dp\":\"" + json_escape(metadata.dp) +
			   "\",\"memo\":\"" + json_escape(metadata.memo) +
			   "\",\"title_template\":\"" + json_escape(metadata.title_template) +
			   "\",\"description_template\":\"" + json_escape(metadata.description_template) +
			   "\",\"tags_template\":\"" + json_escape(metadata.tags_template) + "\"}";
	const std::wstring path = L"/matches/" + std::to_wstring(metadata.id) + L"/metadata";
	const HttpResponse response = http_put_json(endpoint, path.c_str(), body);
	result.http_status = response.status;
	result.body = response.body;

	if (!response.ok) {
		result.status = MetadataUpdateStatus::unavailable;
		result.error = response.error;
		return result;
	}
	if (response.status == 400 || response.status == 404) {
		result.status = MetadataUpdateStatus::rejected;
		result.error = extract_json_string(response.body, "message");
		if (result.error.empty()) {
			result.error = "PUT /matches/{id}/metadata rejected metadata";
		}
		return result;
	}
	if (response.status != 200) {
		result.status = MetadataUpdateStatus::invalid_response;
		result.error = "PUT /matches/{id}/metadata returned non-200 status";
		return result;
	}

	populate_match_metadata(result.match, response.body);
	if (result.match.id <= 0) {
		result.status = MetadataUpdateStatus::invalid_response;
		result.error = "PUT /matches/{id}/metadata response is missing match id";
		return result;
	}
	result.status = MetadataUpdateStatus::accepted;
	return result;
#else
	result.status = MetadataUpdateStatus::unavailable;
	result.error = "Match metadata updates are only implemented on Windows";
	return result;
#endif
}

UploadMetadataPreviewResult LocalhostApiClient::fetch_upload_metadata_preview(const WorkerEndpoint &endpoint,
									     int match_id) const
{
	UploadMetadataPreviewResult result;

#ifdef _WIN32
	if (match_id <= 0) {
		result.status = UploadMetadataPreviewStatus::not_found;
		result.error = "Match id is required";
		return result;
	}

	const std::wstring path = L"/matches/" + std::to_wstring(match_id) + L"/upload-metadata";
	const HttpResponse response = http_get(endpoint, path.c_str());
	result.http_status = response.status;
	result.body = response.body;

	if (!response.ok) {
		result.status = UploadMetadataPreviewStatus::unavailable;
		result.error = response.error;
		return result;
	}
	if (response.status == 404) {
		result.status = UploadMetadataPreviewStatus::not_found;
		result.error = extract_json_string(response.body, "message");
		if (result.error.empty()) {
			result.error = "Match metadata is not available";
		}
		return result;
	}
	if (response.status != 200) {
		result.status = UploadMetadataPreviewStatus::invalid_response;
		result.error = "GET /matches/{id}/upload-metadata returned non-200 status";
		return result;
	}

	populate_upload_metadata_preview(result.preview, response.body);
	if (result.preview.match_id <= 0 || result.preview.title.empty() || result.preview.description.empty()) {
		result.status = UploadMetadataPreviewStatus::invalid_response;
		result.error = "GET /matches/{id}/upload-metadata response is missing preview fields";
		return result;
	}
	result.status = UploadMetadataPreviewStatus::reachable;
	return result;
#else
	result.status = UploadMetadataPreviewStatus::unavailable;
	result.error = "Upload metadata preview is only implemented on Windows";
	return result;
#endif
}

VideoPreviewResult LocalhostApiClient::fetch_match_video_preview(const WorkerEndpoint &endpoint,
								int match_id,
								int frame_index) const
{
	VideoPreviewResult result;

#ifdef _WIN32
	if (match_id <= 0) {
		result.status = VideoPreviewStatus::not_found;
		result.error = "Match id is required";
		return result;
	}
	if (frame_index < 1) {
		frame_index = 1;
	}
	if (frame_index > 3) {
		frame_index = 3;
	}

	const std::wstring path = L"/matches/" + std::to_wstring(match_id) + L"/video-preview?frame=" +
				  std::to_wstring(frame_index);
	const HttpResponse response = http_get(endpoint, path.c_str());
	result.http_status = response.status;
	result.body = response.body;
	if (!response.ok) {
		result.status = VideoPreviewStatus::unavailable;
		result.error = response.error;
		return result;
	}
	if (response.status == 404) {
		result.status = VideoPreviewStatus::not_found;
		result.error = extract_json_string(response.body, "message");
		if (result.error.empty()) {
			result.error = "Match video preview is not available";
		}
		return result;
	}
	if (response.status != 200) {
		result.status = VideoPreviewStatus::invalid_response;
		result.error = "GET /matches/{id}/video-preview returned non-200 status";
		return result;
	}
	populate_video_preview(result.preview, response.body);
	result.status = VideoPreviewStatus::reachable;
	return result;
#else
	result.status = VideoPreviewStatus::unavailable;
	result.error = "Video preview is only implemented on Windows";
	return result;
#endif
}

WorkerActionResult LocalhostApiClient::fetch_setup_validation(const WorkerEndpoint &endpoint) const
{
	WorkerActionResult result;

#ifdef _WIN32
	const HttpResponse response = http_post_json(endpoint, L"/setup/validate", "{}");
	result.http_status = response.status;
	result.body = response.body;
	if (!response.ok) {
		result.status = WorkerActionStatus::unavailable;
		result.error = response.error;
		return result;
	}
	if (response.status != 200) {
		result.status = WorkerActionStatus::invalid_response;
		result.error = "POST /setup/validate returned non-200 status";
		return result;
	}
	result.status = WorkerActionStatus::accepted;
	return result;
#else
	result.status = WorkerActionStatus::unavailable;
	result.error = "Setup validation is only implemented on Windows";
	return result;
#endif
}

WorkerActionResult LocalhostApiClient::register_detection_template(const WorkerEndpoint &endpoint,
								   const std::string &kind,
								   const std::string &path,
								   double threshold,
								   int confirmations) const
{
	WorkerActionResult result;

#ifdef _WIN32
	std::ostringstream body;
	body << "{\"kind\":\"" << json_escape(kind)
	     << "\",\"path\":\"" << json_escape(path)
	     << "\",\"threshold\":" << threshold
	     << ",\"confirmations\":" << confirmations << "}";
	const HttpResponse response = http_post_json(endpoint, L"/setup/templates/register", body.str());
	result.http_status = response.status;
	result.body = response.body;
	if (!response.ok) {
		result.status = WorkerActionStatus::unavailable;
		result.error = response.error;
		return result;
	}
	if (response.status == 400 || response.status == 409) {
		result.status = WorkerActionStatus::rejected;
		result.error = extract_json_string(response.body, "message");
		if (result.error.empty()) {
			result.error = "POST /setup/templates/register rejected the template";
		}
		return result;
	}
	if (response.status != 200) {
		result.status = WorkerActionStatus::invalid_response;
		result.error = "POST /setup/templates/register returned non-200 status";
		return result;
	}
	result.status = WorkerActionStatus::accepted;
	return result;
#else
	result.status = WorkerActionStatus::unavailable;
	result.error = "Template registration is only implemented on Windows";
	return result;
#endif
}

WorkerActionResult LocalhostApiClient::capture_detection_template(const WorkerEndpoint &endpoint,
								  const std::string &kind,
								  const std::string &content_base64,
								  double threshold,
								  int confirmations) const
{
	WorkerActionResult result;

#ifdef _WIN32
	std::ostringstream body;
	body << "{\"kind\":\"" << json_escape(kind)
	     << "\",\"extension\":\"png\""
	     << ",\"content_base64\":\"" << json_escape(content_base64)
	     << "\",\"threshold\":" << threshold
	     << ",\"confirmations\":" << confirmations << "}";
	const HttpResponse response = http_post_json(endpoint, L"/setup/templates/capture", body.str());
	result.http_status = response.status;
	result.body = response.body;
	if (!response.ok) {
		result.status = WorkerActionStatus::unavailable;
		result.error = response.error;
		return result;
	}
	if (response.status == 400 || response.status == 409) {
		result.status = WorkerActionStatus::rejected;
		result.error = extract_json_string(response.body, "message");
		if (result.error.empty()) {
			result.error = "POST /setup/templates/capture rejected the captured template";
		}
		return result;
	}
	if (response.status != 200) {
		result.status = WorkerActionStatus::invalid_response;
		result.error = "POST /setup/templates/capture returned non-200 status";
		return result;
	}
	result.status = WorkerActionStatus::accepted;
	return result;
#else
	result.status = WorkerActionStatus::unavailable;
	result.error = "Template capture is only implemented on Windows";
	return result;
#endif
}

WorkerActionResult LocalhostApiClient::test_detection_template(const WorkerEndpoint &endpoint,
							      const std::string &kind,
							      const std::string &frame_text) const
{
	WorkerActionResult result;

#ifdef _WIN32
	const std::string body = "{\"kind\":\"" + json_escape(kind) + "\",\"frame_text\":\"" +
				 json_escape(frame_text) + "\"}";
	const HttpResponse response = http_post_json(endpoint, L"/detection/test", body);
	result.http_status = response.status;
	result.body = response.body;
	if (!response.ok) {
		result.status = WorkerActionStatus::unavailable;
		result.error = response.error;
		return result;
	}
	if (response.status == 400 || response.status == 409) {
		result.status = WorkerActionStatus::rejected;
		result.error = extract_json_string(response.body, "message");
		if (result.error.empty()) {
			result.error = "POST /detection/test rejected the test payload";
		}
		return result;
	}
	if (response.status != 200) {
		result.status = WorkerActionStatus::invalid_response;
		result.error = "POST /detection/test returned non-200 status";
		return result;
	}
	result.status = WorkerActionStatus::accepted;
	return result;
#else
	result.status = WorkerActionStatus::unavailable;
	result.error = "Detection tests are only implemented on Windows";
	return result;
#endif
}

WorkerActionResult LocalhostApiClient::test_detection_template_base64(const WorkerEndpoint &endpoint,
								      const std::string &kind,
								      const std::string &frame_base64) const
{
	WorkerActionResult result;

#ifdef _WIN32
	const std::string body = "{\"kind\":\"" + json_escape(kind) + "\",\"frame_base64\":\"" +
				 json_escape(frame_base64) + "\"}";
	const HttpResponse response = http_post_json(endpoint, L"/detection/test", body);
	result.http_status = response.status;
	result.body = response.body;
	if (!response.ok) {
		result.status = WorkerActionStatus::unavailable;
		result.error = response.error;
		return result;
	}
	if (response.status == 400 || response.status == 409) {
		result.status = WorkerActionStatus::rejected;
		result.error = extract_json_string(response.body, "message");
		if (result.error.empty()) {
			result.error = "POST /detection/test rejected the captured frame";
		}
		return result;
	}
	if (response.status != 200) {
		result.status = WorkerActionStatus::invalid_response;
		result.error = "POST /detection/test returned non-200 status";
		return result;
	}
	result.status = WorkerActionStatus::accepted;
	return result;
#else
	result.status = WorkerActionStatus::unavailable;
	result.error = "Detection tests are only implemented on Windows";
	return result;
#endif
}

WorkerActionResult LocalhostApiClient::send_detection_frame_base64(const WorkerEndpoint &endpoint,
								   const std::string &frame_base64) const
{
	WorkerActionResult result;

#ifdef _WIN32
	const std::string body = "{\"frame_base64\":\"" + json_escape(frame_base64) + "\"}";
	const HttpResponse response = http_post_json(endpoint, L"/detection/frame", body);
	result.http_status = response.status;
	result.body = response.body;
	if (!response.ok) {
		result.status = WorkerActionStatus::unavailable;
		result.error = response.error;
		return result;
	}
	if (response.status == 400 || response.status == 409 || response.status == 503) {
		result.status = WorkerActionStatus::rejected;
		result.error = extract_json_string(response.body, "message");
		if (result.error.empty()) {
			result.error = "POST /detection/frame rejected the captured frame";
		}
		return result;
	}
	if (response.status != 200) {
		result.status = WorkerActionStatus::invalid_response;
		result.error = "POST /detection/frame returned non-200 status";
		return result;
	}
	result.status = WorkerActionStatus::accepted;
	return result;
#else
	result.status = WorkerActionStatus::unavailable;
	result.error = "Detection frame feed is only implemented on Windows";
	return result;
#endif
}

OAuthAuthorizationUrlResult LocalhostApiClient::request_upload_oauth_authorization_url(const WorkerEndpoint &endpoint) const
{
	OAuthAuthorizationUrlResult result;

#ifdef _WIN32
	const HttpResponse response = http_post_json(endpoint, L"/upload/oauth/authorization-url", "{}");
	result.http_status = response.status;
	result.body = response.body;
	if (!response.ok) {
		result.status = WorkerActionStatus::unavailable;
		result.error = response.error;
		return result;
	}
	if (response.status == 400 || response.status == 409) {
		result.status = WorkerActionStatus::rejected;
		result.error = extract_json_string(response.body, "message");
		if (result.error.empty()) {
			result.error = "POST /upload/oauth/authorization-url rejected OAuth setup";
		}
		return result;
	}
	if (response.status != 200) {
		result.status = WorkerActionStatus::invalid_response;
		result.error = "POST /upload/oauth/authorization-url returned non-200 status";
		return result;
	}
	result.authorization_url = extract_json_string(response.body, "authorization_url");
	result.redirect_uri = extract_json_string(response.body, "redirect_uri");
	if (result.authorization_url.empty()) {
		result.status = WorkerActionStatus::invalid_response;
		result.error = "POST /upload/oauth/authorization-url response is missing authorization_url";
		return result;
	}
	result.status = WorkerActionStatus::accepted;
	return result;
#else
	result.status = WorkerActionStatus::unavailable;
	result.error = "OAuth authorization is only implemented on Windows";
	return result;
#endif
}

WorkerActionResult LocalhostApiClient::refresh_upload_oauth_token(const WorkerEndpoint &endpoint) const
{
	WorkerActionResult result;

#ifdef _WIN32
	const HttpResponse response = http_post_json(endpoint, L"/upload/oauth/refresh", "{}");
	result.http_status = response.status;
	result.body = response.body;
	if (!response.ok) {
		result.status = WorkerActionStatus::unavailable;
		result.error = response.error;
		return result;
	}
	if (response.status == 400 || response.status == 409) {
		result.status = WorkerActionStatus::rejected;
		result.error = extract_json_string(response.body, "message");
		if (result.error.empty()) {
			result.error = "POST /upload/oauth/refresh rejected token refresh";
		}
		return result;
	}
	if (response.status != 200) {
		result.status = WorkerActionStatus::invalid_response;
		result.error = "POST /upload/oauth/refresh returned non-200 status";
		return result;
	}
	result.status = WorkerActionStatus::accepted;
	return result;
#else
	result.status = WorkerActionStatus::unavailable;
	result.error = "OAuth refresh is only implemented on Windows";
	return result;
#endif
}

} // namespace odr::plugin
