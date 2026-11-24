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
struct Sample { double r, value; bool discontinuity=false; };
// Refine a sampled local extremum without assuming an analytic potential. This
// also applies to the interpolated Coulomb callback used by the deformed solver.
Sample extremum(const std::function<double(double)>& f, Sample a, Sample b, Sample c, bool maximum) {
    constexpr double ratio=0.6180339887498948482;
    const auto better=[&](double x, double y){return maximum ? x>y : x<y;};
    Sample best=b;
    double left=a.r, right=c.r;
    Sample x{right-ratio*(right-left),0}, y{left+ratio*(right-left),0};
    x.value=f(x.r); y.value=f(y.r);
    for (int i=0;i<200 && right-left>1e-12*std::max(1.0,right);++i) {
        if (better(x.value,best.value)) best=x;
        if (better(y.value,best.value)) best=y;
        if (better(x.value,y.value)) {
            right=y.r; y=x; x={right-ratio*(right-left),0}; x.value=f(x.r);
        } else {
            left=x.r; x=y; y={left+ratio*(right-left),0}; y.value=f(y.r);
        }
    }
    if (better(x.value,best.value)) best=x;
    if (better(y.value,best.value)) best=y;
    return best;
}
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
    // The fixed nuclear-region scan is validated for this numerical scale.
    // Reject larger depths before interpreting an unobserved tail as no barrier.
    if (p.nuclear_depth_mev > 10000)
        return fail("invalid_input", "Nuclear depth exceeds the supported maximum of 10000 MeV.");
    if (req.q_mev == 0) return fail("closed_barrier", "Zero decay energy has no finite outer Coulomb turning point.");
    const auto f=[&](double r){
        const double value=potential(r)-req.q_mev;
        if (!finite(value)) throw std::runtime_error("Potential evaluation is not finite.");
        return value;
    };
    try {
    // Fine scan through the nuclear region, then exponentially spaced exterior samples.
    const bool diffuse=p.kind==PotentialKind::woods_saxon;
    const double end=std::max(p.charge_radius_fm, p.nuclear_radius_fm+(diffuse ? 40*p.diffuseness_fm : 0));
    const double step=diffuse ? std::min(0.02, p.diffuseness_fm/16) : 0.02;
    if (!finite(end) || end/step > 1000000) return fail("unsupported_topology", "Potential scale exceeds scan limit.");
    std::vector<Edge> edges;
    double previous=std::min(1e-8,step/100), y=f(previous);
    const bool initially_forbidden = y > 0;
    auto visit=[&](Sample s) {
        if (s.r<=previous) return;
        if ((y>0)!=(s.value>0)) edges.push_back({s.discontinuity ? discontinuity_fm : root(f,previous,s.r),s.value>0});
        previous=s.r; y=s.value;
    };
    // Delay each middle sample until its right neighbor is known. A refined
    // extremum may fall on either side of the middle sample; hold the right-side
    // point until the next interval, keeping the root brackets in radial order.
    // This uses constant scan memory even for the million-interval limit.
    Sample left{previous,y}, middle{};
    bool has_middle=false;
    std::optional<Sample> pending;
    auto sample=[&](double r, bool discontinuity=false) {
        const Sample right{r,f(r),discontinuity};
        if (!has_middle) {middle=right; has_middle=true; return;}
        std::optional<Sample> refined;
        if (!middle.discontinuity && !right.discontinuity) {
            const bool maximum=middle.value>=left.value && middle.value>=right.value && (middle.value>left.value || middle.value>right.value);
            const bool minimum=middle.value<=left.value && middle.value<=right.value && (middle.value<left.value || middle.value<right.value);
            if (maximum || minimum) refined=extremum(f,left,middle,right,maximum);
        }
        if (refined && refined->r<=middle.r) {
            if (pending && pending->r<refined->r) {visit(*pending); visit(*refined);}
            else {visit(*refined); if (pending) visit(*pending);}
        } else if (pending) visit(*pending);
        visit(middle);
        pending=refined && refined->r>middle.r ? refined : std::nullopt;
        left=middle; middle=right;
    };
    const int n=static_cast<int>(std::ceil(end/step));
    double last_scanned=previous;
    for (int i=1;i<=n;++i) {
        const double r=end*i/n;
        if (p.kind==PotentialKind::square_well && last_scanned < discontinuity_fm && r >= discontinuity_fm) {
            sample(std::nextafter(discontinuity_fm,0.0));
            sample(discontinuity_fm,true);
            last_scanned=discontinuity_fm;
        }
        if (r>last_scanned) {sample(r); last_scanned=r;}
    }
    if (pending) visit(*pending);
    if (has_middle) visit(middle);
    for (int i=0;i<200 && y>0;++i) {const double r=previous*1.25; visit({r,f(r)});}
    if (y>0) return fail("numerical_error", "Outer turning-point search exceeded its finite range at positive decay energy.");
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
    } catch (const std::exception& e) { return fail("numerical_error", e.what()); }
}
SphericalWkbResult calculate_spherical_wkb(const SphericalWkbRequest& req) {
    return calculate_radial_wkb(req, [&](double r){ return spherical_potential(req.potential,r); }, req.potential.nuclear_radius_fm);
}
}

