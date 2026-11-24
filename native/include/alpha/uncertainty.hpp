#pragma once
#include "alpha/spherical_wkb.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
namespace alpha {
// Gaussian parameter order: Q, depth, nuclear radius, diffuseness. Covariance
// has squared units and must be symmetric positive semidefinite.
struct UncertaintyRequest {
    SphericalWkbRequest baseline;
    std::array<double,4> standard_deviations{};
    std::optional<std::array<double,16>> covariance;
    std::uint64_t seed = 1;
    std::size_t draws = 1000;
};
struct UncertaintyResult {
    std::string status, message;
    std::size_t accepted = 0, rejected = 0, closed = 0, zero_rate = 0, invalid = 0, numerical_failure = 0, unsupported = 0;
    double rejection_fraction = 0;
    std::optional<double> log10_half_life_p16, log10_half_life_p50, log10_half_life_p84;
    bool assumed_independent = false;
};
// Validates only the requested sampling distribution and draw count; it does
// not evaluate the baseline barrier or generate draws.
std::optional<std::string> validate_uncertainty_request(const UncertaintyRequest&);
UncertaintyResult sample_spherical_uncertainty(const UncertaintyRequest&, const std::function<bool()>& cancelled = {});
}


