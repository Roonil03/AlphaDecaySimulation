#pragma once
#include "alpha/spherical_wkb.hpp"
#include <nlohmann/json.hpp>
// Appends optional analysis results to the normal spherical response.
void append_numerical_analyses(const nlohmann::json&, const alpha::SphericalWkbRequest&, nlohmann::json&);
