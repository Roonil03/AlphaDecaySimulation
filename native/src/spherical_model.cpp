#include "alpha/spherical_model.hpp"
#include <cmath>

namespace alpha {
SphericalWkbRequest make_spherical_wkb_request(const Request& request, SphericalPotential potential) {
    potential.daughter_z = request.parent_z - 2;
    potential.reduced_mass_mev = 931.49410242 * 4.0 * (request.parent_a - 4.0) / request.parent_a;
    potential.l = request.l;
    SphericalWkbRequest input;
    input.potential = potential;
    input.q_mev = request.q_mev;
    input.preformation = request.preformation;
    input.assault_hz = request.assault_hz;
    return input;
}
SphericalModelResult calculate_spherical_model(const Request& request, SphericalPotential potential) {
    if (!std::isfinite(request.q_mev) || request.q_mev < 0 || request.q_mev > 100 ||
        !std::isfinite(request.preformation) || request.preformation < 0 || request.preformation > 1)
        throw std::invalid_argument("input outside supported domain");
    auto checked = request;
    if (checked.q_mev < .01) checked.q_mev = .01;
    if (checked.preformation == 0) checked.preformation = 1;
    const auto reference = calculate(checked);
    auto input=make_spherical_wkb_request(request,potential);
    SphericalModelResult result;
    result.spherical = calculate_spherical_wkb(input);
    if (result.spherical.status == "invalid_input")
        throw std::invalid_argument(result.spherical.message);
    if (request.q_mev >= .01 && request.preformation > 0)
        result.external_reference = reference;
    return result;
}
}
