#pragma once
#include "alpha/core.hpp"
#include "alpha/spherical_wkb.hpp"

namespace alpha {
struct SphericalModelResult {
    SphericalWkbResult spherical;
    std::optional<Result> external_reference;
};
// Shared CLI/viewer entry point. The external comparison exists only when the
// actual request is within its domain; validation substitutions are never shown.
SphericalModelResult calculate_spherical_model(const Request&, SphericalPotential);
}
