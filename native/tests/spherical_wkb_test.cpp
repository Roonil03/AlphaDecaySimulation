#include "alpha/spherical_wkb.hpp"
#include "alpha/spherical_model.hpp"
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <stdexcept>

using namespace alpha;
void check(bool pass, const char* message) { if (!pass) throw std::runtime_error(message); }
void near(double a, double b, double tol, const char* message) { check(std::abs(a-b)<=tol,message); }
template<class F> long double bisect_reference(F f, long double a, long double b) {
    const bool positive=f(a)>0;
    for (int i=0;i<100;++i) {
        const long double m=(a+b)/2;
        if ((f(m)>0)==positive) a=m; else b=m;
    }
    return (a+b)/2;
}
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
        {
            auto peak_request=request;
            peak_request.q_mev=22.80425893924691;
            // Independent analytic derivative brackets the smooth maximum. Q
            // lies above every old .02-fm scan sample but below this maximum.
            const auto reference_potential=[](long double r) {
                return 2*82*1.43996448L/r-100/(1+std::exp((r-7)/.65L));
            };
            const auto derivative=[](long double r) {
                const auto t=std::exp(-(r-7)/.65L);
                return 100*t/(.65L*(1+t)*(1+t))-2*82*1.43996448L/(r*r);
            };
            const auto maximum=bisect_reference(derivative,9,10);
            near(static_cast<double>(maximum),9.645957048708102,1e-11,"independent Woods-Saxon maximum location");
            near(static_cast<double>(reference_potential(maximum)),22.80428784202867,1e-11,"independent Woods-Saxon maximum height");
            const auto excess=[&](long double r){return reference_potential(r)-peak_request.q_mev;};
            check(excess(9.64L)<0 && excess(9.66L)<0 && excess(maximum)>0,"near-peak forbidden interval is narrower than scan spacing");
            const auto inner=bisect_reference(excess,9.64L,maximum);
            const auto outer=bisect_reference(excess,maximum,9.66L);
            // Independent fixed midpoint quadrature in r, without the solver's
            // root finder or adaptive sine-coordinate Simpson quadrature.
            constexpr int panels=65536;
            long double area=0;
            const auto width=(outer-inner)/panels;
            for (int i=0;i<panels;++i) area+=std::sqrt(excess(inner+(i+.5L)*width));
            const auto action=area*width*std::sqrt(2.L*p.reduced_mass_mev)/197.3269804L;
            const auto narrow=calculate_spherical_wkb(peak_request);
            check(narrow.status=="ok" && narrow.action && narrow.log10_half_life_s,"near-peak Woods-Saxon barrier is resolved");
            near(*narrow.inner_turning_fm,static_cast<double>(inner),1e-8,"near-peak inner root matches independent reference");
            near(*narrow.outer_turning_fm,static_cast<double>(outer),1e-8,"near-peak outer root matches independent reference");
            near(*narrow.action,static_cast<double>(action),1e-10,"near-peak action matches independent quadrature");
        }
        {
            // The shared radial solver must refine the supplied callback, not
            // substitute spherical extrema for a deformed/custom potential.
            const double center=9.647, half_width=.002;
            const auto narrow=calculate_radial_wkb(request,[&](double r){return request.q_mev+half_width*half_width-(r-center)*(r-center);},7);
            check(narrow.status=="ok" && narrow.action,"callback narrow maximum is resolved");
            near(*narrow.inner_turning_fm,center-half_width,1e-8,"callback narrow-barrier inner root");
            near(*narrow.outer_turning_fm,center+half_width,1e-8,"callback narrow-barrier outer root");
            near(*narrow.action,std::sqrt(2*p.reduced_mass_mev)/197.3269804*3.14159265358979323846*half_width*half_width/2,1e-10,"callback parabolic-barrier exact action");
            const double pocket_center=4.011, pocket_half_width=.001;
            const auto pocket=calculate_radial_wkb(request,[&](double r){return request.q_mev+((r-pocket_center)*(r-pocket_center)-pocket_half_width*pocket_half_width)*(10-r);},7);
            check(pocket.status=="ok" && pocket.action,"callback narrow minimum retains the allowed pocket");
            near(*pocket.inner_turning_fm,pocket_center+pocket_half_width,1e-8,"callback narrow-pocket outgoing root");
            near(*pocket.outer_turning_fm,10,1e-8,"callback narrow-pocket outer root");
            auto tiny_q=request; tiny_q.q_mev=1e-30;
            const auto exhausted=calculate_spherical_wkb(tiny_q);
            check(exhausted.status=="numerical_error" && !exhausted.action && !exhausted.log10_half_life_s,
                  "positive-Q outer-search exhaustion is numerical failure, not a closed barrier");
        }
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
