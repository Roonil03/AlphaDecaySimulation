#include "alpha_numerov.h"
#include <cmath>
#include <stdexcept>
int main() {
    alpha_numerov_request req{};
    req.version=1; req.kind=0; req.reduced_mass_mev=3657; req.q_mev=5;
    req.radius_fm=7; req.diffuseness_fm=.65; req.charge_radius_fm=7;
    req.lead_radius_fm=2; req.match_radius_fm=20; req.step_fm=.01;
    alpha_numerov_result out{};
    if (alpha_numerov_solve(nullptr,&out)!=-1 || alpha_numerov_solve(&req,nullptr)!=-1)
        throw std::runtime_error("null pointer handling");
    if (alpha_numerov_solve(&req,&out)!=0 || out.version!=1 || out.status!=0 || std::abs(out.log10_transmission)>=1e-5)
        throw std::runtime_error("free transmission or ABI result");
}
