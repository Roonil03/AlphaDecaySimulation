#include "channel_json.hpp"
#include "alpha/channels.hpp"
#include "alpha/deformation.hpp"
#include <set>
#include <limits>
#include <algorithm>
#include <cmath>
#include <stdexcept>
using json=nlohmann::json;
namespace {
void keys(const json& j, const std::set<std::string>& allowed) { if(!j.is_object()) throw std::invalid_argument("expected an object"); for(auto it=j.begin();it!=j.end();++it) if(!allowed.contains(it.key())) throw std::invalid_argument("unknown field: "+it.key()); }
int exact_int(const json& v) {
 if(v.is_number_unsigned()) { auto n=v.get<json::number_unsigned_t>(); if(n>static_cast<json::number_unsigned_t>(std::numeric_limits<int>::max())) throw std::invalid_argument("integer out of range"); return static_cast<int>(n); }
 if(!v.is_number_integer()) throw std::invalid_argument("expected integer");
 auto n=v.get<json::number_integer_t>(); if(n<std::numeric_limits<int>::min()||n>std::numeric_limits<int>::max()) throw std::invalid_argument("integer out of range"); return static_cast<int>(n);
}
std::optional<alpha::SpinParity> spin(const json& j) { if(j.is_null())return {}; keys(j,{"twice_j","parity"}); return alpha::SpinParity{exact_int(j.at("twice_j")),exact_int(j.at("parity"))}; }
json opt(const std::optional<double>& v) { return v ? json(*v) : json(nullptr); }
}
json run_channels(const json& input,const alpha::SphericalWkbRequest& base, const std::function<bool()>& cancelled) {
 alpha::Deformation d;
 if(input.contains("deformation")) { const auto& x=input.at("deformation"); keys(x,{"beta2","beta4"}); d.beta2=x.value("beta2",0.0); d.beta4=x.value("beta4",0.0); }
 (void)alpha::deformed_radius(1,d,0);
 std::optional<alpha::SpinParity> parent; if(input.contains("parent_spin")) parent=spin(input.at("parent_spin"));
 std::optional<double> competing; if(input.contains("competing_rate_hz")&&!input.at("competing_rate_hz").is_null()) competing=input.at("competing_rate_hz").get<double>();
 if(!input.contains("channels")||!input.at("channels").is_array()||input.at("channels").empty()) throw std::invalid_argument("channels must be a nonempty array");
 std::vector<alpha::ChannelInput> cs; std::vector<alpha::ChannelSelection> selections; std::vector<std::optional<double>> logs; json rows=json::array();
 for(const auto& x:input.at("channels")) {
  if(cancelled && cancelled()) throw std::runtime_error("cancelled");
  keys(x,{"name","excitation_mev","daughter_spin","l","preformation","assault_hz"});
  alpha::ChannelInput c; c.name=x.value("name",std::string()); c.excitation_mev=x.value("excitation_mev",0.0); c.preformation=x.value("preformation",base.preformation); c.assault_hz=x.value("assault_hz",base.assault_hz);
  if(x.contains("daughter_spin")) c.daughter=spin(x.at("daughter_spin")); if(x.contains("l")&&!x.at("l").is_null()) { c.l=exact_int(x.at("l")); }
  auto selection=alpha::select_channel(base.q_mev,parent,c); json row={{"name",c.name},{"q_mev",selection.q_mev},{"l",selection.l?json(*selection.l):json(nullptr)}}; std::optional<double> logp;
  if(selection.status=="ok") {
   auto req=base; req.q_mev=selection.q_mev; req.potential.l=*selection.l; req.preformation=c.preformation; req.assault_hz=c.assault_hz;
   auto r=alpha::calculate_deformed_wkb(req,d,cancelled); if(r.status=="cancelled") throw std::runtime_error("cancelled"); row["barrier_status"]=r.status; row["barrier_message"]=r.message; row["angular_error_log10"]=r.angular_error_log10; row["charge_error_log10"]=r.charge_error_log10; row["coulomb_error_mev"]=r.coulomb_error_mev; row["angular_points"]=r.angular_points; row["multipole_order"]=r.multipole_order;
   logp=r.log10_penetrability; if(!logp) selection.status=r.status;
  }
  row["log10_penetrability"]=opt(logp); rows.push_back(row); cs.push_back(c); selections.push_back(selection); logs.push_back(logp);
 }
 auto rates=alpha::channel_rates(cs,selections,logs,competing);
 for(size_t i=0;i<rates.size();++i) { rows[i]["status"]=rates[i].status; rows[i]["log10_rate_hz"]=opt(rates[i].log10_rate_hz); rows[i]["log10_partial_half_life_s"]=opt(rates[i].log10_partial_half_life_s); rows[i]["conditional_alpha_branch"]=opt(rates[i].conditional_alpha_branch); rows[i]["total_branch"]=opt(rates[i].total_branch); }
 bool complete=std::all_of(rates.begin(),rates.end(),[](const alpha::ChannelRate& r){return r.status=="ok" || r.status=="closed" || r.status=="forbidden" || r.status=="zero_preformation";});
 std::optional<double> alpha_log, total_log, total_half;
 if(complete) {
  double largest=-INFINITY, scaled=0;
  for(const auto& r:rates) if(r.log10_rate_hz) largest=std::max(largest,*r.log10_rate_hz);
  if(std::isfinite(largest)) {for(const auto& r:rates) if(r.log10_rate_hz) scaled+=std::pow(10.0,*r.log10_rate_hz-largest); alpha_log=largest+std::log10(scaled);}
  if(competing && *competing>0) {double c=std::log10(*competing); if(alpha_log) {double m=std::max(c,*alpha_log); total_log=m+std::log10(std::pow(10.0,c-m)+std::pow(10.0,*alpha_log-m));} else total_log=c;}
  else total_log=alpha_log;
  if(total_log) total_half=std::log10(std::log(2.0))-*total_log;
 }
 return {{"channels",rows},{"branch_convention",competing?"conditional_alpha_and_total":"conditional_alpha"},
         {"log10_total_alpha_rate_hz",opt(alpha_log)},{"log10_total_rate_hz",opt(total_log)},{"log10_total_half_life_s",opt(total_half)}};
}
