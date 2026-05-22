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
		std::lock_guard<std::mutex> lock(mutex_);
		ownership_ = WorkerOwnership::reused_existing;
		blog(LOG_INFO,
		     "%s worker reused existing instance api_version=%s version=%s instance_id=%s pid=%s started_at=%s user_data_dir=%s",
		     kLogPrefix, preflight.api_version.c_str(), preflight.version.c_str(),
		     preflight.instance_id.c_str(), preflight.pid.c_str(),
		     preflight.started_at.c_str(), preflight.user_data_dir.c_str());
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

	if (!wait_until_ready(config)) {
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
	}
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

bool WorkerProcessManager::wait_until_ready(const WorkerLaunchConfig &config)
{
	const auto deadline = std::chrono::steady_clock::now() +
			      std::chrono::milliseconds(config.startup_timeout_ms);

	while (!stop_requested_ && std::chrono::steady_clock::now() < deadline) {
		WorkerProbeResult probe = api_client_.probe_health(config.endpoint, config.user_data_dir);
		if (probe.reusable()) {
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
