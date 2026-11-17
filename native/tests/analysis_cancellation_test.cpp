#include "alpha/numerov.hpp"
#include "alpha/uncertainty.hpp"
#include "alpha/deformation.hpp"
#include "channel_json.hpp"
#include <atomic>
#include <stdexcept>
void check(bool b) {if(!b) throw std::runtime_error("cancellation integration failed");}
int main() {
    alpha::SphericalWkbRequest input; std::atomic_bool stop{true};
    check(alpha::calculate_numerov(input,{},&stop).status=="cancelled");
    alpha::UncertaintyRequest u; u.baseline=input; check(alpha::sample_spherical_uncertainty(u,[]{return true;}).status=="cancelled");
    check(alpha::calculate_deformed_wkb(input,{.2,0},[]{return true;}).status=="cancelled");
    bool cancelled=false; try { run_channels({{"channels",nlohmann::json::array({{{"name","ground"},{"l",0}}})}},input,[]{return true;}); } catch(const std::runtime_error&) {cancelled=true;} check(cancelled);
}
