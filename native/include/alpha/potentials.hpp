#pragma once

namespace alpha {
// Energies are MeV, distances fm, mass MeV/c^2; angular momentum is dimensionless.
enum class PotentialKind { square_well, woods_saxon };
enum class CentrifugalConvention { physical, langer_wkb };
struct SphericalPotential {
    PotentialKind kind = PotentialKind::woods_saxon;
    double daughter_z = 82;
    double reduced_mass_mev = 3657;
    int l = 0;
    double nuclear_depth_mev = 100;
    double nuclear_radius_fm = 7;
    double diffuseness_fm = 0.65;
    double charge_radius_fm = 7;
    CentrifugalConvention centrifugal = CentrifugalConvention::physical;
};
double nuclear_potential(const SphericalPotential&, double radius_fm);
double coulomb_potential(const SphericalPotential&, double radius_fm);
double centrifugal_potential(const SphericalPotential&, double radius_fm);
double spherical_potential(const SphericalPotential&, double radius_fm);
}
