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

RecordingCommandResult LocalhostApiClient::send_recording_command(const WorkerEndpoint &endpoint,
								  const std::string &action,
								  const std::string &source) const
{
	RecordingCommandResult result;

#ifdef _WIN32
	const std::string body = "{\"action\":\"" + action + "\",\"source\":\"" + source + "\"}";
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

	result.state.state = extract_json_string(response.body, "state");
	result.state.session_id = extract_json_string(response.body, "session_id");
	result.state.command_source = extract_json_string(response.body, "command_source");
	result.state.last_action = extract_json_string(response.body, "last_action");
	result.state.reason = extract_json_string(response.body, "reason");
	result.state.updated_at = extract_json_string(response.body, "updated_at");

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

} // namespace odr::plugin
