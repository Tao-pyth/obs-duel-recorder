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

std::string extract_json_string(const std::string &body, const char *key)
{
	const std::regex pattern(std::string("\"") + key + "\"\\s*:\\s*\"([^\"]*)\"");
	std::smatch match;
	if (!std::regex_search(body, match, pattern) || match.size() < 2) {
		return {};
	}
	return match[1].str();
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

	WinHttpSetTimeouts(session, 1000, 1000, 1000, 1000);

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
			   "\",\"memo\":\"" + json_escape(metadata.memo) + "\"}";
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

} // namespace odr::plugin
