#include "alpha/channels.hpp"
#include "alpha/deformation.hpp"
#include <stdexcept>
#include <source_location>
#include <cmath>
#include <iostream>
using namespace alpha;
void check(bool value, const std::source_location& where = std::source_location::current()) {
 if (!value) throw std::runtime_error("channel/deformation check failed at line " + std::to_string(where.line()));
}
int main(){
 ChannelInput a; a.name="ground"; a.daughter=SpinParity{0,1};
 auto sel=select_channel(5,SpinParity{0,1},a); check(sel.status=="ok" && *sel.l==0);
 ChannelInput b=a; b.excitation_mev=1; b.daughter=SpinParity{4,1}; auto sb=select_channel(5,SpinParity{0,1},b); check(sb.status=="ok"&&*sb.l==2);
 b.l=1; check(select_channel(5,SpinParity{0,1},b).status=="forbidden");
 b.excitation_mev=5; check(select_channel(5,SpinParity{0,1},b).status=="closed");
 b.excitation_mev=1; b.daughter.reset(); b.l.reset(); check(select_channel(5,SpinParity{0,1},b).status=="unknown_spin_requires_l");
 std::vector<ChannelInput> cs{a,a,a}; cs[2].preformation=0; std::vector<ChannelSelection> ss{sel,sel,sel};
 auto rates=channel_rates(cs,ss,{-300.,-301.,-300.},1e-280); check(std::abs(*rates[0].conditional_alpha_branch+*rates[1].conditional_alpha_branch-1)<1e-12); check(rates[2].status=="zero_preformation"); check(*rates[0].total_branch<*rates[0].conditional_alpha_branch);
 auto closed=sel; closed.status="closed"; auto zero=channel_rates({a},{closed},{std::nullopt},1.0); check(!zero[0].conditional_alpha_branch && *zero[0].total_branch==0);
 auto unavailable=channel_rates({a},{sel},{std::nullopt}); check(unavailable[0].status=="numerical_error" && !unavailable[0].total_branch && !unavailable[0].conditional_alpha_branch);
 auto zero_factor=a; zero_factor.preformation=0;
 auto known_zero=channel_rates({zero_factor,a},{sel,sel},{std::nullopt,-30.});
 check(known_zero[0].status=="zero_preformation" && !known_zero[0].log10_rate_hz);
 check(*known_zero[0].conditional_alpha_branch==0 && *known_zero[1].conditional_alpha_branch==1);
 zero_factor.assault_hz=0;
 bool invalid_rate=false; try { channel_rates({zero_factor},{sel},{std::nullopt}); } catch(const std::invalid_argument&) { invalid_rate=true; }
 check(invalid_rate);
 SphericalWkbRequest req; auto s=calculate_spherical_wkb(req); auto d=calculate_deformed_wkb(req,{}); check(s.status==d.status); check(s.log10_penetrability==d.log10_penetrability); check(std::abs(deformed_radius(7,{},0.3)-7)<1e-12); check(std::abs(deformed_coulomb(req.potential,{},8,0.3)-coulomb_potential(req.potential,8))<1e-12);
 check(calculate_deformed_wkb(req,{0.2,0.05},[]{return true;}).status=="cancelled");
 Deformation shape{0.2,0.05}; double volume=0; for(int i=0;i<10000;i++){double r=deformed_radius(7,shape,(i+0.5)/10000.); volume+=r*r*r/10000;} check(std::abs(volume-343)<1e-5);
 auto deformed=calculate_deformed_wkb(req,shape); std::cerr<<"deformed status="<<deformed.status<<" angular="<<deformed.angular_error_log10<<" coulomb="<<deformed.coulomb_error_mev<<" charge log="<<deformed.charge_error_log10<<" "<<deformed.message<<"\n"; check(deformed.status=="ok"); check(deformed.angular_error_log10<1e-4); check(deformed.charge_error_log10<1e-4);
 req.potential.kind=PotentialKind::square_well;
 req.potential.nuclear_radius_fm=9; req.potential.charge_radius_fm=9;
 req.potential.diffuseness_fm=.65;
 const auto wide=calculate_deformed_wkb(req,{.2,0});
 req.potential.diffuseness_fm=.01;
 const auto narrow=calculate_deformed_wkb(req,{.2,0});
 check(wide.status=="ok" && narrow.status=="ok");
 check(std::abs(*wide.log10_penetrability-*narrow.log10_penetrability)<1e-7);
 std::cout<<"channel and deformation tests passed\n";
}
