#pragma once
#include <optional>
#include <stdexcept>

namespace alpha {
struct Request {
  int parent_z = 84, parent_a = 212, l = 0;
  double q_mev = 8.9542, radius_fm = 9.0, preformation = 0.1, assault_hz = 1e21;
  bool research_model = true;
  std::optional<double> parent_binding_mev;
};
struct Result {
  double log10_half_life_s, log10_penetrability, outer_turning_fm;
  std::optional<double> research_log10_half_life_s, shell_log10_half_life_s;
};
// External spherical Coulomb plus centrifugal barrier, in MeV and fm.
Result calculate(const Request &request);
} // namespace alpha
