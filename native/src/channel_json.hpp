#pragma once
#include "alpha/spherical_wkb.hpp"
#include <nlohmann/json.hpp>
// Input fields: deformation {beta2,beta4}, parent_spin {twice_j,parity},
// channels [{name,excitation_mev,daughter_spin,l,preformation,assault_hz}],
// and optional competing_rate_hz. Base has the selected potential and Q.
nlohmann::json run_channels(const nlohmann::json& input, const alpha::SphericalWkbRequest& base);
