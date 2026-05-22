#include "overlay-sources.hpp"

#include <obs-frontend-api.h>
#include <obs-module.h>

#include <array>
#include <cstring>
#include <string>

namespace odr::plugin {
namespace {

constexpr const char *kLogPrefix = "OBS Duel Recorder";

struct OverlayField {
	const char *key;
	OverlayFieldSettings settings;
};

struct SourceNameCount {
	std::string name;
	size_t count = 0;
};

bool enum_source_name_count(void *param, obs_source_t *source)
{
	auto *context = static_cast<SourceNameCount *>(param);
	const char *name = obs_source_get_name(source);
	if (name && context->name == name) {
		++context->count;
	}
	return true;
}

bool starts_with(const char *value, const char *prefix)
{
	return value && std::strncmp(value, prefix, std::strlen(prefix)) == 0;
}

bool is_supported_text_source_id(const char *id)
{
	return starts_with(id, "text_gdiplus") || starts_with(id, "text_ft2_source");
}

bool is_supported_text_source(obs_source_t *source)
{
	const char *id = obs_source_get_id(source);
	const char *unversioned_id = obs_source_get_unversioned_id(source);
	return is_supported_text_source_id(id) || is_supported_text_source_id(unversioned_id);
}

std::string find_available_text_source_kind()
{
	const char *id = nullptr;
	const char *unversioned_id = nullptr;
	for (size_t index = 0; obs_enum_input_types2(index, &id, &unversioned_id); ++index) {
		if (is_supported_text_source_id(id)) {
			return id;
		}
		if (is_supported_text_source_id(unversioned_id) && id) {
			return id;
		}
	}
	for (size_t index = 0; obs_enum_input_types(index, &id); ++index) {
		if (is_supported_text_source_id(id)) {
			return id;
		}
	}
	return {};
}

void add_diagnostic(OverlaySourceEnsureResult &result, const char *code, const OverlayField &field, const char *action,
		    const char *reason)
{
	result.success = false;
	result.diagnostics.push_back(
		OverlaySourceDiagnostic{code, field.key, field.settings.source_name, action, reason});
}

bool add_to_current_scene(obs_source_t *source)
{
	obs_source_t *scene_source = obs_frontend_get_current_scene();
	if (!scene_source) {
		return false;
	}
	obs_scene_t *scene = obs_scene_from_source(scene_source);
	if (!scene) {
		obs_source_release(scene_source);
		return false;
	}
	obs_sceneitem_t *item = obs_scene_add(scene, source);
	if (item) {
		obs_sceneitem_release(item);
	}
	obs_source_release(scene_source);
	return item != nullptr;
}

void create_source(OverlaySourceEnsureResult &result, const OverlayField &field, const std::string &source_kind)
{
	obs_data_t *data = obs_data_create();
	obs_data_set_string(data, "text", field.settings.default_text.c_str());
	obs_source_t *source = obs_source_create(source_kind.c_str(), field.settings.source_name.c_str(), data, nullptr);
	obs_data_release(data);

	if (!source) {
		add_diagnostic(result, "overlay_update_failed", field, "create", "obs_source_create_failed");
		return;
	}
	if (!add_to_current_scene(source)) {
		obs_source_release(source);
		add_diagnostic(result, "overlay_update_failed", field, "create", "current_scene_add_failed");
		return;
	}

	blog(LOG_INFO, "%s overlay source ready field=%s source=%s action=create kind=%s", kLogPrefix, field.key,
	     field.settings.source_name.c_str(), source_kind.c_str());
	obs_source_release(source);
}

void ensure_field(OverlaySourceEnsureResult &result, const OverlayField &field, bool auto_create_sources)
{
	SourceNameCount count{field.settings.source_name};
	obs_enum_sources(enum_source_name_count, &count);
	if (count.count > 1) {
		add_diagnostic(result, "overlay_source_duplicate", field, "skip", "duplicate_source_name");
		return;
	}

	obs_source_t *source = obs_get_source_by_name(field.settings.source_name.c_str());
	if (source) {
		const bool supported = is_supported_text_source(source);
		const char *id = obs_source_get_id(source);
		if (supported) {
			blog(LOG_INFO, "%s overlay source ready field=%s source=%s action=reuse kind=%s", kLogPrefix,
			     field.key, field.settings.source_name.c_str(), id ? id : "unknown");
		} else {
			add_diagnostic(result, "overlay_source_unsupported", field, "skip",
				       id ? id : "unknown_source_kind");
		}
		obs_source_release(source);
		return;
	}

	if (!auto_create_sources) {
		add_diagnostic(result, "overlay_source_missing", field, "skip", "source_not_found");
	}
}

std::array<OverlayField, 5> fields_from_settings(const OverlaySettings &settings)
{
	return {{{"deck_name", settings.deck_name},
		 {"sequence_number", settings.sequence_number},
		 {"result", settings.result},
		 {"opponent_deck", settings.opponent_deck},
		 {"recording_state", settings.recording_state}}};
}

} // namespace

OverlaySourceEnsureResult ensure_overlay_text_sources(const OverlaySettings &settings)
{
	OverlaySourceEnsureResult result;
	if (!settings.enabled) {
		blog(LOG_INFO, "%s overlay sources skipped reason=disabled", kLogPrefix);
		return result;
	}

	const std::array<OverlayField, 5> fields = fields_from_settings(settings);
	for (const OverlayField &field : fields) {
		if (field.settings.source_name.empty()) {
			add_diagnostic(result, "overlay_settings_invalid", field, "skip", "empty_source_name");
			continue;
		}
		ensure_field(result, field, settings.auto_create_sources);
	}

	if (!settings.auto_create_sources) {
		return result;
	}

	const std::string source_kind = find_available_text_source_kind();
	if (source_kind.empty()) {
		for (const OverlayField &field : fields) {
			if (field.settings.source_name.empty()) {
				continue;
			}
			obs_source_t *source = obs_get_source_by_name(field.settings.source_name.c_str());
			if (!source) {
				add_diagnostic(result, "overlay_text_source_unavailable", field, "create",
					       "text_source_kind_not_found");
			} else {
				obs_source_release(source);
			}
		}
		return result;
	}

	for (const OverlayField &field : fields) {
		if (field.settings.source_name.empty()) {
			continue;
		}
		obs_source_t *source = obs_get_source_by_name(field.settings.source_name.c_str());
		if (source) {
			obs_source_release(source);
			continue;
		}
		create_source(result, field, source_kind);
	}
	return result;
}

void log_overlay_source_result(const OverlaySourceEnsureResult &result)
{
	for (const OverlaySourceDiagnostic &diagnostic : result.diagnostics) {
		blog(LOG_WARNING, "%s overlay diagnostic=%s field=%s source=%s action=%s reason=%s", kLogPrefix,
		     diagnostic.code.c_str(), diagnostic.field.c_str(), diagnostic.source_name.c_str(),
		     diagnostic.action.c_str(), diagnostic.reason.c_str());
	}
}

} // namespace odr::plugin
