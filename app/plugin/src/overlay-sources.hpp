#pragma once

#include "plugin-settings.hpp"
#include "worker-api.hpp"

#include <string>
#include <vector>

namespace odr::plugin {

struct OverlaySourceDiagnostic {
	std::string code;
	std::string field;
	std::string source_name;
	std::string action;
	std::string reason;
};

struct OverlaySourceEnsureResult {
	bool success = true;
	std::vector<OverlaySourceDiagnostic> diagnostics;
};

OverlaySourceEnsureResult ensure_overlay_text_sources(const OverlaySettings &settings);
OverlaySourceEnsureResult update_overlay_sources(const OverlaySettings &settings, const OverlayStatePayload &state);
OverlaySourceEnsureResult update_deck_sequence_overlay_sources(const OverlaySettings &settings,
							       const OverlayStatePayload &state);
void log_overlay_source_result(const OverlaySourceEnsureResult &result);

} // namespace odr::plugin
