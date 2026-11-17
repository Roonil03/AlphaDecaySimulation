#include "alpha/potentials.hpp"
#include <cmath>
#include <limits>
#include <stdexcept>

namespace alpha {
namespace { constexpr double e2 = 1.43996448, hbar_c = 197.3269804; }
double nuclear_potential(const SphericalPotential& p, double r) {
    if (p.kind == PotentialKind::square_well)
        return r < p.nuclear_radius_fm ? -p.nuclear_depth_mev : 0.0;
    const double x = (r - p.nuclear_radius_fm) / p.diffuseness_fm;
    if (x > 700) return 0;
    return -p.nuclear_depth_mev / (1 + std::exp(x));
}
double coulomb_potential(const SphericalPotential& p, double r) {
    const double k = 2 * p.daughter_z * e2, R = p.charge_radius_fm;
    return r < R ? k * (3 - (r / R) * (r / R)) / (2 * R) : k / r;
}
double centrifugal_potential(const SphericalPotential& p, double r) {
    const double factor = p.centrifugal == CentrifugalConvention::physical
        ? static_cast<double>(p.l) * (p.l + 1) : (p.l + 0.5) * (p.l + 0.5);
    if (factor == 0) return 0;
    if (r == 0) return std::numeric_limits<double>::infinity();
    return hbar_c * hbar_c * factor / (2 * p.reduced_mass_mev * r * r);
}
double spherical_potential(const SphericalPotential& p, double r) {
    if (r < 0 || !std::isfinite(r)) throw std::invalid_argument("radius must be finite and nonnegative");
    return nuclear_potential(p, r) + coulomb_potential(p, r) + centrifugal_potential(p, r);
}
}
