#include "alpha/channels.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
namespace alpha {
namespace {
bool valid(SpinParity s) { return s.twice_j>=0 && (s.parity==1 || s.parity==-1); }
}
ChannelSelection select_channel(double q, std::optional<SpinParity> parent, const ChannelInput& c) {
    ChannelSelection out;
    if (!std::isfinite(q) || !std::isfinite(c.excitation_mev) || c.excitation_mev<0 || (parent && !valid(*parent)) || (c.daughter && !valid(*c.daughter)) || (c.l && (*c.l<0 || *c.l>1000))) { out.status="invalid_input"; return out; }
    out.q_mev=q-c.excitation_mev;
    if (out.q_mev<=0) { out.status="closed"; return out; }
    if (!parent || !c.daughter) {
        if (!c.l) { out.status="unknown_spin_requires_l"; return out; }
        out.l=c.l; out.status="ok"; return out;
    }
    const int a=parent->twice_j, b=c.daughter->twice_j;
    if ((a-b)%2 != 0) { out.status="forbidden"; return out; }
    const int low=std::abs(a-b)/2, high=(a+b)/2;
    auto allowed=[&](int l){ return l>=low && l<=high && ((l%2==0 ? 1 : -1)==parent->parity*c.daughter->parity); };
    if (c.l) { if (!allowed(*c.l)) { out.status="forbidden"; return out; } out.l=c.l; }
    else { int l=low; if (!allowed(l)) ++l; if (!allowed(l)) { out.status="forbidden"; return out; } out.l=l; }
    out.status="ok"; return out;
}
std::vector<ChannelRate> channel_rates(const std::vector<ChannelInput>& cs, const std::vector<ChannelSelection>& selections, const std::vector<std::optional<double>>& logp, std::optional<double> competing) {
    if (cs.size()!=selections.size() || cs.size()!=logp.size() || (competing && (!std::isfinite(*competing) || *competing<0))) throw std::invalid_argument("Invalid channel rate inputs");
    std::vector<ChannelRate> out(cs.size());
    double maxlog=-std::numeric_limits<double>::infinity();
    for (size_t i=0;i<cs.size();++i) {
        auto& r=out[i]; r.status=selections[i].status;
        if (!std::isfinite(cs[i].preformation) || cs[i].preformation<0 || cs[i].preformation>1 || !std::isfinite(cs[i].assault_hz) || cs[i].assault_hz<=0 || (logp[i] && (!std::isfinite(*logp[i]) || *logp[i]>0))) throw std::invalid_argument("Invalid rate parameters");
        if (r.status!="ok" || !logp[i]) continue;
        if (cs[i].preformation==0) { r.status="zero_preformation"; continue; }
        r.log10_rate_hz=std::log10(cs[i].preformation)+std::log10(cs[i].assault_hz)+*logp[i];
        r.log10_partial_half_life_s=std::log10(std::log(2.0))-*r.log10_rate_hz;
        maxlog=std::max(maxlog,*r.log10_rate_hz);
    }
    double scaled=0;
    if (std::isfinite(maxlog)) for (const auto& r:out) if (r.log10_rate_hz) scaled+=std::pow(10.0,*r.log10_rate_hz-maxlog);
    const double logsum=scaled>0 ? maxlog+std::log10(scaled) : -std::numeric_limits<double>::infinity();
    double totalmax=logsum;
    if (competing && *competing>0) totalmax=std::max(logsum,std::log10(*competing));
    double total_scaled=0;
    if (std::isfinite(totalmax)) { if (std::isfinite(logsum)) total_scaled+=std::pow(10.0,logsum-totalmax); if (competing && *competing>0) total_scaled+=std::pow(10.0,std::log10(*competing)-totalmax); }
    for (auto& r:out) {
        if (r.log10_rate_hz) r.conditional_alpha_branch=std::pow(10.0,*r.log10_rate_hz-logsum);
        if (competing && total_scaled>0) r.total_branch=r.log10_rate_hz ? std::pow(10.0,*r.log10_rate_hz-totalmax)/total_scaled : 0;
    }
    return out;
}
}
