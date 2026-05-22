#include "worker-launcher.hpp"

#include <obs-module.h>

#include <chrono>
#include <cwchar>
#include <utility>
#include <vector>

namespace odr::plugin {
namespace {

constexpr const char *kLogPrefix = "OBS Duel Recorder";
constexpr const char *kExpectedApiVersion = "0.3";
constexpr const char *kExpectedWorkerVersion = "0.3.0";

bool identity_changed(const WorkerProbeResult &baseline, const WorkerProbeResult &current)
{
	return (!baseline.instance_id.empty() && !current.instance_id.empty() && baseline.instance_id != current.instance_id) ||
	       (!baseline.pid.empty() && !current.pid.empty() && baseline.pid != current.pid) ||
	       (!baseline.started_at.empty() && !current.started_at.empty() && baseline.started_at != current.started_at);
}

#ifdef _WIN32

std::vector<wchar_t> build_environment_block(const std::wstring &user_data_dir)
{
	std::vector<wchar_t> block;
	LPWCH current_env = GetEnvironmentStringsW();
	if (!current_env) {
		return block;
	}

	for (LPWCH entry = current_env; *entry != L'\0'; entry += wcslen(entry) + 1) {
		const std::wstring value(entry);
		if (value.rfind(L"ODR_USER_DATA_DIR=", 0) == 0) {
			continue;
		}
		block.insert(block.end(), value.begin(), value.end());
		block.push_back(L'\0');
	}

	const std::wstring override_value = L"ODR_USER_DATA_DIR=" + user_data_dir;
	block.insert(block.end(), override_value.begin(), override_value.end());
	block.push_back(L'\0');
	block.push_back(L'\0');

	FreeEnvironmentStringsW(current_env);
	return block;
}

#endif

} // namespace

std::wstring read_env_wstring(const wchar_t *name)
{
#ifdef _WIN32
	const DWORD required = GetEnvironmentVariableW(name, nullptr, 0);
	if (required == 0) {
		return {};
	}
	std::wstring value(required, L'\0');
	const DWORD written = GetEnvironmentVariableW(name, value.data(), required);
	if (written == 0) {
		return {};
	}
	value.resize(written);
	return value;
#else
	(void)name;
	return {};
#endif
}

WorkerProcessManager::WorkerProcessManager()
	: api_client_(kExpectedApiVersion, kExpectedWorkerVersion)
{
}

WorkerProcessManager::~WorkerProcessManager()
{
	stop();
}

void WorkerProcessManager::start_async(WorkerLaunchConfig config)
{
	std::lock_guard<std::mutex> lock(mutex_);
	if (worker_thread_.joinable() || ownership_ != WorkerOwnership::none) {
		return;
	}

	stop_requested_ = false;
	worker_thread_ = std::thread([this, config = std::move(config)]() mutable {
		start(std::move(config));
	});
}

void WorkerProcessManager::start(WorkerLaunchConfig config)
{
	if (config.user_data_dir.empty()) {
		blog(LOG_WARNING, "%s config_error: ODR_USER_DATA_DIR is required before launching Worker", kLogPrefix);
		return;
	}

	blog(LOG_INFO, "%s worker preflight host=%s port=%u user_data_dir=%s",
	     kLogPrefix, to_utf8(config.endpoint.host).c_str(), config.endpoint.port,
	     to_utf8(config.user_data_dir).c_str());

	WorkerProbeResult preflight = api_client_.probe_health(config.endpoint, config.user_data_dir);
	if (preflight.reusable()) {
		{
			std::lock_guard<std::mutex> lock(mutex_);
			ownership_ = WorkerOwnership::reused_existing;
		}
		blog(LOG_INFO,
		     "%s worker reused existing instance api_version=%s version=%s instance_id=%s pid=%s started_at=%s user_data_dir=%s",
		     kLogPrefix, preflight.api_version.c_str(), preflight.version.c_str(),
		     preflight.instance_id.c_str(), preflight.pid.c_str(),
		     preflight.started_at.c_str(), preflight.user_data_dir.c_str());
		monitor_heartbeat(config, preflight);
		return;
	}

	if (preflight.status != WorkerProbeStatus::unreachable) {
		blog(LOG_WARNING,
		     "%s worker launch blocked status=%d http=%lu error=%s api_version=%s version=%s instance_id=%s user_data_dir=%s",
		     kLogPrefix, static_cast<int>(preflight.status), preflight.http_status,
		     preflight.error.c_str(), preflight.api_version.c_str(), preflight.version.c_str(),
		     preflight.instance_id.c_str(), preflight.user_data_dir.c_str());
		return;
	}

	if (!spawn_worker(config)) {
		return;
	}

	WorkerProbeResult ready_probe;
	if (!wait_until_ready(config, ready_probe)) {
		blog(LOG_WARNING, "%s worker startup timeout; terminating plugin-owned process", kLogPrefix);
#ifdef _WIN32
		std::lock_guard<std::mutex> lock(mutex_);
		if (ownership_ == WorkerOwnership::spawned_by_plugin && process_info_.hProcess) {
			TerminateProcess(process_info_.hProcess, 0);
			WaitForSingleObject(process_info_.hProcess, 3000);
			close_process_handles();
			ownership_ = WorkerOwnership::none;
		}
#endif
		return;
	}

	monitor_heartbeat(config, ready_probe);
}

bool WorkerProcessManager::spawn_worker(const WorkerLaunchConfig &config)
{
#ifdef _WIN32
	std::wstring command_line = L"\"" + config.command + L"\" --host " + config.endpoint.host +
				    L" --port " + to_wstring(config.endpoint.port);
	std::vector<wchar_t> mutable_command(command_line.begin(), command_line.end());
	mutable_command.push_back(L'\0');

	std::vector<wchar_t> environment = build_environment_block(config.user_data_dir);
	STARTUPINFOW startup_info{};
	startup_info.cb = sizeof(startup_info);

	PROCESS_INFORMATION process_info{};
	if (!CreateProcessW(nullptr, mutable_command.data(), nullptr, nullptr, FALSE,
			    CREATE_UNICODE_ENVIRONMENT | CREATE_NO_WINDOW,
			    environment.empty() ? nullptr : environment.data(), nullptr,
			    &startup_info, &process_info)) {
		blog(LOG_WARNING, "%s worker launch failed command=%s user_data_dir=%s error=%lu",
		     kLogPrefix, to_utf8(config.command).c_str(),
		     to_utf8(config.user_data_dir).c_str(), GetLastError());
		return false;
	}

	std::lock_guard<std::mutex> lock(mutex_);
	process_info_ = process_info;
	ownership_ = WorkerOwnership::spawned_by_plugin;
	blog(LOG_INFO, "%s worker spawned pid=%lu user_data_dir=%s",
	     kLogPrefix, process_info_.dwProcessId, to_utf8(config.user_data_dir).c_str());
	return true;
#else
	(void)config;
	blog(LOG_WARNING, "%s worker launch is only implemented on Windows", kLogPrefix);
	return false;
#endif
}

bool WorkerProcessManager::wait_until_ready(const WorkerLaunchConfig &config, WorkerProbeResult &ready_probe)
{
	const auto deadline = std::chrono::steady_clock::now() +
			      std::chrono::milliseconds(config.startup_timeout_ms);

	while (!stop_requested_ && std::chrono::steady_clock::now() < deadline) {
		WorkerProbeResult probe = api_client_.probe_health(config.endpoint, config.user_data_dir);
		if (probe.reusable()) {
			ready_probe = probe;
			blog(LOG_INFO,
			     "%s worker running api_version=%s version=%s instance_id=%s pid=%s started_at=%s user_data_dir=%s",
			     kLogPrefix, probe.api_version.c_str(), probe.version.c_str(),
			     probe.instance_id.c_str(), probe.pid.c_str(),
			     probe.started_at.c_str(), probe.user_data_dir.c_str());
			return true;
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(250));
	}

	return false;
}

void WorkerProcessManager::monitor_heartbeat(const WorkerLaunchConfig &config, const WorkerProbeResult &baseline)
{
	blog(LOG_INFO,
	     "%s heartbeat baseline interval_ms=%u failure_threshold=%u api_version=%s version=%s instance_id=%s pid=%s started_at=%s user_data_dir=%s",
	     kLogPrefix, config.heartbeat_interval_ms, config.heartbeat_failure_threshold,
	     baseline.api_version.c_str(), baseline.version.c_str(), baseline.instance_id.c_str(),
	     baseline.pid.c_str(), baseline.started_at.c_str(), baseline.user_data_dir.c_str());

	unsigned int consecutive_failures = 0;
	bool replacement_reported = false;
	bool timeout_reported = false;

	while (!stop_requested_) {
		std::this_thread::sleep_for(std::chrono::milliseconds(config.heartbeat_interval_ms));
		if (stop_requested_) {
			break;
		}

#ifdef _WIN32
		if (current_ownership() == WorkerOwnership::spawned_by_plugin) {
			std::lock_guard<std::mutex> lock(mutex_);
			if (process_info_.hProcess) {
				DWORD exit_code = 0;
				if (GetExitCodeProcess(process_info_.hProcess, &exit_code) && exit_code != STILL_ACTIVE) {
					blog(LOG_WARNING,
					     "%s heartbeat crashed exit_code=%lu user_data_dir=%s instance_id=%s pid=%s started_at=%s",
					     kLogPrefix, exit_code, to_utf8(config.user_data_dir).c_str(),
					     baseline.instance_id.c_str(), baseline.pid.c_str(), baseline.started_at.c_str());
					return;
				}
			}
		}
#endif

		WorkerProbeResult probe = api_client_.probe_health(config.endpoint, config.user_data_dir);
		if (probe.reusable()) {
			if (consecutive_failures > 0) {
				blog(LOG_INFO,
				     "%s heartbeat recovered failures=%u api_version=%s version=%s instance_id=%s pid=%s started_at=%s user_data_dir=%s",
				     kLogPrefix, consecutive_failures, probe.api_version.c_str(), probe.version.c_str(),
				     probe.instance_id.c_str(), probe.pid.c_str(), probe.started_at.c_str(), probe.user_data_dir.c_str());
			}
			consecutive_failures = 0;
			timeout_reported = false;

			if (!replacement_reported && identity_changed(baseline, probe)) {
				replacement_reported = true;
				blog(LOG_WARNING,
				     "%s heartbeat unexpected_process_change baseline_instance_id=%s current_instance_id=%s baseline_pid=%s current_pid=%s baseline_started_at=%s current_started_at=%s user_data_dir=%s",
				     kLogPrefix, baseline.instance_id.c_str(), probe.instance_id.c_str(),
				     baseline.pid.c_str(), probe.pid.c_str(), baseline.started_at.c_str(),
				     probe.started_at.c_str(), probe.user_data_dir.c_str());
			}
			continue;
		}

		++consecutive_failures;
		blog(LOG_WARNING,
		     "%s heartbeat probe_failed failures=%u status=%d http=%lu error=%s user_data_dir=%s instance_id=%s pid=%s started_at=%s",
		     kLogPrefix, consecutive_failures, static_cast<int>(probe.status), probe.http_status,
		     probe.error.c_str(), to_utf8(config.user_data_dir).c_str(), baseline.instance_id.c_str(),
		     baseline.pid.c_str(), baseline.started_at.c_str());

		if (!timeout_reported && consecutive_failures >= config.heartbeat_failure_threshold) {
			timeout_reported = true;
			blog(LOG_WARNING,
			     "%s heartbeat timeout threshold=%u host=%s port=%u user_data_dir=%s logs=<ODR_USER_DATA_DIR>/logs/",
			     kLogPrefix, config.heartbeat_failure_threshold, to_utf8(config.endpoint.host).c_str(),
			     config.endpoint.port, to_utf8(config.user_data_dir).c_str());
		}
	}
}

WorkerOwnership WorkerProcessManager::current_ownership() const
{
	std::lock_guard<std::mutex> lock(mutex_);
	return ownership_;
}

void WorkerProcessManager::stop()
{
	stop_requested_ = true;
	if (worker_thread_.joinable()) {
		worker_thread_.join();
	}

	std::lock_guard<std::mutex> lock(mutex_);
	if (ownership_ == WorkerOwnership::reused_existing) {
		blog(LOG_INFO, "%s worker reused existing instance; not stopping foreign/manual process", kLogPrefix);
		ownership_ = WorkerOwnership::none;
		return;
	}

#ifdef _WIN32
	if (ownership_ == WorkerOwnership::spawned_by_plugin && process_info_.hProcess) {
		DWORD exit_code = 0;
		if (GetExitCodeProcess(process_info_.hProcess, &exit_code) && exit_code == STILL_ACTIVE) {
			TerminateProcess(process_info_.hProcess, 0);
			WaitForSingleObject(process_info_.hProcess, 3000);
		}
		blog(LOG_INFO, "%s worker plugin-owned process stopped", kLogPrefix);
		close_process_handles();
	}
#endif

	ownership_ = WorkerOwnership::none;
}

void WorkerProcessManager::close_process_handles()
{
#ifdef _WIN32
	if (process_info_.hThread) {
		CloseHandle(process_info_.hThread);
		process_info_.hThread = nullptr;
	}
	if (process_info_.hProcess) {
		CloseHandle(process_info_.hProcess);
		process_info_.hProcess = nullptr;
	}
	process_info_.dwProcessId = 0;
	process_info_.dwThreadId = 0;
#endif
}

} // namespace odr::plugin
