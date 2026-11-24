#pragma once
#include "alpha/spherical_wkb.hpp"
#include <nlohmann/json.hpp>
// Validates supplied analysis options even when no selected channel is open.
void validate_numerical_analysis_options(const nlohmann::json&);
// Appends optional analysis results to the normal spherical response.
void append_numerical_analyses(const nlohmann::json&, const alpha::SphericalWkbRequest&, nlohmann::json&);
