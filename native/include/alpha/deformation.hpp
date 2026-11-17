#pragma once
#include "alpha/spherical_wkb.hpp"
namespace alpha {
struct Deformation { double beta2=0, beta4=0; };
struct DeformedWkbResult {
    std::string status, message;
    std::optional<double> log10_penetrability, log10_half_life_s;
    double angular_error_log10=0, coulomb_error_mev=0;
    int angular_points=0, multipole_order=0;
};
// Throws invalid_argument outside the supported positive, bounded shape domain.
double deformed_radius(double volume_radius_fm, Deformation, double cos_theta);
double deformed_coulomb(const SphericalPotential&, Deformation, double radius_fm, double cos_theta, int order=16, int quadrature=128);
DeformedWkbResult calculate_deformed_wkb(const SphericalWkbRequest&, Deformation);
}
