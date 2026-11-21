#pragma once
#include "alpha/spherical_wkb.hpp"
#include "alpha_numerov.h"
#include <atomic>
#include <optional>
#include <string>
namespace alpha {
struct NumerovOptions { double lead_radius_fm=5, match_radius_fm=500, step_fm=.05; };
struct NumerovResult {
    std::string status, message;
    std::optional<double> log10_transmission, log10_half_life_s, step_error_decades, match_error_decades, wkb_difference_decades;
};
NumerovResult calculate_numerov(const SphericalWkbRequest&, const NumerovOptions&, const std::atomic_bool* cancelled=nullptr);
}
