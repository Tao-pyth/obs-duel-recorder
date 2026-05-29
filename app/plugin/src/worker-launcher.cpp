#include "worker-launcher.hpp"

#include <obs-module.h>

#include <chrono>
#include <cwchar>
#include <sstream>
#include <utility>
#include <vector>

namespace odr::plugin {
namespace {

constexpr const char *kLogPrefix = "OBS Duel Recorder";
constexpr const char *kExpectedApiVersion = "2.3";
constexpr const char *kExpectedWorkerVersion = "2.3.0";

bool identity_changed(const WorkerProbeResult &baseline, const WorkerProbeResult &current)
{
	return (!baseline.instance_id.empty() && !current.instance_id.empty() && baseline.instance_id != current.instance_id) ||
	       (!baseline.pid.empty() && !current.pid.empty() && baseline.pid != current.pid) ||
	       (!baseline.started_at.empty() && !current.started_at.empty() && baseline.started_at != current.started_at);
}

WorkerDiagnosticState diagnostic_state_for_probe(const WorkerProbeResult &probe)
{
	switch (probe.status) {
	case WorkerProbeStatus::reachable:
		return WorkerDiagnosticState::running;
	case WorkerProbeStatus::api_incompatible:
		return WorkerDiagnosticState::api_incompatible;
	case WorkerProbeStatus::runtime_root_mismatch:
		return WorkerDiagnosticState::runtime_dir_error;
	case WorkerProbeStatus::invalid_response:
		return WorkerDiagnosticState::unhealthy;
	case WorkerProbeStatus::unreachable:
		return WorkerDiagnosticState::starting;
	}
	return WorkerDiagnosticState::unhealthy;
}

std::string summarize_probe(const WorkerProbeResult &probe)
{
	return "status=" + std::to_string(static_cast<int>(probe.status)) +
	       " http=" + std::to_string(probe.http_status) +
	       " api_version=" + probe.api_version +
	       " version=" + probe.version +
	       " instance_id=" + probe.instance_id +
	       " pid=" + probe.pid +
	       " started_at=" + probe.started_at +
	       " user_data_dir=" + probe.user_data_dir;
}

#ifdef _WIN32

bool file_exists(const std::wstring &path)
{
	if (path.empty()) {
		return false;
	}
	const DWORD attributes = GetFileAttributesW(path.c_str());
	return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

bool directory_exists(const std::wstring &path)
{
	if (path.empty()) {
		return false;
	}
	const DWORD attributes = GetFileAttributesW(path.c_str());
	return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

std::wstring append_path(const std::wstring &directory, const wchar_t *name)
{
	if (directory.empty()) {
		return name;
	}
	const wchar_t last = directory.back();
	if (last == L'\\' || last == L'/') {
		return directory + name;
	}
	return directory + L"\\" + name;
}

bool ensure_directory_tree(const std::wstring &path, std::string &error)
{
	if (path.empty()) {
		error = "user_data_dir is empty";
		return false;
	}

	std::wstring current;
	for (size_t i = 0; i < path.size(); ++i) {
		current.push_back(path[i]);
		if (path[i] != L'\\' && path[i] != L'/') {
			continue;
		}
		if (current.size() <= 3) {
			continue;
		}
		if (directory_exists(current)) {
			continue;
		}
		if (!CreateDirectoryW(current.c_str(), nullptr) && !directory_exists(current)) {
			error = "Failed to create user_data_dir parent path=" + to_utf8(current) +
				" windows_error=" + std::to_string(GetLastError());
			return false;
		}
	}

	if (directory_exists(path)) {
		return true;
	}
	if (!CreateDirectoryW(path.c_str(), nullptr) && !directory_exists(path)) {
		error = "Failed to create user_data_dir path=" + to_utf8(path) +
			" windows_error=" + std::to_string(GetLastError());
		return false;
	}
	return true;
}

bool validate_user_data_dir(const std::wstring &path, std::string &error)
{
	if (!ensure_directory_tree(path, error)) {
		return false;
	}

	const std::wstring probe_path = append_path(path, L".odr-plugin-write-test");
	HANDLE file = CreateFileW(probe_path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
				  FILE_ATTRIBUTE_TEMPORARY, nullptr);
	if (file == INVALID_HANDLE_VALUE) {
		error = "user_data_dir is not writable path=" + to_utf8(path) +
			" windows_error=" + std::to_string(GetLastError());
		return false;
	}
	const char probe[] = "ok";
	DWORD written = 0;
	const bool wrote = WriteFile(file, probe, sizeof(probe) - 1, &written, nullptr) &&
			   written == sizeof(probe) - 1;
	CloseHandle(file);
	DeleteFileW(probe_path.c_str());
	if (!wrote) {
		error = "user_data_dir write probe failed path=" + to_utf8(path) +
			" windows_error=" + std::to_string(GetLastError());
		return false;
	}
	return true;
}

std::wstring parent_directory(const std::wstring &path)
{
	const size_t pos = path.find_last_of(L"\\/");
	if (pos == std::wstring::npos) {
		return {};
	}
	return path.substr(0, pos);
}

bool looks_like_path(const std::wstring &value)
{
	return value.find_first_of(L"\\/") != std::wstring::npos;
}

std::wstring worker_working_directory(const WorkerLaunchConfig &config)
{
	if (!looks_like_path(config.command)) {
		return {};
	}
	return parent_directory(config.command);
}

std::string sanitized_launch_summary(const WorkerLaunchConfig &config, const std::wstring &working_directory)
{
	std::ostringstream out;
	out << "command_path=" << to_utf8(config.command)
	    << " args=\"--host " << to_utf8(config.endpoint.host)
	    << " --port " << config.endpoint.port << "\""
	    << " working_dir="
	    << (working_directory.empty() ? "<inherit>" : to_utf8(working_directory))
	    << " user_data_dir=" << to_utf8(config.user_data_dir);
	return out.str();
}

std::string worker_discovery_summary(const WorkerLaunchConfig &config)
{
	std::ostringstream out;
	out << "command=" << to_utf8(config.command);
	if (!config.expected_worker_path.empty()) {
		out << " expected_worker=" << to_utf8(config.expected_worker_path);
		const bool expected_exists = file_exists(config.expected_worker_path);
		out << " expected_exists=" << (expected_exists ? "true" : "false");
		if (!expected_exists) {
			out << " action=copy_full_odr-worker_directory_to_"
			    << to_utf8(parent_directory(config.expected_worker_path));
		}
	}
	if (!config.wrong_nested_worker_path.empty()) {
		const bool wrong_exe_exists = file_exists(config.wrong_nested_worker_path);
		const bool wrong_dir_exists = directory_exists(parent_directory(parent_directory(config.wrong_nested_worker_path)));
		out << " wrong_nested_worker=" << to_utf8(config.wrong_nested_worker_path)
		    << " wrong_nested_exists=" << (wrong_exe_exists ? "true" : "false")
		    << " wrong_nested_dir_exists=" << (wrong_dir_exists ? "true" : "false");
		if (wrong_exe_exists || wrong_dir_exists) {
			out << " wrong_path_action=move_worker_out_of_obs-plugins_64bit_to_obs-plugins_worker";
		}
	}
	return out.str();
}

const char *launch_error_category(DWORD error)
{
	switch (error) {
	case ERROR_FILE_NOT_FOUND:
	case ERROR_PATH_NOT_FOUND:
		return "missing";
	case ERROR_ACCESS_DENIED:
		return "not_executable_or_access_denied";
	case ERROR_BAD_EXE_FORMAT:
		return "not_executable_or_wrong_architecture";
	default:
		return "failed_to_start";
	}
}

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
		update_status(WorkerDiagnosticState::config_error, config, WorkerOwnership::none,
			      "ODR_USER_DATA_DIR is required before launching Worker");
		blog(LOG_WARNING, "%s config_error: ODR_USER_DATA_DIR is required before launching Worker", kLogPrefix);
		return;
	}

#ifdef _WIN32
	std::string user_data_error;
	if (!validate_user_data_dir(config.user_data_dir, user_data_error)) {
		update_status(WorkerDiagnosticState::runtime_dir_error, config, WorkerOwnership::none, user_data_error);
		blog(LOG_WARNING, "%s runtime_dir_error: %s", kLogPrefix, user_data_error.c_str());
		return;
	}
#endif

	update_status(WorkerDiagnosticState::starting, config, WorkerOwnership::none, "probing Worker health");
	blog(LOG_INFO, "%s worker preflight host=%s port=%u user_data_dir=%s",
	     kLogPrefix, to_utf8(config.endpoint.host).c_str(), config.endpoint.port,
	     to_utf8(config.user_data_dir).c_str());

	WorkerProbeResult preflight = api_client_.probe_health(config.endpoint, config.user_data_dir);
	if (preflight.reusable()) {
		{
			std::lock_guard<std::mutex> lock(mutex_);
			ownership_ = WorkerOwnership::reused_existing;
		}
		update_status(WorkerDiagnosticState::running, config, WorkerOwnership::reused_existing, {}, &preflight);
		blog(LOG_INFO,
		     "%s worker reused existing instance api_version=%s version=%s instance_id=%s pid=%s started_at=%s user_data_dir=%s",
		     kLogPrefix, preflight.api_version.c_str(), preflight.version.c_str(),
		     preflight.instance_id.c_str(), preflight.pid.c_str(),
		     preflight.started_at.c_str(), preflight.user_data_dir.c_str());
		monitor_heartbeat(config, preflight);
		return;
	}

	if (preflight.status != WorkerProbeStatus::unreachable) {
		update_status(diagnostic_state_for_probe(preflight), config, WorkerOwnership::none, preflight.error, &preflight);
		blog(LOG_WARNING,
		     "%s worker launch blocked status=%d http=%lu error=%s api_version=%s version=%s instance_id=%s user_data_dir=%s",
		     kLogPrefix, static_cast<int>(preflight.status), preflight.http_status,
		     preflight.error.c_str(), preflight.api_version.c_str(), preflight.version.c_str(),
		     preflight.instance_id.c_str(), preflight.user_data_dir.c_str());
		return;
	}

	std::string launch_error;
	if (!spawn_worker(config, launch_error)) {
		update_status(WorkerDiagnosticState::config_error, config, WorkerOwnership::none, launch_error);
		blog(LOG_WARNING, "%s worker config_error: %s", kLogPrefix, launch_error.c_str());
		return;
	}
	update_status(WorkerDiagnosticState::starting, config, WorkerOwnership::spawned_by_plugin, "waiting for Worker readiness");

	WorkerProbeResult ready_probe;
	if (!wait_until_ready(config, ready_probe)) {
		const bool exited_before_ready =
			ready_probe.error.find("exited before readiness") != std::string::npos;
		const std::string startup_error =
			exited_before_ready ? ready_probe.error : "worker startup timeout";
		update_status(exited_before_ready ? WorkerDiagnosticState::crashed : WorkerDiagnosticState::unhealthy,
			      config, WorkerOwnership::spawned_by_plugin, startup_error, &ready_probe);
		blog(LOG_WARNING, "%s worker startup %s; terminating plugin-owned process",
		     kLogPrefix, exited_before_ready ? "exited_before_ready" : "timeout");
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

	update_status(WorkerDiagnosticState::running, config, WorkerOwnership::spawned_by_plugin, {}, &ready_probe);
	monitor_heartbeat(config, ready_probe);
}

bool WorkerProcessManager::spawn_worker(const WorkerLaunchConfig &config, std::string &launch_error)
{
#ifdef _WIN32
	const std::string discovery = worker_discovery_summary(config);
	const std::wstring working_directory = worker_working_directory(config);
	const std::string launch_summary = sanitized_launch_summary(config, working_directory);
	if (!config.expected_worker_path.empty() && !file_exists(config.expected_worker_path)) {
		blog(LOG_WARNING, "%s worker discovery packaged_worker_missing %s", kLogPrefix, discovery.c_str());
	}
	if (!config.wrong_nested_worker_path.empty() &&
	    (file_exists(config.wrong_nested_worker_path) ||
	     directory_exists(parent_directory(parent_directory(config.wrong_nested_worker_path))))) {
		blog(LOG_WARNING, "%s worker discovery wrong_nested_worker_detected %s", kLogPrefix, discovery.c_str());
	}
	if (!working_directory.empty() && !directory_exists(working_directory)) {
		launch_error = "Worker launch failed category=bad_working_directory " + launch_summary +
			       " " + discovery;
		blog(LOG_WARNING, "%s worker launch preflight_failed %s", kLogPrefix, launch_error.c_str());
		return false;
	}

	std::wstring command_line = L"\"" + config.command + L"\" --host " + config.endpoint.host +
				    L" --port " + to_wstring(config.endpoint.port);
	std::vector<wchar_t> mutable_command(command_line.begin(), command_line.end());
	mutable_command.push_back(L'\0');

	std::vector<wchar_t> environment = build_environment_block(config.user_data_dir);
	STARTUPINFOW startup_info{};
	startup_info.cb = sizeof(startup_info);

	PROCESS_INFORMATION process_info{};
	blog(LOG_INFO, "%s worker launch attempt %s", kLogPrefix, launch_summary.c_str());
	if (!CreateProcessW(nullptr, mutable_command.data(), nullptr, nullptr, FALSE,
			    CREATE_UNICODE_ENVIRONMENT | CREATE_NO_WINDOW,
			    environment.empty() ? nullptr : environment.data(),
			    working_directory.empty() ? nullptr : working_directory.c_str(),
			    &startup_info, &process_info)) {
		const DWORD error = GetLastError();
		launch_error = "Worker launch failed category=" + std::string(launch_error_category(error)) +
			       " windows_error=" + std::to_string(error) + " " +
			       launch_summary + " " + discovery;
		blog(LOG_WARNING, "%s worker launch failed %s", kLogPrefix, launch_error.c_str());
		return false;
	}

	std::lock_guard<std::mutex> lock(mutex_);
	process_info_ = process_info;
	ownership_ = WorkerOwnership::spawned_by_plugin;
	blog(LOG_INFO, "%s worker spawned pid=%lu %s",
	     kLogPrefix, process_info_.dwProcessId, launch_summary.c_str());
	return true;
#else
	(void)config;
	launch_error = "Worker launch is only implemented on Windows";
	blog(LOG_WARNING, "%s worker launch is only implemented on Windows", kLogPrefix);
	return false;
#endif
}

bool WorkerProcessManager::wait_until_ready(const WorkerLaunchConfig &config, WorkerProbeResult &ready_probe)
{
	const auto deadline = std::chrono::steady_clock::now() +
			      std::chrono::milliseconds(config.startup_timeout_ms);

	while (!stop_requested_ && std::chrono::steady_clock::now() < deadline) {
#ifdef _WIN32
		bool plugin_owned = false;
		HANDLE process_handle = nullptr;
		{
			std::lock_guard<std::mutex> lock(mutex_);
			plugin_owned = ownership_ == WorkerOwnership::spawned_by_plugin;
			process_handle = process_info_.hProcess;
		}
		if (plugin_owned && process_handle) {
			DWORD exit_code = 0;
			if (GetExitCodeProcess(process_handle, &exit_code) && exit_code != STILL_ACTIVE) {
				ready_probe.status = WorkerProbeStatus::unreachable;
				ready_probe.error = "Worker exited before readiness exit_code=" +
						    std::to_string(exit_code);
				update_status(WorkerDiagnosticState::crashed, config,
					      WorkerOwnership::spawned_by_plugin, ready_probe.error, &ready_probe);
				const std::wstring working_directory = worker_working_directory(config);
				const std::string launch_summary = sanitized_launch_summary(config, working_directory);
				blog(LOG_WARNING, "%s worker startup exited_before_ready exit_code=%lu %s",
				     kLogPrefix, exit_code, launch_summary.c_str());
				return false;
			}
		}
#endif

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
		ready_probe = probe;
		update_status(diagnostic_state_for_probe(probe), config, current_ownership(), probe.error, &probe);
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
					status_.state = WorkerDiagnosticState::crashed;
					status_.ownership = WorkerOwnership::spawned_by_plugin;
					status_.endpoint = config.endpoint;
					status_.user_data_dir = config.user_data_dir;
					status_.error = "Worker exited with code " + std::to_string(exit_code);
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
			OverlayFetchResult overlay = api_client_.fetch_overlay_state(config.endpoint);
			UploadStatusResult upload = api_client_.fetch_upload_status(config.endpoint);
			RecordingStateFetchResult recording = api_client_.fetch_recording_state(config.endpoint);
			QueueActionFetchResult queue_action = api_client_.fetch_queue_action_item(config.endpoint);
			update_status(WorkerDiagnosticState::running, config, current_ownership(), {}, &probe, 0, &overlay, &upload, &recording, &queue_action);

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
		update_status(WorkerDiagnosticState::unhealthy, config, current_ownership(), probe.error, &probe, consecutive_failures);
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

void WorkerProcessManager::update_status(WorkerDiagnosticState state, const WorkerLaunchConfig &config,
					 WorkerOwnership ownership, const std::string &error,
					 const WorkerProbeResult *probe, unsigned int consecutive_failures,
					 const OverlayFetchResult *overlay, const UploadStatusResult *upload,
					 const RecordingStateFetchResult *recording,
					 const QueueActionFetchResult *queue_action)
{
	std::lock_guard<std::mutex> lock(mutex_);
	status_.state = state;
	status_.ownership = ownership;
	status_.endpoint = config.endpoint;
	status_.user_data_dir = config.user_data_dir;
	status_.worker_command = config.command;
	status_.expected_worker_path = config.expected_worker_path;
	status_.wrong_nested_worker_path = config.wrong_nested_worker_path;
	status_.error = error;
	status_.consecutive_failures = consecutive_failures;
	if (probe) {
		status_.last_probe_summary = summarize_probe(*probe);
		status_.http_status = probe->http_status;
		status_.api_version = probe->api_version;
		status_.version = probe->version;
		status_.instance_id = probe->instance_id;
		status_.pid = probe->pid;
		status_.started_at = probe->started_at;
	}
	if (overlay) {
		status_.overlay_http_status = overlay->http_status;
		status_.overlay_error = overlay->error;
		status_.overlay_state_available = overlay->status == OverlayFetchStatus::reachable;
		if (status_.overlay_state_available) {
			status_.overlay_state = overlay->state;
		}
	}
	if (upload) {
		status_.upload_status = *upload;
		status_.upload_status_available = upload->status == UploadStatusFetchStatus::reachable;
	}
	if (recording) {
		status_.recording_state_available = recording->reachable();
		status_.recording_error = recording->reachable() ? std::string{} : recording->error;
		if (recording->reachable()) {
			status_.recording_state = recording->state;
		}
	}
	if (queue_action) {
		status_.queue_action_item = *queue_action;
		status_.queue_action_item_available = queue_action->reachable();
	}
}

WorkerStatusSnapshot WorkerProcessManager::status_snapshot() const
{
	std::lock_guard<std::mutex> lock(mutex_);
	return status_;
}

void WorkerProcessManager::record_recording_output_path(std::string output_path, std::string evidence)
{
	std::lock_guard<std::mutex> lock(mutex_);
	status_.recording_output_path = std::move(output_path);
	status_.recording_output_evidence = std::move(evidence);
}

void WorkerProcessManager::record_recording_output_evidence(std::string evidence)
{
	std::lock_guard<std::mutex> lock(mutex_);
	status_.recording_output_evidence = std::move(evidence);
}

RecordingCommandResult WorkerProcessManager::send_recording_command(const std::string &action, const std::string &source,
								     const std::string &video_path)
{
	WorkerEndpoint endpoint;
	{
		std::lock_guard<std::mutex> lock(mutex_);
		endpoint = status_.endpoint;
		if (status_.state != WorkerDiagnosticState::running) {
			RecordingCommandResult result;
			result.status = RecordingCommandStatus::unavailable;
			result.error = "Worker is not running";
			status_.recording_state_available = false;
			status_.recording_error = result.error;
			return result;
		}
	}
	RecordingCommandResult result = api_client_.send_recording_command(endpoint, action, source, video_path);
	{
		std::lock_guard<std::mutex> lock(mutex_);
		if (result.accepted()) {
			status_.recording_state_available = true;
			status_.recording_state = result.state;
			status_.recording_error.clear();
		} else {
			status_.recording_error = result.error.empty() ? "Recording command failed" : result.error;
		}
	}
	return result;
}

QueueCommandResult WorkerProcessManager::send_queue_command(int item_id, const std::string &action,
							    const std::string &youtube_video_id)
{
	WorkerEndpoint endpoint;
	{
		std::lock_guard<std::mutex> lock(mutex_);
		endpoint = status_.endpoint;
		if (status_.state != WorkerDiagnosticState::running) {
			QueueCommandResult result;
			result.status = QueueCommandStatus::unavailable;
			result.error = "Worker is not running";
			return result;
		}
	}
	QueueCommandResult result = api_client_.send_queue_command(endpoint, item_id, action, youtube_video_id);
	{
		std::lock_guard<std::mutex> lock(mutex_);
		if (result.accepted()) {
			status_.queue_action_item_available = false;
			status_.queue_action_item = QueueActionFetchResult{};
		} else {
			status_.queue_action_item.error = result.error.empty() ? "Queue command failed" : result.error;
		}
	}
	return result;
}

MatchFetchResult WorkerProcessManager::fetch_latest_match()
{
	WorkerEndpoint endpoint;
	{
		std::lock_guard<std::mutex> lock(mutex_);
		endpoint = status_.endpoint;
		if (status_.state != WorkerDiagnosticState::running) {
			MatchFetchResult result;
			result.status = MatchFetchStatus::unavailable;
			result.error = "Worker is not running";
			return result;
		}
	}
	return api_client_.fetch_latest_match(endpoint);
}

MetadataUpdateResult WorkerProcessManager::update_match_metadata(const MatchMetadataPayload &metadata)
{
	WorkerEndpoint endpoint;
	{
		std::lock_guard<std::mutex> lock(mutex_);
		endpoint = status_.endpoint;
		if (status_.state != WorkerDiagnosticState::running) {
			MetadataUpdateResult result;
			result.status = MetadataUpdateStatus::unavailable;
			result.error = "Worker is not running";
			return result;
		}
	}
	return api_client_.update_match_metadata(endpoint, metadata);
}

UploadMetadataPreviewResult WorkerProcessManager::fetch_upload_metadata_preview(int match_id)
{
	WorkerEndpoint endpoint;
	{
		std::lock_guard<std::mutex> lock(mutex_);
		endpoint = status_.endpoint;
		if (status_.state != WorkerDiagnosticState::running) {
			UploadMetadataPreviewResult result;
			result.status = UploadMetadataPreviewStatus::unavailable;
			result.error = "Worker is not running";
			return result;
		}
	}
	return api_client_.fetch_upload_metadata_preview(endpoint, match_id);
}

VideoPreviewResult WorkerProcessManager::fetch_match_video_preview(int match_id, int frame_index)
{
	WorkerEndpoint endpoint;
	{
		std::lock_guard<std::mutex> lock(mutex_);
		endpoint = status_.endpoint;
		if (status_.state != WorkerDiagnosticState::running) {
			VideoPreviewResult result;
			result.status = VideoPreviewStatus::unavailable;
			result.error = "Worker is not running";
			return result;
		}
	}
	return api_client_.fetch_match_video_preview(endpoint, match_id, frame_index);
}

WorkerActionResult WorkerProcessManager::fetch_setup_validation()
{
	WorkerEndpoint endpoint;
	{
		std::lock_guard<std::mutex> lock(mutex_);
		endpoint = status_.endpoint;
		if (status_.state != WorkerDiagnosticState::running) {
			WorkerActionResult result;
			result.status = WorkerActionStatus::unavailable;
			result.error = "Worker is not running";
			return result;
		}
	}
	return api_client_.fetch_setup_validation(endpoint);
}

WorkerActionResult WorkerProcessManager::register_detection_template(const std::string &kind,
								     const std::string &path,
								     double threshold,
								     int confirmations)
{
	WorkerEndpoint endpoint;
	{
		std::lock_guard<std::mutex> lock(mutex_);
		endpoint = status_.endpoint;
		if (status_.state != WorkerDiagnosticState::running) {
			WorkerActionResult result;
			result.status = WorkerActionStatus::unavailable;
			result.error = "Worker is not running";
			return result;
		}
	}
	return api_client_.register_detection_template(endpoint, kind, path, threshold, confirmations);
}

WorkerActionResult WorkerProcessManager::capture_detection_template(const std::string &kind,
								    const std::string &content_base64,
								    double threshold,
								    int confirmations)
{
	WorkerEndpoint endpoint;
	{
		std::lock_guard<std::mutex> lock(mutex_);
		endpoint = status_.endpoint;
		if (status_.state != WorkerDiagnosticState::running) {
			WorkerActionResult result;
			result.status = WorkerActionStatus::unavailable;
			result.error = "Worker is not running";
			return result;
		}
	}
	return api_client_.capture_detection_template(endpoint, kind, content_base64, threshold, confirmations);
}

WorkerActionResult WorkerProcessManager::test_detection_template(const std::string &kind,
								 const std::string &frame_text)
{
	WorkerEndpoint endpoint;
	{
		std::lock_guard<std::mutex> lock(mutex_);
		endpoint = status_.endpoint;
		if (status_.state != WorkerDiagnosticState::running) {
			WorkerActionResult result;
			result.status = WorkerActionStatus::unavailable;
			result.error = "Worker is not running";
			return result;
		}
	}
	return api_client_.test_detection_template(endpoint, kind, frame_text);
}

WorkerActionResult WorkerProcessManager::test_detection_template_base64(const std::string &kind,
									const std::string &frame_base64)
{
	WorkerEndpoint endpoint;
	{
		std::lock_guard<std::mutex> lock(mutex_);
		endpoint = status_.endpoint;
		if (status_.state != WorkerDiagnosticState::running) {
			WorkerActionResult result;
			result.status = WorkerActionStatus::unavailable;
			result.error = "Worker is not running";
			return result;
		}
	}
	return api_client_.test_detection_template_base64(endpoint, kind, frame_base64);
}

WorkerActionResult WorkerProcessManager::send_detection_frame_base64(const std::string &frame_base64)
{
	WorkerEndpoint endpoint;
	{
		std::lock_guard<std::mutex> lock(mutex_);
		endpoint = status_.endpoint;
		if (status_.state != WorkerDiagnosticState::running) {
			WorkerActionResult result;
			result.status = WorkerActionStatus::unavailable;
			result.error = "Worker is not running";
			return result;
		}
	}
	return api_client_.send_detection_frame_base64(endpoint, frame_base64);
}

OAuthAuthorizationUrlResult WorkerProcessManager::request_upload_oauth_authorization_url()
{
	WorkerEndpoint endpoint;
	{
		std::lock_guard<std::mutex> lock(mutex_);
		endpoint = status_.endpoint;
		if (status_.state != WorkerDiagnosticState::running) {
			OAuthAuthorizationUrlResult result;
			result.status = WorkerActionStatus::unavailable;
			result.error = "Worker is not running";
			return result;
		}
	}
	return api_client_.request_upload_oauth_authorization_url(endpoint);
}

WorkerActionResult WorkerProcessManager::refresh_upload_oauth_token()
{
	WorkerEndpoint endpoint;
	{
		std::lock_guard<std::mutex> lock(mutex_);
		endpoint = status_.endpoint;
		if (status_.state != WorkerDiagnosticState::running) {
			WorkerActionResult result;
			result.status = WorkerActionStatus::unavailable;
			result.error = "Worker is not running";
			return result;
		}
	}
	return api_client_.refresh_upload_oauth_token(endpoint);
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
		status_.state = WorkerDiagnosticState::not_started;
		status_.ownership = WorkerOwnership::none;
		status_.error = "Worker manager stopped; reused existing Worker left running";
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
	status_.state = WorkerDiagnosticState::not_started;
	status_.ownership = WorkerOwnership::none;
	status_.error = "Worker manager stopped";
	status_.consecutive_failures = 0;
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
