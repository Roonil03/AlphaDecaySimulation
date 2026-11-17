#include "alpha/channels.hpp"
#include "alpha/deformation.hpp"
#include <cassert>
#include <cmath>
#include <iostream>
using namespace alpha;
int main(){
 ChannelInput a; a.name="ground"; a.daughter=SpinParity{0,1};
 auto sel=select_channel(5,SpinParity{0,1},a); assert(sel.status=="ok" && *sel.l==0);
 ChannelInput b=a; b.excitation_mev=1; b.daughter=SpinParity{4,1}; auto sb=select_channel(5,SpinParity{0,1},b); assert(sb.status=="ok"&&*sb.l==2);
 b.l=1; assert(select_channel(5,SpinParity{0,1},b).status=="forbidden");
 b.excitation_mev=5; assert(select_channel(5,SpinParity{0,1},b).status=="closed");
 b.excitation_mev=1; b.daughter.reset(); b.l.reset(); assert(select_channel(5,SpinParity{0,1},b).status=="unknown_spin_requires_l");
 std::vector<ChannelInput> cs{a,a,a}; cs[2].preformation=0; std::vector<ChannelSelection> ss{sel,sel,sel};
 auto rates=channel_rates(cs,ss,{-300.,-301.,-300.},1e-280); assert(std::abs(rates[0].conditional_alpha_branch+rates[1].conditional_alpha_branch-1)<1e-12); assert(rates[2].status=="zero_preformation"); assert(*rates[0].total_branch<rates[0].conditional_alpha_branch);
 SphericalWkbRequest req; auto s=calculate_spherical_wkb(req); auto d=calculate_deformed_wkb(req,{}); assert(s.status==d.status); assert(s.log10_penetrability==d.log10_penetrability); assert(std::abs(deformed_radius(7,{},0.3)-7)<1e-12); assert(std::abs(deformed_coulomb(req.potential,{},8,0.3)-coulomb_potential(req.potential,8))<1e-12);
 Deformation shape{0.2,0.05}; double volume=0; for(int i=0;i<10000;i++){double r=deformed_radius(7,shape,(i+0.5)/10000.); volume+=r*r*r/10000;} assert(std::abs(volume-343)<1e-5);
 auto deformed=calculate_deformed_wkb(req,shape); std::cerr<<"deformed status="<<deformed.status<<" angular="<<deformed.angular_error_log10<<" coulomb="<<deformed.coulomb_error_mev<<" charge log="<<deformed.charge_error_log10<<" "<<deformed.message<<"\n"; assert(deformed.status=="ok"); assert(deformed.angular_error_log10<1e-4); assert(deformed.charge_error_log10<1e-4);
 std::cout<<"channel and deformation tests passed\n";
}

