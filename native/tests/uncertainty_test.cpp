#include "alpha/uncertainty.hpp"
#include <cassert>
#include <cmath>
int main() {
    alpha::UncertaintyRequest r; r.draws=40; r.baseline.potential.kind=alpha::PotentialKind::square_well; r.baseline.potential.nuclear_radius_fm=9; r.baseline.potential.charge_radius_fm=9; r.standard_deviations[0]=0.05;
    auto a=alpha::sample_spherical_uncertainty(r), b=alpha::sample_spherical_uncertainty(r);
    assert(a.status=="ok" && a.accepted==40 && a.assumed_independent && a.log10_half_life_p16<a.log10_half_life_p84 && a.log10_half_life_p50==b.log10_half_life_p50);
    std::array<double,16> bad{}; bad[0]=-1; r.covariance=bad; assert(alpha::sample_spherical_uncertainty(r).status=="invalid");
    r.covariance=std::array<double,16>{}; auto c=alpha::sample_spherical_uncertainty(r); assert(c.status=="ok" && !c.assumed_independent && c.log10_half_life_p16==c.log10_half_life_p84);
}
