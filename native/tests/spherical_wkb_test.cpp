#include "alpha/spherical_wkb.hpp"
#include "alpha/spherical_model.hpp"
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <stdexcept>

using namespace alpha;
void check(bool pass, const char* message) { if (!pass) throw std::runtime_error(message); }
void near(double a, double b, double tol, const char* message) { check(std::abs(a-b)<=tol,message); }
int main() {
    try {
        SphericalWkbRequest request;
        auto& p=request.potential;
        p.kind=PotentialKind::square_well; p.daughter_z=82; p.reduced_mass_mev=3657;
        p.nuclear_depth_mev=100; p.nuclear_radius_fm=9; p.charge_radius_fm=9; p.l=0;
        request.q_mev=5;
        auto result=calculate_spherical_wkb(request);
        check(result.status=="ok", "square well must have one pocket and barrier");
        const double k=2*p.daughter_z*1.43996448, b=k/request.q_mev, x=p.nuclear_radius_fm/b;
        const double expected=std::sqrt(2*p.reduced_mass_mev)/197.3269804*k/std::sqrt(request.q_mev)*(std::acos(std::sqrt(x))-std::sqrt(x*(1-x)));
        near(*result.inner_turning_fm,9,1e-12,"square discontinuity is barrier boundary");
        near(*result.outer_turning_fm,b,b*1e-10,"outer Coulomb turning point");
        near(*result.log10_penetrability,-2*expected/std::log(10.0),1e-7,"analytic Coulomb penetrability");
        for (double q : {0.01, 25.0}) {
            request.q_mev=q;
            result=calculate_spherical_wkb(request);
            check(result.status=="ok", "extreme Coulomb barrier");
            const double outer=k/q, ratio=p.nuclear_radius_fm/outer;
            const double action=std::sqrt(2*p.reduced_mass_mev)/197.3269804*k/std::sqrt(q)*(std::acos(std::sqrt(ratio))-std::sqrt(ratio*(1-ratio)));
            near(*result.log10_penetrability,-2*action/std::log(10.0),1e-5,"extreme analytic Coulomb penetrability");
        }
        request.q_mev=5;
        request.preformation=0; result=calculate_spherical_wkb(request);
        check(result.status=="zero_preformation" && !result.log10_half_life_s && result.log10_penetrability,"zero preformation status");
        request.preformation=.1; request.q_mev=1000; result=calculate_spherical_wkb(request);
        check(result.status=="above_barrier", "above barrier status");
        request.q_mev=5; p.kind=PotentialKind::woods_saxon; p.nuclear_radius_fm=7; p.charge_radius_fm=7;
        result=calculate_spherical_wkb(request);
        check(result.status=="ok" && *result.outer_turning_fm>*result.inner_turning_fm && *result.action>0,"Woods-Saxon barrier");
        near(coulomb_potential(p,7-1e-8),coulomb_potential(p,7+1e-8),1e-6,"charge sphere continuity");
        p.l=1; p.centrifugal=CentrifugalConvention::physical;
        const double physical=centrifugal_potential(p,7);
        p.centrifugal=CentrifugalConvention::langer_wkb;
        near(centrifugal_potential(p,7)/physical,2.25/2,1e-12,"Langer convention");
        result=calculate_spherical_wkb(request);
        check(result.status=="ok", "centrifugal inner boundary retains outgoing pocket");
        request.q_mev=0; check(calculate_spherical_wkb(request).status=="closed_barrier","zero-energy closed barrier");
        request.q_mev=5; p.nuclear_depth_mev=1; p.l=0; p.centrifugal=CentrifugalConvention::physical;
        check(calculate_spherical_wkb(request).status=="no_pocket","pocketless status");
        request.q_mev=-1; check(calculate_spherical_wkb(request).status=="invalid_input","invalid energy");
        request.q_mev=5; p.nuclear_depth_mev=1e18; p.nuclear_radius_fm=9; p.charge_radius_fm=9;
        // This accepted-domain regression formerly skipped the barrier at 40 fm.
        check(spherical_potential(p,35)<5 && spherical_potential(p,40)>5 && spherical_potential(p,50)<5,
              "extreme depth repro has a real outgoing barrier");
        result=calculate_spherical_wkb(request);
        check(result.status=="invalid_input" && !result.log10_half_life_s && !result.action,
              "unsupported depth must not fabricate above-barrier status");
        p.nuclear_depth_mev=10000;
        check(calculate_spherical_wkb(request).status=="ok","documented maximum depth remains supported");
        // Both CLI and the viewer Calculate action use this exact entry point.
        Request channel;
        for (auto kind : {PotentialKind::square_well, PotentialKind::woods_saxon}) {
            p.kind=kind; p.nuclear_depth_mev=100;
            for (double q : {0.0, .001}) {
                channel.q_mev=q;
                const auto calculation=calculate_spherical_model(channel,p);
                check(calculation.spherical.status==(q==0 ? "closed_barrier" : "ok"),
                      "shared viewer/CLI low-energy calculation");
                check(!calculation.external_reference,"no substituted external/Qi/Ren reference");
                if (q==0) check(!calculation.spherical.log10_half_life_s,"zero-Q lifetime unavailable");
            }
            channel.q_mev=5;
            check(calculate_spherical_model(channel,p).external_reference.has_value(),"actual reference available");
            channel.preformation=0;
            const auto zero=calculate_spherical_model(channel,p);
            check(zero.spherical.status=="zero_preformation" && !zero.external_reference,"zero preformation viewer status");
            channel.preformation=.1;
        }
        std::cout << "spherical WKB tests passed\n";
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return EXIT_FAILURE; }
}
