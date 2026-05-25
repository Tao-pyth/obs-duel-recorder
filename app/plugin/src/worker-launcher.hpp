#pragma once

#include "worker-api.hpp"

#include <atomic>
#include <mutex>
#include <string>
#include <thread>

#ifdef _WIN32
#include <windows.h>
#endif

namespace odr::plugin {

enum class WorkerOwnership {
	none,
	spawned_by_plugin,
	reused_existing,
};

enum class WorkerDiagnosticState {
	not_started,
	starting,
	running,
	unhealthy,
	config_error,
	runtime_dir_error,
	api_incompatible,
	crashed,
};

struct WorkerLaunchConfig {
	WorkerEndpoint endpoint;
	std::wstring user_data_dir;
	std::wstring command = L"odr-worker";
	std::wstring expected_worker_path;
	std::wstring wrong_nested_worker_path;
	unsigned int startup_timeout_ms = 10000;
	unsigned int heartbeat_interval_ms = 2000;
	unsigned int heartbeat_failure_threshold = 3;
};

struct WorkerStatusSnapshot {
	WorkerDiagnosticState state = WorkerDiagnosticState::not_started;
	WorkerOwnership ownership = WorkerOwnership::none;
	WorkerEndpoint endpoint;
	OverlayStatePayload overlay_state;
	std::wstring user_data_dir;
	std::wstring worker_command;
	std::wstring expected_worker_path;
	std::wstring wrong_nested_worker_path;
	RecordingStatePayload recording_state;
	std::string last_probe_summary;
	std::string error;
	std::string recording_error;
	std::string recording_output_path;
	std::string recording_output_evidence;
	std::string overlay_error;
	std::string api_version;
	std::string version;
	std::string instance_id;
	std::string pid;
	std::string started_at;
	unsigned long http_status = 0;
	unsigned long overlay_http_status = 0;
	unsigned int consecutive_failures = 0;
	bool recording_state_available = false;
	bool overlay_state_available = false;
};

class WorkerProcessManager {
public:
	WorkerProcessManager();
	~WorkerProcessManager();

	void start_async(WorkerLaunchConfig config);
	void stop();
	WorkerStatusSnapshot status_snapshot() const;
	void record_recording_output_path(std::string output_path, std::string evidence);
	void record_recording_output_evidence(std::string evidence);
	RecordingCommandResult send_recording_command(const std::string &action, const std::string &source,
						      const std::string &video_path = {});

private:
	void start(WorkerLaunchConfig config);
	bool spawn_worker(const WorkerLaunchConfig &config, std::string &launch_error);
	bool wait_until_ready(const WorkerLaunchConfig &config, WorkerProbeResult &ready_probe);
	void monitor_heartbeat(const WorkerLaunchConfig &config, const WorkerProbeResult &baseline);
	WorkerOwnership current_ownership() const;
	void update_status(WorkerDiagnosticState state, const WorkerLaunchConfig &config,
			   WorkerOwnership ownership, const std::string &error,
			   const WorkerProbeResult *probe = nullptr, unsigned int consecutive_failures = 0,
			   const OverlayFetchResult *overlay = nullptr);
	void close_process_handles();

	LocalhostApiClient api_client_;
	mutable std::mutex mutex_;
	std::thread worker_thread_;
	std::atomic_bool stop_requested_{false};
	WorkerOwnership ownership_ = WorkerOwnership::none;
	WorkerStatusSnapshot status_;

#ifdef _WIN32
	PROCESS_INFORMATION process_info_{};
#endif
};

std::wstring read_env_wstring(const wchar_t *name);

} // namespace odr::plugin
