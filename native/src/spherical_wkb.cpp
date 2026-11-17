#include "alpha/spherical_wkb.hpp"
#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <stdexcept>
#include <vector>

namespace alpha {
namespace {
constexpr double hbar_c = 197.3269804;
constexpr double pi = 3.14159265358979323846;
struct Edge { double r; bool enters_forbidden; };
double root(const std::function<double(double)>& f, double a, double b) {
    const bool positive = f(a) > 0;
    for (int i = 0; i < 200 && b - a > 1e-12 * std::max(1.0, b); ++i) {
        const double m = a + (b-a)/2;
        if ((f(m) > 0) == positive) a = m; else b = m;
    }
    return a + (b-a)/2;
}
double integrate(const std::function<double(double)>& f, double a, double b, double fa, double fm, double fb, double whole, double tol, int depth) {
    const double m=(a+b)/2, lm=(a+m)/2, rm=(m+b)/2;
    const double fl=f(lm), fr=f(rm);
    const double left=(m-a)*(fa+4*fl+fm)/6, right=(b-m)*(fm+4*fr+fb)/6;
    const double delta=left+right-whole;
    if (std::abs(delta) <= 15*tol) return left+right+delta/15;
    if (!depth) throw std::runtime_error("quadrature did not converge");
    return integrate(f,a,m,fa,fl,fm,left,tol/2,depth-1)+integrate(f,m,b,fm,fr,fb,right,tol/2,depth-1);
}
}
SphericalWkbResult calculate_radial_wkb(const SphericalWkbRequest& req, const std::function<double(double)>& potential, double discontinuity_fm) {
    const auto& p=req.potential;
    auto fail=[](const char* status, const char* message) { SphericalWkbResult r; r.status=status; r.message=message; return r; };
    const auto finite=[](double x){return std::isfinite(x);};
    if (!finite(req.q_mev) || req.q_mev < 0 || !finite(req.preformation) || req.preformation < 0 || req.preformation > 1 ||
        !finite(req.assault_hz) || req.assault_hz <= 0 || !finite(p.daughter_z) || p.daughter_z <= 0 ||
        !finite(p.reduced_mass_mev) || p.reduced_mass_mev <= 0 || p.l < 0 || !finite(p.nuclear_depth_mev) || p.nuclear_depth_mev <= 0 ||
        !finite(p.nuclear_radius_fm) || p.nuclear_radius_fm <= 0 || !finite(p.charge_radius_fm) || p.charge_radius_fm <= 0 ||
        !finite(p.diffuseness_fm) || p.diffuseness_fm <= 0)
        return fail("invalid_input", "Parameters must be finite and in their physical domain.");
    if (req.q_mev == 0) return fail("closed_barrier", "Zero decay energy has no finite outer Coulomb turning point.");
    const auto f=[&](double r){return potential(r)-req.q_mev;};
    // Fine scan through the nuclear region, then exponentially spaced exterior samples.
    const double end=std::max(p.charge_radius_fm, p.nuclear_radius_fm+40*p.diffuseness_fm);
    const double step=std::min(0.02, p.diffuseness_fm/16);
    if (!finite(end) || end/step > 1000000) return fail("unsupported_topology", "Potential scale exceeds scan limit.");
    std::vector<Edge> edges;
    double previous=std::min(1e-8,step/100), y=f(previous);
    const bool initially_forbidden = y > 0;
    auto visit=[&](double r, bool discontinuity) {
        double value=f(r);
        if ((y>0)!=(value>0)) edges.push_back({discontinuity ? discontinuity_fm : root(f,previous,r),value>0});
        previous=r; y=value;
    };
    const int n=static_cast<int>(std::ceil(end/step));
    for (int i=1;i<=n;++i) {
        const double r=end*i/n;
        if (p.kind==PotentialKind::square_well && previous < discontinuity_fm && r >= discontinuity_fm) {
            visit(std::nextafter(discontinuity_fm,0.0),false);
            visit(discontinuity_fm,true);
        }
        if (r>previous) visit(r,false);
    }
    for (int i=0;i<200 && y>0;++i) visit(previous*1.25,false);
    if (y>0) return fail("closed_barrier", "No finite outer turning point found.");
    // Only one outgoing barrier and at most one inner centrifugal boundary are supported.
    std::vector<Edge> outgoing;
    for (const auto& edge: edges) if (edge.enters_forbidden) outgoing.push_back(edge);
    if (outgoing.empty()) return initially_forbidden && !edges.empty()
        ? fail("no_pocket", "No allowed pocket lies inside the outer turning point.")
        : fail("above_barrier", "No outgoing barrier enclosing an allowed pocket was found.");
    if (outgoing.size()!=1 || edges.back().enters_forbidden || edges.size()>3 ||
        (edges.size()==3 && (edges[0].enters_forbidden || !edges[1].enters_forbidden)))
        return fail("unsupported_topology", "Expected one allowed pocket and one outgoing barrier.");
    const double inner=outgoing.front().r, outer=edges.back().r;
    if (!(outer>inner)) return fail("unsupported_topology", "Turning points are not ordered.");
    const double scale=std::sqrt(2*p.reduced_mass_mev)/hbar_c;
    const auto integrand=[&](double t) {
        const double s=std::sin(t), c=std::cos(t), r=inner+(outer-inner)*s*s;
        return scale*std::sqrt(std::max(0.0,f(r)))*2*(outer-inner)*s*c;
    };
    try {
        const double a=0,b=pi/2,m=(a+b)/2,fa=integrand(a),fm=integrand(m),fb=integrand(b);
        const double action=integrate(integrand,a,b,fa,fm,fb,(b-a)*(fa+4*fm+fb)/6,1e-8,28);
        if (!finite(action)) return fail("numerical_error", "Action is not finite.");
        SphericalWkbResult result; result.status=req.preformation==0 ? "zero_preformation" : "ok";
        result.inner_turning_fm=inner; result.outer_turning_fm=outer; result.action=action;
        result.log10_penetrability=-2*action/std::log(10.0);
        if (req.preformation>0) result.log10_half_life_s=std::log10(std::log(2.0))-std::log10(req.assault_hz)-std::log10(req.preformation)-*result.log10_penetrability;
        else result.message="Zero preformation gives an infinite half-life; the JSON value must be null.";
        if (!finite(*result.log10_penetrability) || (result.log10_half_life_s && !finite(*result.log10_half_life_s)))
            return fail("numerical_error", "Log result is not finite.");
        return result;
    } catch (const std::exception&) { return fail("numerical_error", "Action quadrature did not converge."); }
}
SphericalWkbResult calculate_spherical_wkb(const SphericalWkbRequest& req) {
    return calculate_radial_wkb(req, [&](double r){ return spherical_potential(req.potential,r); }, req.potential.nuclear_radius_fm);
}
}

