#include "alpha/uncertainty.hpp"
#include <algorithm>
#include <cmath>
#include <random>
#include <limits>
#include <vector>
namespace alpha {
UncertaintyResult sample_spherical_uncertainty(const UncertaintyRequest& r, const std::function<bool()>& cancelled) {
    UncertaintyResult out;
    if (r.draws == 0 || r.draws > 1000000) { out.status="invalid"; out.message="Draw count must be between 1 and 1000000"; return out; }
    std::array<double,16> cov{};
    if (r.covariance) cov=*r.covariance;
    else { out.assumed_independent=true; for (int i=0;i<4;++i) { if (!std::isfinite(r.standard_deviations[i]) || r.standard_deviations[i]<0) {out.status="invalid"; out.message="Invalid standard deviation"; return out;} cov[4*i+i]=r.standard_deviations[i]*r.standard_deviations[i]; } }
    double scale=0;
    for (double x:cov) {if (!std::isfinite(x)) {out.status="invalid"; out.message="Covariance must be finite"; return out;} scale=std::max(scale,std::abs(x));}
    const double tolerance=64*std::numeric_limits<double>::epsilon()*scale;
    for (int i=0;i<4;++i) for (int j=0;j<4;++j) if (std::abs(cov[4*i+j]-cov[4*j+i])>tolerance) {out.status="invalid"; out.message="Covariance must be symmetric"; return out;}
    std::array<double,16> L{};
    for (int i=0;i<4;++i) for (int j=0;j<=i;++j) {
        double x=cov[4*i+j]; for(int k=0;k<j;++k) x-=L[4*i+k]*L[4*j+k];
        if (i==j) {if (x < -tolerance) {out.status="invalid"; out.message="Covariance is not positive semidefinite"; return out;} L[4*i+j]=std::sqrt(std::max(0.0,x));}
        else if (L[4*j+j]>0) L[4*i+j]=x/L[4*j+j];
        else if (std::abs(x)>tolerance) {out.status="invalid"; out.message="Covariance is not positive semidefinite"; return out;}
    }
    std::mt19937_64 rng(r.seed); std::normal_distribution<double> normal; std::vector<double> values; values.reserve(r.draws);
    for (std::size_t n=0;n<r.draws;++n) {
        if (cancelled && cancelled()) {out.status="cancelled"; out.message="Cancelled"; return out;}
        std::array<double,4> z{}, d{}; for (auto& v:z) v=normal(rng);
        for (int i=0;i<4;++i) for (int j=0;j<=i;++j) d[i]+=L[4*i+j]*z[j];
        auto p=r.baseline; p.q_mev+=d[0]; p.potential.nuclear_depth_mev+=d[1]; p.potential.nuclear_radius_fm+=d[2]; p.potential.diffuseness_fm+=d[3];
        auto x=calculate_spherical_wkb(p); if(x.status=="ok" && x.log10_half_life_s && std::isfinite(*x.log10_half_life_s)) values.push_back(*x.log10_half_life_s); else ++out.rejected;
    }
    out.accepted=values.size(); if(values.empty()) {out.status="no_valid_draws"; out.message="All parameter draws were rejected"; return out;}
    std::sort(values.begin(),values.end()); auto quantile=[&](double q) {double pos=q*(values.size()-1); auto lo=static_cast<std::size_t>(pos); auto hi=std::min(lo+1,values.size()-1); return values[lo]+(pos-lo)*(values[hi]-values[lo]);};
    out.log10_half_life_p16=quantile(.16); out.log10_half_life_p50=quantile(.5); out.log10_half_life_p84=quantile(.84); out.status="ok";
    out.message=out.assumed_independent ? "Input covariance absent: parameters assumed independent; model and numerical errors are separate" : "Parameter spread only; model and numerical errors are separate"; return out;
}
}


