#include "fit_import.hpp"
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

using json = nlohmann::json;

static void require(bool condition, const char *message) {
  if (!condition) throw std::runtime_error(message);
}

static json fixture() {
  return json::parse(R"({
    "schema_version": 1, "dataset_version": "test-v1",
    "preformation": 0.02, "assault_hz": 1e21, "calibration_ids": ["fit"],
    "cases": [{"id": "fit", "split": "calibration", "request": {
      "parent_z": 84, "parent_a": 212, "q_mev": 8.9542, "l": 0, "radius_fm": 9
    }}]
  })");
}

static void rejected_atomically(const json &candidate) {
  json current = fixture();
  const auto before = current;
  bool rejected = false;
  try { load_fit_import(candidate, current); }
  catch (const std::exception &) { rejected = true; }
  require(rejected, "invalid fit accepted");
  require(current == before, "rejected fit changed the current import");
}

int main() {
  try {
    json current;
    load_fit_import(fixture(), current);
    const auto imported = parse_fit_request(current["cases"][0]["request"], current);
    require(imported.request.parent_z == 84 && imported.request.preformation == .02,
            "valid fit request changed");
    for (const char *key : {"schema_version", "parent_z", "parent_a", "l"}) {
      for (const auto &value : {json(1.0), json(true), json("1"), json(nullptr),
                                json(2147483648ULL), json(-2147483649LL)}) {
        auto candidate = fixture();
        candidate["cases"][0]["request"][key] = value;
        rejected_atomically(candidate);
      }
    }
    for (const auto &value : {json(1.0), json(true), json(2), json(nullptr)}) {
      auto candidate = fixture();
      candidate["schema_version"] = value;
      rejected_atomically(candidate);
    }
    for (const char *key : {"parent_z", "parent_a", "l", "q_mev", "radius_fm"}) {
      auto candidate = fixture();
      candidate["cases"][0]["request"].erase(key);
      rejected_atomically(candidate);
    }
    for (const auto &change : {json{{"parent_z", 2}}, json{{"l", -1}},
                              json{{"q_mev", "9"}}, json{{"radius_fm", false}},
                              json{{"channels", json::array()}},
                              json{{"potential_model", "unsupported"}},
                              json{{"potential_model", "square_well"}, {"nuclear_depth_mev", -1}}}) {
      auto candidate = fixture();
      candidate["cases"][0]["request"].update(change);
      rejected_atomically(candidate);
    }
    for (const auto &change : {json{{"cases", nullptr}}, json{{"calibration_ids", {"absent"}}},
                              json{{"preformation", true}}, json{{"preformation", 0}},
                              json{{"assault_hz", std::numeric_limits<double>::infinity()}},
                              json{{"dataset_version", "  "}}}) {
      auto candidate = fixture();
      candidate.update(change);
      rejected_atomically(candidate);
    }
    // Every row is checked before replacing a previously valid import.
    auto candidate = fixture();
    auto bad = candidate["cases"][0];
    bad["id"] = "bad"; bad["split"] = "validation";
    bad["request"]["parent_a"] = 212.5;
    candidate["cases"].push_back(bad);
    rejected_atomically(candidate);
    for (const char *kind : {"square_well", "woods_saxon"}) {
      auto valid = fixture();
      auto &request = valid["cases"][0]["request"];
      request.update({{"schema_version", 1}, {"potential_model", kind},
                      {"centrifugal_convention", "langer_wkb"}, {"nuclear_depth_mev", 100},
                      {"research_model", true}, {"parent_binding_mev", nullptr}});
      load_fit_import(valid, current);
      require(parse_fit_request(request, valid).convention == 1, "valid convention lost");
    }
    std::cout << "fit import integer, domain and atomic validation passed\n";
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
