#include "fit_import.hpp"
#include "alpha/spherical_model.hpp"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <limits>
#include <set>
#include <stdexcept>
#include <string>

using json = nlohmann::json;
namespace {
int integer(const json &object, const char *key) {
  const auto &value = object.at(key);
  const std::string error = std::string(key) + " must be an integer in int range";
  if (value.is_number_unsigned()) {
    const auto number = value.get<json::number_unsigned_t>();
    if (number > static_cast<json::number_unsigned_t>(std::numeric_limits<int>::max()))
      throw std::invalid_argument(error);
    return static_cast<int>(number);
  }
  if (!value.is_number_integer()) throw std::invalid_argument(error);
  const auto number = value.get<json::number_integer_t>();
  if (number < std::numeric_limits<int>::min() || number > std::numeric_limits<int>::max())
    throw std::invalid_argument(error);
  return static_cast<int>(number);
}

double number(const json &object, const char *key) {
  const auto &value = object.at(key);
  if (!value.is_number() || !std::isfinite(value.get<double>()))
    throw std::invalid_argument(std::string(key) + " must be a finite number");
  return value.get<double>();
}

bool nonblank(const std::string &value) {
  return std::any_of(value.begin(), value.end(), [](unsigned char c) { return !std::isspace(c); });
}
}

ImportedFitRequest parse_fit_request(const json &r, const json &fit) {
  if (!r.is_object()) throw std::invalid_argument("Invalid fit request");
  const std::set<std::string> allowed = {
      "schema_version", "parent_z", "parent_a", "q_mev", "l", "radius_fm",
      "preformation", "assault_hz", "research_model", "parent_binding_mev",
      "potential_model", "centrifugal_convention", "nuclear_depth_mev",
      "nuclear_radius_fm", "diffuseness_fm", "charge_radius_fm"};
  for (auto it = r.begin(); it != r.end(); ++it)
    if (!allowed.contains(it.key()))
      throw std::invalid_argument("Unsupported fit request field: " + it.key());
  if (r.contains("schema_version") && integer(r, "schema_version") != 1)
    throw std::invalid_argument("Unsupported fit request schema");
  ImportedFitRequest result;
  auto &p = result.request;
  p.parent_z = integer(r, "parent_z");
  p.parent_a = integer(r, "parent_a");
  p.l = integer(r, "l");
  p.q_mev = number(r, "q_mev");
  p.radius_fm = number(r, "radius_fm");
  p.preformation = number(fit, "preformation");
  p.assault_hz = number(fit, "assault_hz");
  // Dataset rate parameters are superseded by the common fitted normalization.
  for (const char *key : {"preformation", "assault_hz"})
    if (r.contains(key)) number(r, key);
  p.research_model = r.value("research_model", false);
  if (r.contains("parent_binding_mev") && !r.at("parent_binding_mev").is_null())
    p.parent_binding_mev = number(r, "parent_binding_mev");
  const auto kind = r.value("potential_model", std::string("external"));
  if (kind != "external" && kind != "square_well" && kind != "woods_saxon")
    throw std::invalid_argument("Unsupported fit potential");
  const auto convention = r.value("centrifugal_convention", std::string("physical"));
  if (convention != "physical" && convention != "langer_wkb")
    throw std::invalid_argument("Unsupported centrifugal convention");
  result.potential_model = kind == "external" ? 0 : kind == "square_well" ? 1 : 2;
  result.convention = convention == "physical" ? 0 : 1;
  result.nuclear_radius = result.charge_radius = p.radius_fm;
  if (r.contains("nuclear_depth_mev")) result.depth = number(r, "nuclear_depth_mev");
  if (r.contains("nuclear_radius_fm")) result.nuclear_radius = number(r, "nuclear_radius_fm");
  if (r.contains("diffuseness_fm")) result.diffuseness = number(r, "diffuseness_fm");
  if (r.contains("charge_radius_fm")) result.charge_radius = number(r, "charge_radius_fm");
  if (result.potential_model == 0) {
    for (const char *key : {"nuclear_depth_mev", "nuclear_radius_fm", "diffuseness_fm", "charge_radius_fm", "centrifugal_convention"})
      if (r.contains(key)) throw std::invalid_argument(std::string(key) + " requires a spherical potential model");
    alpha::calculate(p);
  } else {
    alpha::SphericalPotential potential;
    potential.kind = result.potential_model == 1 ? alpha::PotentialKind::square_well : alpha::PotentialKind::woods_saxon;
    potential.centrifugal = result.convention == 0 ? alpha::CentrifugalConvention::physical : alpha::CentrifugalConvention::langer_wkb;
    potential.nuclear_depth_mev = result.depth;
    potential.nuclear_radius_fm = result.nuclear_radius;
    potential.diffuseness_fm = result.diffuseness;
    potential.charge_radius_fm = result.charge_radius;
    alpha::calculate_spherical_model(p, potential);
  }
  return result;
}

void validate_fit_import(const json &fit) {
  if (!fit.is_object() || integer(fit, "schema_version") != 1 ||
      !fit.at("cases").is_array() || !fit.at("calibration_ids").is_array())
    throw std::invalid_argument("Unsupported fit schema");
  if (!fit.at("dataset_version").is_string() || !nonblank(fit.at("dataset_version").get<std::string>()))
    throw std::invalid_argument("Invalid dataset version");
  const double preformation = number(fit, "preformation"), hz = number(fit, "assault_hz");
  if (preformation <= 0 || preformation > 1 || hz < 1e-300 || hz > 1e30)
    throw std::invalid_argument("Invalid fitted rate parameters");
  std::set<std::string> ids, calibration;
  for (const auto &item : fit.at("calibration_ids")) {
    const auto id = item.get<std::string>();
    if (!nonblank(id) || !calibration.insert(id).second)
      throw std::invalid_argument("Invalid calibration ID");
  }
  for (const auto &row : fit.at("cases")) {
    const auto id = row.at("id").get<std::string>();
    const auto split = row.at("split").get<std::string>();
    if (!nonblank(id) || !ids.insert(id).second ||
        (split != "calibration" && split != "validation") ||
        (calibration.contains(id) != (split == "calibration")))
      throw std::invalid_argument("Invalid fit case membership");
    for (const char *key : {"log10_half_life_s", "baseline_log10_half_life_s", "predicted_log10_half_life_s", "residual"})
      if (row.contains(key)) number(row, key);
    parse_fit_request(row.at("request"), fit);
  }
  if (calibration.empty() || !std::all_of(calibration.begin(), calibration.end(), [&](const auto &id) { return ids.contains(id); }))
    throw std::invalid_argument("Invalid calibration IDs");
}

void load_fit_import(json candidate, json &current) {
  validate_fit_import(candidate);
  current = std::move(candidate);
}
