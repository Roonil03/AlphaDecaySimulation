#pragma once
#include "alpha/potentials.hpp"
#include <optional>
#include <string>

namespace alpha {
struct SphericalWkbRequest {
    SphericalPotential potential;
    double q_mev = 5;
    double preformation = 0.1;
    double assault_hz = 1e21;
};
struct SphericalWkbResult {
    std::string status;
    std::optional<double> inner_turning_fm, outer_turning_fm, action, log10_penetrability, log10_half_life_s;
    std::string message;
};
// A single allowed pocket followed by one barrier is required. Log results are
// retained even when ordinary floating-point probabilities would underflow.
SphericalWkbResult calculate_spherical_wkb(const SphericalWkbRequest&);
}
