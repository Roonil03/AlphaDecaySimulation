#include "alpha/core.hpp"
#include "channel_json.hpp"
#include "alpha/spherical_wkb.hpp"
#include "alpha/spherical_model.hpp"
#include "embedded.hpp"
#include "viewer.hpp"
#include "numerical_analysis.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <nlohmann/json.hpp>
#include <set>
#include <string>
using json = nlohmann::json;

static int integer_field(const json &j, const char *name) {
  const auto &value = j.at(name);
  const std::string error = std::string(name) + " must be an integer in int range";
  if (value.is_number_unsigned()) {
    const auto n = value.get<json::number_unsigned_t>();
    if (n > static_cast<json::number_unsigned_t>(std::numeric_limits<int>::max()))
      throw std::invalid_argument(error);
    return static_cast<int>(n);
  }
  if (!value.is_number_integer())
    throw std::invalid_argument(error);
  const auto n = value.get<json::number_integer_t>();
  if (n < std::numeric_limits<int>::min() || n > std::numeric_limits<int>::max())
    throw std::invalid_argument(error);
  return static_cast<int>(n);
}

static alpha::Request parse(const json &j) {
  const std::set<std::string> allowed = {"schema_version",
                                         "parent_z",
                                         "parent_a",
                                         "q_mev",
                                         "l",
                                         "radius_fm",
                                         "preformation",
                                         "assault_hz",
                                         "research_model",
                                         "parent_binding_mev", "potential_model",
                                         "nuclear_depth_mev", "nuclear_radius_fm",
                                         "diffuseness_fm", "charge_radius_fm",
"centrifugal_convention", "numerov", "uncertainty", "deformation", "parent_spin", "channels", "competing_rate_hz"};
  if (!j.is_object())
    throw std::invalid_argument("request must be an object");
  for (auto it = j.begin(); it != j.end(); ++it)
    if (!allowed.contains(it.key()))
      throw std::invalid_argument("unknown field: " + it.key());
  if (integer_field(j, "schema_version") != 1)
    throw std::invalid_argument("unsupported schema_version");
  alpha::Request p;
  p.parent_z = integer_field(j, "parent_z");
  p.parent_a = integer_field(j, "parent_a");
  p.q_mev = j.at("q_mev").get<double>();
  p.l = integer_field(j, "l");
  p.radius_fm = j.at("radius_fm").get<double>();
  p.preformation = j.at("preformation").get<double>();
  p.assault_hz = j.at("assault_hz").get<double>();
  p.research_model = j.value("research_model", false);
  if (j.contains("parent_binding_mev") && !j["parent_binding_mev"].is_null())
    p.parent_binding_mev = j["parent_binding_mev"].get<double>();
  return p;
}
static json serialize(const alpha::Result &r) {
  json j = {{"schema_version", 1},
            {"log10_half_life_s", r.log10_half_life_s},
            {"log10_penetrability", r.log10_penetrability},
            {"outer_turning_fm", r.outer_turning_fm}};
  if (r.research_log10_half_life_s)
    j["research_log10_half_life_s"] = *r.research_log10_half_life_s;
  if (r.shell_log10_half_life_s)
    j["shell_log10_half_life_s"] = *r.shell_log10_half_life_s;
  return j;
}
static json run_request(const json &j) {
  auto p = parse(j);
  const auto model = j.value("potential_model", std::string("external"));
  if (model == "external") {
for (const char *key : {"deformation", "parent_spin", "channels", "competing_rate_hz", "numerov", "uncertainty"})
      if (j.contains(key)) throw std::invalid_argument(std::string(key) + " requires a spherical potential model");

    for (const char *key : {"nuclear_depth_mev", "nuclear_radius_fm", "diffuseness_fm", "charge_radius_fm", "centrifugal_convention"})
      if (j.contains(key)) throw std::invalid_argument(std::string(key) + " requires a spherical potential model");
    return serialize(alpha::calculate(p));
  }
  if (model != "square_well" && model != "woods_saxon")
    throw std::invalid_argument("unknown potential_model");
  alpha::SphericalPotential v;
  v.kind = model == "square_well" ? alpha::PotentialKind::square_well : alpha::PotentialKind::woods_saxon;
  v.nuclear_depth_mev = j.value("nuclear_depth_mev", 100.0);
  v.nuclear_radius_fm = j.value("nuclear_radius_fm", p.radius_fm);
  v.diffuseness_fm = j.value("diffuseness_fm", .65);
  v.charge_radius_fm = j.value("charge_radius_fm", p.radius_fm);
  const auto convention = j.value("centrifugal_convention", std::string("physical"));
  if (convention == "langer_wkb") v.centrifugal = alpha::CentrifugalConvention::langer_wkb;
  else if (convention != "physical") throw std::invalid_argument("unknown centrifugal_convention");
  auto calculation=alpha::calculate_spherical_model(p, v);
  const bool channel_mode=j.contains("deformation") || j.contains("channels") || j.contains("parent_spin") || j.contains("competing_rate_hz");
  if(channel_mode) {
    json channel_input=j;
    if(!channel_input.contains("channels")) channel_input["channels"]=json::array({{{"name","ground"},{"l",p.l}}});
    auto input=alpha::make_spherical_wkb_request(p,v);
    json out=run_channels(channel_input,input);
    out["schema_version"]=1; out["potential_model"]=model; out["centrifugal_convention"]=convention;
    // Each returned numerical comparison is a spherical reference for the
    // selected channel, not a deformed transmission or a total-width result.
    if(j.contains("numerov") || j.contains("uncertainty")) {
      for (std::size_t i=0;i<out["channels"].size();++i) {
        auto& row=out["channels"][i]; const auto& spec=channel_input["channels"][i];
        if(row["status"]!="ok" || row["l"].is_null()) continue;
        auto channel=input; channel.q_mev=row["q_mev"].get<double>(); channel.potential.l=row["l"].get<int>();
        channel.preformation=spec.value("preformation",input.preformation); channel.assault_hz=spec.value("assault_hz",input.assault_hz);
        json analysis; append_numerical_analyses(j,channel,analysis); analysis["scope"]="spherical_channel_reference"; row["numerical_analysis"]=analysis;
      }
    }
    return out;
  }
  const auto &r=calculation.spherical;
  json out={{"schema_version",1},{"potential_model",model},{"centrifugal_convention",convention},
            {"status",r.status},{"message",r.message}};
  const auto put=[&](const char *key, const std::optional<double>& x) { out[key]=x ? json(*x) : json(nullptr); };
  put("inner_turning_fm",r.inner_turning_fm); put("outer_turning_fm",r.outer_turning_fm);
  put("action",r.action); put("log10_penetrability",r.log10_penetrability); put("log10_half_life_s",r.log10_half_life_s);
  // Only present a reference for the actual input, never a substituted status check.
  if (calculation.external_reference) out["external_reference"] = serialize(*calculation.external_reference);
  v.daughter_z=p.parent_z-2; v.reduced_mass_mev=931.49410242*4.0*(p.parent_a-4.0)/p.parent_a; v.l=p.l;
  alpha::SphericalWkbRequest input; input.potential=v; input.q_mev=p.q_mev; input.preformation=p.preformation; input.assault_hz=p.assault_hz;
  append_numerical_analyses(j,input,out);
  return out;
}
static void check(bool b, const std::string &what) {
  if (!b)
    throw std::runtime_error(what);
}
static void self_test() {
  auto catalog = json::parse(embedded::catalog);
  check(catalog.size() == 16, "catalog size");
  int count = 0;
  for (auto data : {embedded::validation, embedded::holdout}) {
    auto report = json::parse(data);
    auto inputs = report.at("inputs");
    for (auto &row : report.at("nuclides")) {
      auto it = std::find_if(catalog.begin(), catalog.end(), [&](auto &x) {
        return x.at("symbol").template get<std::string>() + "-" +
                   std::to_string(x.at("a").template get<int>()) ==
               row.at("isotope").get<std::string>();
      });
      check(it != catalog.end(), "missing catalog row");
      alpha::Request p;
      p.parent_z = it->at("z");
      p.parent_a = it->at("a");
      p.q_mev = it->at("q_mev");
      p.l = inputs.at("l");
      p.radius_fm = inputs.at("radius_fm");
      p.preformation = inputs.at("preformation");
      p.assault_hz = inputs.at("assault_hz");
      p.parent_binding_mev = it->at("parent_binding_mev").get<double>();
      auto r = alpha::calculate(p);
      check(std::abs(r.log10_half_life_s -
                     row.at("baseline_log10_s").get<double>()) < 1e-7,
            "WKB reference");
      check(r.research_log10_half_life_s &&
                std::abs(*r.research_log10_half_life_s -
                         row.at("research_log10_s").get<double>()) < 1e-9,
            "Qi reference");
      check(r.shell_log10_half_life_s &&
                std::abs(*r.shell_log10_half_life_s -
                         row.at("shell_log10_s").get<double>()) < 1e-9,
            "Ren reference");
      check(std::abs(std::log10(it->at("partial_half_life_s").get<double>()) -
                     row.at("reference_log10_s").get<double>()) < 1e-12,
            "measured reference");
      ++count;
    }
  }
  check(count == 15, "reference count");
  auto p = alpha::Request{};
  p.preformation = 0;
  bool rejected = false;
  try {
    alpha::calculate(p);
  } catch (const std::invalid_argument &) {
    rejected = true;
  }
  check(rejected, "validation");
  std::cout << "Self-test passed: 15 channels, 45 model references.\n";
}
int main(int argc, char **argv) {
  try {
    if (argc == 2 && std::string(argv[1]) == "--run") {
      json j;
      std::cin >> j;
      std::cout << run_request(j).dump() << '\n';
      return 0;
    }
    if (argc == 2 && std::string(argv[1]) == "--self-test") {
      self_test();
      return 0;
    }
    if (argc == 2 && std::string(argv[1]) == "--credits") {
      std::cout << "GLFW\n"
                << embedded::glfw_license << "\nDear ImGui\n"
                << embedded::imgui_license << "\nnlohmann/json\n"
                << embedded::json_license << "\nKhronos OpenGL definitions\n"
                << embedded::khronos_license << "\nProggyClean font\n"
                << embedded::proggyclean_license << '\n';
      return 0;
    }
    return viewer(argc, argv);
  } catch (const std::exception &e) {
    std::cerr << json({{"schema_version", 1}, {"error", e.what()}}).dump()
              << '\n';
    return 1;
  }
}





