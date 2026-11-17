#include "alpha/core.hpp"
#include <algorithm>
#include <cmath>
#include <numbers>

namespace alpha {
namespace {
constexpr double hbar_c = 197.3269804, amu = 931.49410242, e2 = 1.43996448;
bool in(double x, double lo, double hi) {
  return std::isfinite(x) && x >= lo && x <= hi;
}
double action(double radius, double outer, double q, double c, double d,
              double mu) {
  if (radius >= outer)
    return 0;
  constexpr int n = 4096;
  const double lr = std::log(outer / radius);
  double sum = 0;
  for (int i = 0; i <= n; ++i) {
    double t = double(i) / n, r = radius * std::exp(lr * t * (2 - t));
    double difference = c / r + d / (r * r) - q;
    double value =
        std::sqrt(2 * mu * std::max(0.0, difference)) * (2 * r * lr * (1 - t));
    sum += (i == 0 || i == n ? 1 : i % 2 ? 4 : 2) * value;
  }
  return sum / (3 * n * hbar_c);
}
} // namespace
Result calculate(const Request &p) {
  if (p.parent_z < 3 || p.parent_z > 130 || p.parent_a < 5 ||
      p.parent_a > 400 || p.parent_a - p.parent_z < 2 || p.l < 0 || p.l > 100 ||
      !in(p.q_mev, .01, 100) || !in(p.radius_fm, .1, 100) ||
      !in(p.preformation, 0, 1) || p.preformation == 0 ||
      !in(p.assault_hz, 1e-300, 1e30) ||
      (p.parent_binding_mev &&
       (!in(*p.parent_binding_mev, 0, 10000) || *p.parent_binding_mev == 0)))
    throw std::invalid_argument("input outside supported domain");
  const double daughter = p.parent_a - 4.0,
               mu = amu * 4 * daughter / p.parent_a;
  const double c = 2 * (p.parent_z - 2) * e2;
  const double d = hbar_c * hbar_c * p.l * (p.l + 1) / (2 * mu);
  const double outer = (c + std::sqrt(c * c + 4 * p.q_mev * d)) / (2 * p.q_mev);
  const double logp =
      -2 * action(p.radius_fm, outer, p.q_mev, c, d, mu) / std::log(10.0);
  Result result{std::log10(std::log(2.0)) - std::log10(p.preformation) -
                    std::log10(p.assault_hz) - logp,
                logp,
                outer,
                {},
                {}};
  if (p.research_model && p.parent_z % 2 == 0 &&
      (p.parent_a - p.parent_z) % 2 == 0 && p.l == 0) {
    const double ar = 4 * daughter / p.parent_a, charge = 2 * (p.parent_z - 2);
    const double chi = charge * std::sqrt(ar / p.q_mev);
    const double rho =
        std::sqrt(ar * charge * (std::cbrt(daughter) + std::cbrt(4.0)));
    result.research_log10_half_life_s = .4065 * chi - .4311 * rho - 20.7889;
    if (p.parent_binding_mev) {
      const double a = p.parent_a, z = p.parent_z;
      const double drop = 15.5287 * a - 16.9043 * std::pow(a, 2.0 / 3) -
                          91.9686 * std::pow(a / 2 - z, 2) / a -
                          .7025 * z * z / std::cbrt(a) + 12.4439 / std::sqrt(a);
      result.shell_log10_half_life_s =
          -28.1919 - 1.0853 * std::pow(a, 1.0 / 6) * std::sqrt(z) +
          1.6260 * z / std::sqrt(p.q_mev) +
          .1078 * (*p.parent_binding_mev - drop - 2);
    }
  }
  return result;
}
} // namespace alpha
