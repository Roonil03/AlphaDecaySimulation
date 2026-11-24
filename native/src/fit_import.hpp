#pragma once
#include "alpha/core.hpp"
#include <nlohmann/json.hpp>

// Parse into a temporary value so a rejected fit cannot partly change controls.
struct ImportedFitRequest {
  alpha::Request request;
  int potential_model = 0, convention = 0;
  double depth = 100, nuclear_radius = 9, diffuseness = .65, charge_radius = 9;
};

ImportedFitRequest parse_fit_request(const nlohmann::json &request,
                                    const nlohmann::json &fit);
void validate_fit_import(const nlohmann::json &fit);
void load_fit_import(nlohmann::json candidate, nlohmann::json &current);
