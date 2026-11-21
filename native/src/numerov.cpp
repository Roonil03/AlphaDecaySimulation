#include "alpha/numerov.hpp"
#include <cmath>
namespace alpha {
namespace { int32_t is_cancelled(void* p) { return static_cast<const std::atomic_bool*>(p)->load() ? 1 : 0; } }
NumerovResult calculate_numerov(const SphericalWkbRequest& input, const NumerovOptions& options, const std::atomic_bool* cancel) {
    const auto& p=input.potential;
    alpha_numerov_request r{}; r.version=1; r.kind=p.kind==PotentialKind::square_well ? 0 : 1; r.l=p.l;
    r.daughter_z=p.daughter_z; r.reduced_mass_mev=p.reduced_mass_mev; r.q_mev=input.q_mev;
    r.depth_mev=p.nuclear_depth_mev; r.radius_fm=p.nuclear_radius_fm; r.diffuseness_fm=p.diffuseness_fm; r.charge_radius_fm=p.charge_radius_fm;
    r.lead_radius_fm=options.lead_radius_fm; r.match_radius_fm=options.match_radius_fm; r.step_fm=options.step_fm;
    alpha_numerov_result out{};
    alpha_numerov_solve_cancelable(&r,&out,cancel ? is_cancelled : nullptr,const_cast<std::atomic_bool*>(cancel));
    NumerovResult result; const char* statuses[]={"converged","unconverged","invalid_input","numerical_failure","cancelled"};
    result.status=out.status<=4 ? statuses[out.status] : "numerical_failure"; result.message=out.message;
    if (out.status<=1) {
        result.log10_transmission=out.log10_transmission; result.step_error_decades=out.step_error_decades; result.match_error_decades=out.match_error_decades;
        if (out.status==0 && input.preformation>0 && input.preformation<=1 && input.assault_hz>0 && std::isfinite(input.assault_hz))
            result.log10_half_life_s=std::log10(std::log(2.0))-std::log10(input.preformation)-std::log10(input.assault_hz)-out.log10_transmission;
        auto wkb=calculate_spherical_wkb(input);
        if (wkb.log10_penetrability) result.wkb_difference_decades=out.log10_transmission-*wkb.log10_penetrability;
    }
    return result;
}
}
