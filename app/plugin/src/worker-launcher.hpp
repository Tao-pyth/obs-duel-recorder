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

struct WorkerLaunchConfig {
	WorkerEndpoint endpoint;
	std::wstring user_data_dir;
	std::wstring command = L"odr-worker";
	unsigned int startup_timeout_ms = 10000;
};

class WorkerProcessManager {
public:
	WorkerProcessManager();
	~WorkerProcessManager();

	void start_async(WorkerLaunchConfig config);
	void stop();

private:
	void start(WorkerLaunchConfig config);
	bool spawn_worker(const WorkerLaunchConfig &config);
	bool wait_until_ready(const WorkerLaunchConfig &config);
	void close_process_handles();

	LocalhostApiClient api_client_;
	std::mutex mutex_;
	std::thread worker_thread_;
	std::atomic_bool stop_requested_{false};
	WorkerOwnership ownership_ = WorkerOwnership::none;

#ifdef _WIN32
	PROCESS_INFORMATION process_info_{};
#endif
};

std::wstring read_env_wstring(const wchar_t *name);

} // namespace odr::plugin
