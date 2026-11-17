#include "alpha/core.hpp"
#include "embedded.hpp"
#include "viewer.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <nlohmann/json.hpp>
#include <set>
#include <string>
using json = nlohmann::json;

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
                                         "parent_binding_mev"};
  if (!j.is_object())
    throw std::invalid_argument("request must be an object");
  for (auto it = j.begin(); it != j.end(); ++it)
    if (!allowed.contains(it.key()))
      throw std::invalid_argument("unknown field: " + it.key());
  if (j.at("schema_version").get<int>() != 1)
    throw std::invalid_argument("unsupported schema_version");
  alpha::Request p;
  p.parent_z = j.at("parent_z").get<int>();
  p.parent_a = j.at("parent_a").get<int>();
  p.q_mev = j.at("q_mev").get<double>();
  p.l = j.at("l").get<int>();
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
static void check(bool b, const std::string &what) {
  if (!b)
    throw std::runtime_error(what);
}
static void self_test() {
  auto catalog = json::parse(embedded::catalog);
  check(catalog.size() == 15, "catalog size");
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
      std::cout << serialize(alpha::calculate(parse(j))).dump() << '\n';
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
                << embedded::khronos_license << '\n';
      return 0;
    }
    return viewer(argc, argv);
  } catch (const std::exception &e) {
    std::cerr << json({{"schema_version", 1}, {"error", e.what()}}).dump()
              << '\n';
    return 1;
  }
}
