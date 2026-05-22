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

class LocalhostApiClient {
public:
	explicit LocalhostApiClient(std::string expected_api_version, std::string expected_worker_version);

	WorkerProbeResult probe_health(const WorkerEndpoint &endpoint, const std::wstring &expected_user_data_dir) const;
	OverlayFetchResult fetch_overlay_state(const WorkerEndpoint &endpoint) const;

private:
	std::string expected_api_version_;
	std::string expected_worker_version_;
};

std::string to_utf8(const std::wstring &value);
std::wstring to_wstring(uint16_t value);

} // namespace odr::plugin
