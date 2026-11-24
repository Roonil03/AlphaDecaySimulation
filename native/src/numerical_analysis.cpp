#include "numerical_analysis.hpp"
#include "alpha/uncertainty.hpp"
#ifdef ALPHA_ENABLE_NUMEROV
#include "alpha/numerov.hpp"
#endif
#include <limits>
#include <set>
#include <stdexcept>
using json=nlohmann::json;
namespace {
void keys(const json& j, const std::set<std::string>& allowed) {
    if (!j.is_object()) throw std::invalid_argument("analysis options must be an object");
    for (auto it=j.begin();it!=j.end();++it) if (!allowed.contains(it.key())) throw std::invalid_argument("unknown analysis field: "+it.key());
}
std::uint64_t integer(const json& j,const char* key,std::uint64_t fallback) {
    if(!j.contains(key)) return fallback;
    const auto& x=j.at(key);
    if(x.is_number_unsigned()) return x.get<std::uint64_t>();
    if(x.is_number_integer()) { auto v=x.get<std::int64_t>(); if(v>=0) return static_cast<std::uint64_t>(v); }
    throw std::invalid_argument(std::string(key)+" must be a nonnegative integer");
}
json number(const std::optional<double>& v) {return v ? json(*v) : json(nullptr);}
#ifdef ALPHA_ENABLE_NUMEROV
alpha::NumerovOptions numerov_options(const json& n) {
    keys(n,{"lead_radius_fm","match_radius_fm","step_fm"});
    alpha::NumerovOptions o;
    o.lead_radius_fm=n.value("lead_radius_fm",o.lead_radius_fm);
    o.match_radius_fm=n.value("match_radius_fm",o.match_radius_fm);
    o.step_fm=n.value("step_fm",o.step_fm);
    return o;
}
#endif
alpha::UncertaintyRequest uncertainty_request(const json& u) {
    keys(u,{"seed","sample_count","standard_deviations","covariance"});
    alpha::UncertaintyRequest r; r.seed=integer(u,"seed",r.seed);
    auto count=integer(u,"sample_count",r.draws);
    if(count==0 || count>1000000) throw std::invalid_argument("sample_count out of range");
    r.draws=static_cast<std::size_t>(count);
    if(u.contains("standard_deviations")) {
        const auto& a=u.at("standard_deviations");
        if(!a.is_array()||a.size()!=4) throw std::invalid_argument("standard_deviations must have four entries");
        for(int i=0;i<4;++i) r.standard_deviations[i]=a.at(i).get<double>();
    }
    if(u.contains("covariance")) {
        const auto& a=u.at("covariance");
        if(!a.is_array()||a.size()!=4) throw std::invalid_argument("covariance must be a 4x4 array");
        std::array<double,16> c{};
        for(int i=0;i<4;++i) {
            if(!a[i].is_array()||a[i].size()!=4) throw std::invalid_argument("covariance must be a 4x4 array");
            for(int k=0;k<4;++k) c[i*4+k]=a[i][k].get<double>();
        }
        r.covariance=c;
    }
    if(const auto error=alpha::validate_uncertainty_request(r)) throw std::invalid_argument(*error);
    return r;
}
}
void validate_numerical_analysis_options(const json& j) {
    if(j.contains("numerov")) {
#ifdef ALPHA_ENABLE_NUMEROV
        (void)numerov_options(j.at("numerov"));
#else
        throw std::invalid_argument("Numerov support disabled at build time");
#endif
    }
    if(j.contains("uncertainty")) (void)uncertainty_request(j.at("uncertainty"));
}
void append_numerical_analyses(const json& j,const alpha::SphericalWkbRequest& input,json& out) {
    if(j.contains("numerov")) {
#ifdef ALPHA_ENABLE_NUMEROV
        const auto o=numerov_options(j.at("numerov"));
        auto r=alpha::calculate_numerov(input,o);
        out["numerov"]={{"status",r.status},{"message",r.message},{"log10_transmission",number(r.log10_transmission)},{"log10_half_life_s",number(r.log10_half_life_s)},{"step_error_decades",number(r.step_error_decades)},{"match_error_decades",number(r.match_error_decades)},{"wkb_difference_decades",number(r.wkb_difference_decades)},{"lead_radius_fm",o.lead_radius_fm},{"centrifugal_convention","physical"},{"wkb_centrifugal_convention",input.potential.centrifugal==alpha::CentrifugalConvention::physical ? "physical" : "langer_wkb"},{"comparison_note",input.potential.centrifugal==alpha::CentrifugalConvention::physical ? "Same physical centrifugal term" : "Different centrifugal terms: Numerov is physical; WKB uses Langer"}};
#else
        throw std::invalid_argument("Numerov support disabled at build time");
#endif
    }
    if(j.contains("uncertainty")) {
        auto r=uncertainty_request(j.at("uncertainty")); r.baseline=input;
        auto x=alpha::sample_spherical_uncertainty(r);
        if(x.status=="invalid") throw std::invalid_argument(x.message);
        out["uncertainty"]={{"status",x.status},{"message",x.message},{"seed",r.seed},{"sample_count",r.draws},{"accepted",x.accepted},{"rejected",x.rejected},{"rejection_fraction",x.rejection_fraction},{"closed",x.closed},{"zero_rate",x.zero_rate},{"invalid",x.invalid},{"numerical_failure",x.numerical_failure},{"unsupported",x.unsupported},{"assumed_independent",x.assumed_independent},{"log10_half_life_p16",number(x.log10_half_life_p16)},{"log10_half_life_p50",number(x.log10_half_life_p50)},{"log10_half_life_p84",number(x.log10_half_life_p84)}};
    }
}


