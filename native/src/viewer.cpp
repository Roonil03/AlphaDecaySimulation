#include "viewer.hpp"
#include "alpha/core.hpp"
#include "alpha/deformation.hpp"
#include "channel_json.hpp"
#include "alpha/spherical_wkb.hpp"

#include "alpha/uncertainty.hpp"
#ifdef ALPHA_ENABLE_NUMEROV
#include "alpha/numerov.hpp"
#endif
#include <future>
#include <memory>
#include <chrono>
#include <atomic>

#include "alpha/spherical_model.hpp"

#include "embedded.hpp"
#include <glad/gl.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <nlohmann/json.hpp>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cfloat>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <random>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
using json = nlohmann::json;
namespace {
std::string time_label(double log_seconds) {
  struct Unit {
    const char *name;
    double seconds;
  };
  const Unit units[] = {{"years", 31557600.0},
                        {"days", 86400.0},
                        {"hours", 3600.0},
                        {"minutes", 60.0},
                        {"s", 1.0}};
  for (const auto &unit : units) {
    double value = log_seconds - std::log10(unit.seconds);
    if (value >= 0 || unit.seconds == 1.0) {
      std::ostringstream out;
      if (value > 100 || value < -100)
        out << "10^" << value;
      else
        out << std::setprecision(5) << std::pow(10.0, value);
      out << ' ' << unit.name;
      return out.str();
    }
  }
  return {};
}
// A small PNG encoder using uncompressed DEFLATE blocks: no runtime image
// dependency.
uint32_t crc(const unsigned char *p, size_t n) {
  uint32_t c = ~0u;
  while (n--) {
    c ^= *p++;
    for (int k = 0; k < 8; k++)
      c = (c >> 1) ^ (0xedb88320u & (0u - (c & 1)));
  }
  return ~c;
}
void word(std::vector<unsigned char> &v, uint32_t x) {
  for (int s = 24; s >= 0; s -= 8)
    v.push_back((x >> s) & 255);
}
void chunk(std::vector<unsigned char> &out, const char *name,
           const std::vector<unsigned char> &data) {
  word(out, (uint32_t)data.size());
  auto start = out.size();
  out.insert(out.end(), name, name + 4);
  out.insert(out.end(), data.begin(), data.end());
  word(out, crc(out.data() + start, out.size() - start));
}
void png(const char *path, int w, int h) {
  std::vector<unsigned char> pixels(size_t(w) * h * 3), raw;
  glPixelStorei(GL_PACK_ALIGNMENT, 1);
  glReadPixels(0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
  for (int y = h - 1; y >= 0; --y) {
    raw.push_back(0);
    raw.insert(raw.end(), pixels.begin() + size_t(y) * w * 3,
               pixels.begin() + size_t(y + 1) * w * 3);
  }
  std::vector<unsigned char> z = {0x78, 0x01};
  size_t pos = 0;
  while (pos < raw.size()) {
    unsigned n = (unsigned)std::min<size_t>(65535, raw.size() - pos);
    z.push_back(pos + n == raw.size() ? 1 : 0);
    z.push_back(n & 255);
    z.push_back(n >> 8);
    unsigned inv = (~n) & 65535;
    z.push_back(inv & 255);
    z.push_back(inv >> 8);
    z.insert(z.end(), raw.begin() + pos, raw.begin() + pos + n);
    pos += n;
  }
  uint32_t a = 1, b = 0;
  for (auto c : raw) {
    a = (a + c) % 65521;
    b = (b + a) % 65521;
  }
  word(z, (b << 16) | a);
  std::vector<unsigned char> out = {137, 80, 78, 71, 13, 10, 26, 10}, hdr;
  word(hdr, w);
  word(hdr, h);
  hdr.insert(hdr.end(), {8, 2, 0, 0, 0});
  chunk(out, "IHDR", hdr);
  chunk(out, "IDAT", z);
  chunk(out, "IEND", {});
  std::ofstream f(path, std::ios::binary);
  f.write((char *)out.data(), out.size());
  if (!f)
    throw std::runtime_error("cannot write preview");
}
GLuint shader(GLenum type, const char *source) {
  GLuint s = glCreateShader(type);
  glShaderSource(s, 1, &source, nullptr);
  glCompileShader(s);
  GLint ok;
  glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
  if (!ok)
    throw std::runtime_error("shader compilation failed");
  return s;
}
struct Scene {
  GLuint program = 0, vao = 0, vbo = 0, ebo = 0;
  int count = 0;
  float yaw = .45f, pitch = .25f, zoom = 1;
  Scene() {
    const char *vs = R"(#version 330 core
 layout(location=0) in vec3 pos; layout(location=1) in vec3 normal;
 uniform mat4 transform; uniform vec3 center; uniform float size;
 uniform vec3 shape; out vec3 n;
 void main(){float u=pos.y*pos.y; float radius=shape.z*(1.0+shape.x*0.3153915653*(3.0*u-1.0)+shape.y*0.1057855469*(35.0*u*u-30.0*u+3.0));
 n=normal; gl_Position=transform*vec4(center+size*radius*pos,1);})";
    const char *fs = R"(#version 330 core
 in vec3 n; uniform vec3 color; out vec4 frag;
 void main(){float light=.25+.75*max(dot(normalize(n),normalize(vec3(-.5,.8,1))),0); frag=vec4(color*light,1);})";
    GLuint v = shader(GL_VERTEX_SHADER, vs), f = shader(GL_FRAGMENT_SHADER, fs);
    program = glCreateProgram();
    glAttachShader(program, v);
    glAttachShader(program, f);
    glLinkProgram(program);
    GLint ok;
    glGetProgramiv(program, GL_LINK_STATUS, &ok);
    if (!ok)
      throw std::runtime_error("shader link failed");
    glDeleteShader(v);
    glDeleteShader(f);
    std::vector<float> mesh;
    std::vector<unsigned> indices;
    constexpr float pi = 3.141592653589793f;
    for (int i = 0; i <= 16; i++)
      for (int j = 0; j <= 24; j++) {
        float lat = -pi / 2 + pi * i / 16, lon = 2 * pi * j / 24;
        float x = cos(lat) * cos(lon), y = sin(lat), z = cos(lat) * sin(lon);
        mesh.insert(mesh.end(), {x, y, z, x, y, z});
      }
    for (unsigned i = 0; i < 16; i++)
      for (unsigned j = 0; j < 24; j++) {
        unsigned a = i * 25 + j, b = a + 25;
        indices.insert(indices.end(), {a, b, b + 1, a, b + 1, a + 1});
      }
    count = (int)indices.size();
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, mesh.size() * sizeof(float), mesh.data(),
                 GL_STATIC_DRAW);
    glGenBuffers(1, &ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned),
                 indices.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float),
                          (void *)(3 * sizeof(float)));
  }
  ~Scene() {
    glDeleteBuffers(1, &ebo);
    glDeleteBuffers(1, &vbo);
    glDeleteVertexArrays(1, &vao);
    glDeleteProgram(program);
  }
  void draw(int w, int h, const std::vector<std::array<float, 4>> &points,
            double fraction, bool barrier, double ratio, alpha::Deformation deformation) {
    glViewport(0, 0, w, h);
    glEnable(GL_DEPTH_TEST);
    glClearColor(.025f, .045f, .075f, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glUseProgram(program);
    glBindVertexArray(vao);
    float cy = cos(yaw), sy = sin(yaw), cx = cos(pitch), sx = sin(pitch),
          aspect = float(w) / h;
    float scale = .115f * zoom;
    float m[16] = {scale * cy / aspect,
                   scale * sx * sy,
                   scale * cx * sy,
                   0,
                   0,
                   scale * cx,
                   -scale * sx,
                   0,
                   -scale * sy / aspect,
                   scale * sx * cy,
                   scale * cx * cy,
                   0,
                   .35f,
                   0,
                   0,
                   1};
    glUniformMatrix4fv(glGetUniformLocation(program, "transform"), 1, GL_FALSE,
                       m);
    glUniform3f(glGetUniformLocation(program, "shape"), 0, 0, 1);
    if (barrier) {
      double raw0=1-deformation.beta2*0.3153915653+deformation.beta4*0.3173566407;
      double norm=alpha::deformed_radius(1,deformation,0)/raw0;
      glUniform3f(glGetUniformLocation(program, "shape"), float(deformation.beta2), float(deformation.beta4), float(norm));
      glUniform3f(glGetUniformLocation(program, "center"), 0, 0, 0);
      glUniform1f(glGetUniformLocation(program, "size"),
                  5.0f * (float)std::min(1.0, ratio));
      glUniform3f(glGetUniformLocation(program, "color"), .23f, .75f, .95f);
      glDrawElements(GL_TRIANGLES, count, GL_UNSIGNED_INT, nullptr);
      glUniform3f(glGetUniformLocation(program, "shape"), 0, 0, 1);
      glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
      glUniform1f(glGetUniformLocation(program, "size"), 5);
      glUniform3f(glGetUniformLocation(program, "color"), .95f, .65f, .2f);
      glDrawElements(GL_TRIANGLES, count, GL_UNSIGNED_INT, nullptr);
      glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    } else
      for (auto p : points)
        if (p[3] > fraction) {
          glUniform3f(glGetUniformLocation(program, "center"), p[0], p[1],
                      p[2]);
          glUniform1f(glGetUniformLocation(program, "size"), .32f);
          glUniform3f(glGetUniformLocation(program, "color"), .23f, .75f, .95f);
          glDrawElements(GL_TRIANGLES, count, GL_UNSIGNED_INT, nullptr);
        }
    glDisable(GL_DEPTH_TEST);
  }
};
} // namespace
int viewer(int argc, char **argv) {
  bool preview = argc >= 2 && std::string(argv[1]) == "--render-preview";
  if ((!preview && argc != 1) ||
      (preview && (argc < 3 || argc > 9 || argc == 5 || argc == 8)))
    throw std::invalid_argument(
        "usage: AlphaDecay [--run|--self-test|--render-preview path "
        "[catalog-id [width height [external|square_well|woods_saxon [beta2 beta4]]]]]");
  int width = 1280, height = 800;
  if (argc >= 6) {
    width = std::stoi(argv[4]);
    height = std::stoi(argv[5]);
    if (width < 640 || height < 480 || width > 4096 || height > 4096)
      throw std::invalid_argument("preview size outside 640x480 to 4096x4096");
  }
  auto catalog = json::parse(embedded::catalog);
  int selected = 0;
  for (int i = 0; i < (int)catalog.size(); i++) {
    std::string id = catalog[i]["symbol"].get<std::string>() + "-" +
                     std::to_string(catalog[i]["a"].get<int>());
    if ((argc >= 4 && id == argv[3]) || (argc < 4 && id == "Po-212"))
      selected = i;
  }
  if (argc >= 4) {
    auto id = catalog[selected]["symbol"].get<std::string>() + "-" +
              std::to_string(catalog[selected]["a"].get<int>());
    if (id != argv[3])
      throw std::invalid_argument("unknown catalog id");
  }
  if (!glfwInit())
    throw std::runtime_error("GLFW initialization failed");
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  if (preview)
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
  GLFWwindow *window = glfwCreateWindow(
      width, height, "Alpha Decay Planetarium", nullptr, nullptr);
  if (!window) {
    glfwTerminate();
    throw std::runtime_error("OpenGL 3.3 window creation failed");
  }
  glfwMakeContextCurrent(window);
  glfwSwapInterval(preview ? 0 : 1);
  if (!gladLoadGL((GLADloadfunc)glfwGetProcAddress))
    throw std::runtime_error("OpenGL loader failed");
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGui::StyleColorsDark();
  ImGui::GetIO().FontGlobalScale = 1.15f;
  ImGui::GetIO().IniFilename = nullptr;
  ImGui_ImplGlfw_InitForOpenGL(window, true);
  ImGui_ImplOpenGL3_Init("#version 330 core");
  {
    Scene scene;
    alpha::Request request;
    alpha::Result result{};
    bool valid = false, playing = false, barrier = false, reference_available = false;
    float elapsed = 0;
    int model = 0, potential_model = 0, convention = 0;
    double depth = 100, nuclear_radius = 9, diffuseness = .65, charge_radius = 9;
    alpha::SphericalWkbResult alternative;
#ifdef ALPHA_ENABLE_NUMEROV
    struct Analysis { alpha::NumerovResult numerov; alpha::UncertaintyResult uncertainty; };
    std::future<Analysis> analysis_job;
    auto analysis_cancel=std::make_shared<std::atomic_bool>(false);
    std::optional<Analysis> analysis_result;
    alpha::NumerovOptions numerical_options;
    std::array<double,4> sigma{};
    std::array<double,16> covariance{}; bool use_covariance=false;
    int analysis_seed=1, analysis_draws=100;
    bool discard_analysis=false;
#endif
    bool advanced=false, parent_known=false, competing_enabled=false;
    int parent_twice_j=0, parent_parity=1;
    double beta2=0, beta4=0, competing_rate=0;
    struct GuiChannel { double ex=0, preformation=0.1, assault_hz=1e21; int l=0, twice_j=0, parity=1; bool daughter_known=false, custom_rate=false; };
    std::vector<GuiChannel> channel_inputs(1);
    json channel_result;
    std::future<json> channel_job; auto channel_cancel=std::make_shared<std::atomic_bool>(false); bool discard_channel=false;
    std::optional<double> combined_log_half;
    std::string error;
    char fit_path[1024] = {};
    json fit_result;
    std::mt19937 rng(42);
    std::uniform_real_distribution<float> u(0, 1);
    std::vector<std::array<float, 4>> points;
    for (int i = 0; i < 180; i++) {
      float z = 2 * u(rng) - 1, phi = 6.2831853f * u(rng),
            r = 5 * std::cbrt(u(rng)), xy = std::sqrt(1 - z * z);
      points.push_back(
          {r * xy * std::cos(phi), r * z, r * xy * std::sin(phi), u(rng)});
    }
    auto select = [&] {
      channel_cancel->store(true); discard_channel=true; combined_log_half.reset();
#ifdef ALPHA_ENABLE_NUMEROV
      analysis_cancel->store(true); discard_analysis=true; analysis_result.reset();
#endif
      auto &x = catalog[selected];
      request.parent_z = x["z"];
      request.parent_a = x["a"];
      request.q_mev = x["q_mev"];
      request.l = x.value("l", 0);
      request.parent_binding_mev = x.contains("parent_binding_mev") && !x["parent_binding_mev"].is_null()
                                       ? std::optional<double>{x["parent_binding_mev"].get<double>()} : std::nullopt;
      valid = false;
      playing = false;
      elapsed = 0;
    };
    auto make_spherical = [&] {
      alpha::SphericalWkbRequest input;
      input.q_mev=request.q_mev; input.preformation=request.preformation; input.assault_hz=request.assault_hz;
      auto &v=input.potential;
      v.kind=potential_model == 1 ? alpha::PotentialKind::square_well : alpha::PotentialKind::woods_saxon;
      v.daughter_z=request.parent_z-2; v.reduced_mass_mev=931.49410242 * 4.0 * (request.parent_a-4.0) / request.parent_a;
      v.l=request.l; v.nuclear_depth_mev=depth; v.nuclear_radius_fm=nuclear_radius; v.diffuseness_fm=diffuseness; v.charge_radius_fm=charge_radius;
      v.centrifugal=convention == 0 ? alpha::CentrifugalConvention::physical : alpha::CentrifugalConvention::langer_wkb;
      return input;
    };
    auto calculate = [&] {
      if (channel_job.valid()) return;
      try {
        reference_available = false;
        result = {};
        if (potential_model) {

          alpha::SphericalPotential v;
          v.kind=potential_model == 1 ? alpha::PotentialKind::square_well : alpha::PotentialKind::woods_saxon;
          v.nuclear_depth_mev=depth; v.nuclear_radius_fm=nuclear_radius;
          v.diffuseness_fm=diffuseness; v.charge_radius_fm=charge_radius;
          v.centrifugal=convention == 0 ? alpha::CentrifugalConvention::physical : alpha::CentrifugalConvention::langer_wkb;

          auto calculation=alpha::calculate_spherical_model(request, v);
          alternative=calculation.spherical;
          reference_available=calculation.external_reference.has_value();
          if (reference_available) result=*calculation.external_reference;
          auto input=alpha::make_spherical_wkb_request(request,v);
          if (advanced) {
            json data={{"deformation",{{"beta2",beta2},{"beta4",beta4}}},{"channels",json::array()}};
            if(parent_known) data["parent_spin"]={{"twice_j",parent_twice_j},{"parity",parent_parity}};
            if(competing_enabled) data["competing_rate_hz"]=competing_rate;
            for(size_t i=0;i<channel_inputs.size();++i) {
              const auto& c=channel_inputs[i]; json row={{"name",std::to_string(i+1)},{"excitation_mev",c.ex}};
              if(c.l>=0) row["l"]=c.l;
              if(c.custom_rate) {row["preformation"]=c.preformation; row["assault_hz"]=c.assault_hz;}
              if(c.daughter_known) row["daughter_spin"]={{"twice_j",c.twice_j},{"parity",c.parity}};
              data["channels"].push_back(row);
            }
            combined_log_half.reset(); channel_result=json{};
            channel_cancel=std::make_shared<std::atomic_bool>(false); auto token=channel_cancel; discard_channel=false;
            channel_job=std::async(std::launch::async,[data,input,token] {return run_channels(data,input,[token]{return token->load();});});
          }
        } else {
          result=alpha::calculate(request);
          reference_available=true;

        }
        valid = true;
        error.clear();
        elapsed = 0;
        playing = false;
      } catch (const std::exception &e) {
        valid = false;
        playing = false;
        error = e.what();
      }
    };
    select();
    if (argc >= 7) {
      const std::string preview_model=argv[6];
      if (preview_model == "square_well") potential_model=1;
      else if (preview_model == "woods_saxon") potential_model=2;
      else if (preview_model != "external") throw std::invalid_argument("unknown preview potential");
    }
    if(argc==9) {
      if(!potential_model) throw std::invalid_argument("deformed preview requires a nuclear potential");
      beta2=std::stod(argv[7]); beta4=std::stod(argv[8]); advanced=true; barrier=true;
    }
    calculate();
    auto poll_channels = [&] {
      if(channel_job.valid() && channel_job.wait_for(std::chrono::seconds(0))==std::future_status::ready) {
        try { auto value=channel_job.get(); if(!discard_channel) { channel_result=std::move(value); if(!channel_result["log10_total_half_life_s"].is_null()) combined_log_half=channel_result["log10_total_half_life_s"].get<double>(); } }
        catch(const std::exception& e) { if(!discard_channel) {valid=false; error=e.what();} }
      }
    };
    if(preview && channel_job.valid()) {channel_job.wait(); poll_channels();}
    auto selected_log = [&]() -> std::optional<double> {
      if (!valid) return {};
      if (advanced && potential_model && model == 0) return combined_log_half;
      if (model == 1) return reference_available ? result.research_log10_half_life_s : std::nullopt;
      if (model == 2) return reference_available ? result.shell_log10_half_life_s : std::nullopt;
      return potential_model ? alternative.log10_half_life_s : std::optional<double>{result.log10_half_life_s};
    };
    double last = glfwGetTime();
    int frames = 0;
    while (!glfwWindowShouldClose(window)) {
      glfwPollEvents();
      poll_channels();
      double now = glfwGetTime(), dt = std::min(.1, now - last);
      last = now;
      if (playing) {
        elapsed = std::min(10.0f, elapsed + float(dt) * .25f);
        if (elapsed >= 10)
          playing = false;
      }
      int w, h;
      glfwGetFramebufferSize(window, &w, &h);
      if (w <= 0 || h <= 0)
        continue;
      bool model_available = selected_log().has_value();
      scene.draw(w, h, points,
                 model_available ? 1 - std::exp(-std::log(2.0) * elapsed) : 1,
                 barrier && valid,
                 valid && (!potential_model || alternative.outer_turning_fm) ?
                     (potential_model ? (advanced ? nuclear_radius : alternative.inner_turning_fm.value_or(nuclear_radius)) / *alternative.outer_turning_fm : request.radius_fm / result.outer_turning_fm) : 0,
                 valid && potential_model && advanced ? alpha::Deformation{beta2,beta4} : alpha::Deformation{});
      ImGui_ImplOpenGL3_NewFrame();
      ImGui_ImplGlfw_NewFrame();
      ImGui::NewFrame();
      ImGui::SetNextWindowPos(ImVec2(12, 12), ImGuiCond_Always);
      ImGui::SetNextWindowSize(
          ImVec2(std::min(470.0f, float(w) - 24), float(h) - 24),
          ImGuiCond_Always);
      ImGui::Begin("Alpha Decay Planetarium", nullptr,
                   ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                       ImGuiWindowFlags_NoCollapse);
      ImGui::TextWrapped(
          "WKB barrier models | daughter channels");
      ImGui::Separator();
      std::string label = catalog[selected]["symbol"].get<std::string>() + "-" +
                          std::to_string(catalog[selected]["a"].get<int>());
      if (ImGui::BeginCombo("Reference", label.c_str())) {
        for (int i = 0; i < (int)catalog.size(); i++) {
          std::string name = catalog[i]["symbol"].get<std::string>() + "-" +
                             std::to_string(catalog[i]["a"].get<int>());
          if (ImGui::Selectable(name.c_str(), i == selected)) {
            selected = i;
            select();
            calculate();
          }
        }
        ImGui::EndCombo();
      }
      ImGui::PushItemWidth(155.0f);
      bool changed = false;
      if (ImGui::TreeNode("Import Python calibration")) {
        ImGui::InputText("Fit JSON path", fit_path, sizeof(fit_path));
        if (ImGui::Button("Load fit")) {
          try {
            std::ifstream file(fit_path);
            if (!file) throw std::runtime_error("Cannot open fit JSON");
            json candidate = json::parse(file);
            if (candidate.at("schema_version") != 1 || !candidate.at("cases").is_array() || !candidate.at("calibration_ids").is_array())
              throw std::runtime_error("Unsupported fit schema");
            if (!candidate.at("dataset_version").is_string() || candidate.at("dataset_version").get<std::string>().empty())
              throw std::runtime_error("Invalid dataset version");
            const double pre = candidate.at("preformation").get<double>();
            const double hz = candidate.at("assault_hz").get<double>();
            if (!std::isfinite(pre) || pre <= 0 || pre > 1 || !std::isfinite(hz) || hz <= 0)
              throw std::runtime_error("Invalid fitted rate parameters");
            std::set<std::string> ids, calibration;
            for (const auto &id : candidate.at("calibration_ids"))
              if (!calibration.insert(id.get<std::string>()).second) throw std::runtime_error("Duplicate calibration ID");
            for (const auto &row : candidate.at("cases")) {
              const auto id = row.at("id").get<std::string>();
              const auto split = row.at("split").get<std::string>();
              if (id.empty() || !ids.insert(id).second || (split != "calibration" && split != "validation") || (calibration.contains(id) != (split == "calibration")))
                throw std::runtime_error("Invalid fit case membership");
              if (!row.at("request").is_object()) throw std::runtime_error("Invalid fit request");
            }
            if (calibration.empty() || !std::all_of(calibration.begin(), calibration.end(), [&](const auto &id) { return ids.contains(id); }))
              throw std::runtime_error("Invalid calibration IDs");
            fit_result = std::move(candidate);
            error.clear();
          } catch (const std::exception &e) { fit_result = json{}; error = e.what(); }
        }
        if (fit_result.is_object()) {
          ImGui::Text("Dataset: %s", fit_result.value("dataset_version", std::string("unknown")).c_str());
          for (const char *key : {"source", "description"})
            if (fit_result.contains(key) && fit_result[key].is_string()) ImGui::TextWrapped("%s: %s", key, fit_result[key].get<std::string>().c_str());
          for (const auto &row : fit_result["cases"]) {
            const auto label = row.at("id").get<std::string>() + " (" + row.at("split").get<std::string>() + ")";
            if (ImGui::Selectable(label.c_str())) {
              try {
                const auto &r = row.at("request");
                const std::set<std::string> allowed = {"schema_version", "parent_z", "parent_a", "q_mev", "l", "radius_fm", "preformation", "assault_hz", "research_model", "parent_binding_mev", "potential_model", "centrifugal_convention", "nuclear_depth_mev", "nuclear_radius_fm", "diffuseness_fm", "charge_radius_fm"};
                for (auto it = r.begin(); it != r.end(); ++it)
                  if (!allowed.contains(it.key())) throw std::runtime_error("Unsupported fit request field: " + it.key());
                alpha::Request imported = request;
                imported.parent_z = r.at("parent_z").get<int>(); imported.parent_a = r.at("parent_a").get<int>();
                imported.q_mev = r.at("q_mev").get<double>(); imported.l = r.at("l").get<int>(); imported.radius_fm = r.at("radius_fm").get<double>();
                imported.preformation = fit_result.at("preformation").get<double>(); imported.assault_hz = fit_result.at("assault_hz").get<double>();
                imported.research_model = r.value("research_model", false); imported.parent_binding_mev.reset();
                if (r.contains("parent_binding_mev") && !r["parent_binding_mev"].is_null()) imported.parent_binding_mev = r["parent_binding_mev"].get<double>();
                const auto kind = r.value("potential_model", std::string("external"));
                if (kind != "external" && kind != "square_well" && kind != "woods_saxon") throw std::runtime_error("Unsupported fit potential");
                const auto conv = r.value("centrifugal_convention", std::string("physical"));
                if (conv != "physical" && conv != "langer_wkb") throw std::runtime_error("Unsupported centrifugal convention");
                double d = r.value("nuclear_depth_mev", 100.0), nr = r.value("nuclear_radius_fm", imported.radius_fm), df = r.value("diffuseness_fm", .65), cr = r.value("charge_radius_fm", imported.radius_fm);
                request = imported; potential_model = kind == "external" ? 0 : kind == "square_well" ? 1 : 2;
                convention = conv == "physical" ? 0 : 1; depth = d; nuclear_radius = nr; diffuseness = df; charge_radius = cr;
                advanced = false; changed = true; error.clear();
              } catch (const std::exception &e) { error = e.what(); }
            }
          }
        }
        ImGui::TreePop();
      }
      const char *potentials[] = {"External reference", "Square well", "Woods-Saxon"};
      changed |= ImGui::Combo("Potential", &potential_model, potentials, 3);
      if (potential_model) {
        changed |= ImGui::InputDouble("Well depth (MeV)", &depth, 0, 0, "%.6g");
        changed |= ImGui::InputDouble("Nuclear radius (fm)", &nuclear_radius, 0, 0, "%.6g");
        if (potential_model == 2) changed |= ImGui::InputDouble("Diffuseness (fm)", &diffuseness, 0, 0, "%.6g");
        changed |= ImGui::InputDouble("Charge radius (fm)", &charge_radius, 0, 0, "%.6g");
        const char *conventions[] = {"Physical l(l+1)", "Langer WKB (l+1/2)^2"};
        changed |= ImGui::Combo("Centrifugal term", &convention, conventions, 2);
      }
      if (potential_model) {
        changed |= ImGui::Checkbox("Deformation and daughter channels", &advanced);
        if (advanced) {
          changed |= ImGui::InputDouble("Beta 2", &beta2, 0, 0, "%.4g");
          changed |= ImGui::InputDouble("Beta 4", &beta4, 0, 0, "%.4g");
          ImGui::TextWrapped("Blue: R(theta). Gold: spherical-reference outer radius. Barrier is schematic.");
          changed |= ImGui::Checkbox("Parent spin known", &parent_known);
          if(parent_known) { changed |= ImGui::InputInt("Parent twice J", &parent_twice_j); changed |= ImGui::InputInt("Parent parity (+1/-1)", &parent_parity); }
          changed |= ImGui::Checkbox("Include competing rate", &competing_enabled);
          if(competing_enabled) changed |= ImGui::InputDouble("Competing rate (Hz)", &competing_rate, 0, 0, "%.6g");
          if(ImGui::TreeNode("Edit daughter channels")) {
            for(size_t i=0;i<channel_inputs.size();++i) {
              ImGui::PushID(static_cast<int>(i)); auto& c=channel_inputs[i];
              ImGui::Text("Channel %zu",i+1);
              changed |= ImGui::InputDouble("Excitation (MeV)",&c.ex,0,0,"%.6g");
              changed |= ImGui::InputInt("l (-1 = choose from spins)",&c.l);
              changed |= ImGui::Checkbox("Custom preformation / assault",&c.custom_rate);
              if(c.custom_rate) {changed |= ImGui::InputDouble("Channel preformation",&c.preformation,0,0,"%.6g"); changed |= ImGui::InputDouble("Channel assault (Hz)",&c.assault_hz,0,0,"%.6g");}
              changed |= ImGui::Checkbox("Daughter spin known",&c.daughter_known);
              if(c.daughter_known) {changed |= ImGui::InputInt("Daughter twice J",&c.twice_j); changed |= ImGui::InputInt("Daughter parity (+1/-1)",&c.parity);}
              if(channel_inputs.size()>1 && ImGui::Button("Remove channel")) {channel_inputs.erase(channel_inputs.begin()+i); changed=true; ImGui::PopID(); break;}
              ImGui::Separator(); ImGui::PopID();
            }
            if(ImGui::Button("Add channel")) {channel_inputs.emplace_back();changed=true;}
            ImGui::TreePop();
          }
        }
      }
      bool identity_changed = ImGui::InputInt("Parent Z", &request.parent_z);
      identity_changed |= ImGui::InputInt("Parent A", &request.parent_a);
      if (identity_changed)
        request.parent_binding_mev.reset();
      changed |= identity_changed;
      changed |= ImGui::InputDouble("Q (MeV)", &request.q_mev, 0, 0, "%.8g");
      changed |= ImGui::InputInt("Angular momentum l", &request.l);
      changed |=
          ImGui::InputDouble("External boundary (fm)", &request.radius_fm, 0, 0, "%.6g");
      changed |= ImGui::InputDouble("Preformation", &request.preformation, 0, 0,
                                    "%.6g");
      changed |= ImGui::InputDouble("Assault frequency (Hz)",
                                    &request.assault_hz, 0, 0, "%.6g");
      double binding = request.parent_binding_mev.value_or(0);
      if (ImGui::InputDouble("Parent binding (MeV)", &binding, 0, 0, "%.9g")) {
        request.parent_binding_mev =
            binding == 0 ? std::optional<double>{} : binding;
        changed = true;
      }
      if (changed) {
        channel_cancel->store(true); discard_channel=true; combined_log_half.reset();
#ifdef ALPHA_ENABLE_NUMEROV
        analysis_cancel->store(true); discard_analysis=true; analysis_result.reset();
#endif
        valid = false;
        playing = false;
        error.clear();
      }
      if (channel_job.valid()) {
        ImGui::TextUnformatted("Calculating channels in background...");
        if (ImGui::Button("Cancel channels")) {channel_cancel->store(true); discard_channel=true;}
      } else if (ImGui::Button("Calculate")) calculate();
      ImGui::SameLine();
      if (!error.empty())
        ImGui::TextWrapped("%s", error.c_str());
      ImGui::Separator();
#ifdef ALPHA_ENABLE_NUMEROV
      if (analysis_job.valid() && analysis_job.wait_for(std::chrono::seconds(0))==std::future_status::ready) {
        try { auto finished=analysis_job.get(); if (!discard_analysis) analysis_result=std::move(finished); }
        catch (const std::exception& e) { error=e.what(); }
      }
      if (potential_model && ImGui::TreeNode("Numerov and parameter uncertainty")) {
        bool options_changed=false;
        options_changed |= ImGui::InputDouble("Inner lead radius (fm)",&numerical_options.lead_radius_fm,0,0,"%.5g");
        options_changed |= ImGui::InputDouble("Exterior match radius (fm)",&numerical_options.match_radius_fm,0,0,"%.5g");
        options_changed |= ImGui::InputDouble("Numerov step (fm)",&numerical_options.step_fm,0,0,"%.5g");
        options_changed |= ImGui::InputDouble("Q standard deviation (MeV)",&sigma[0],0,0,"%.5g");
        options_changed |= ImGui::InputDouble("Depth standard deviation (MeV)",&sigma[1],0,0,"%.5g");
        options_changed |= ImGui::InputDouble("Radius standard deviation (fm)",&sigma[2],0,0,"%.5g");
        options_changed |= ImGui::InputDouble("Diffuseness standard deviation (fm)",&sigma[3],0,0,"%.5g");
        options_changed |= ImGui::Checkbox("Use covariance matrix",&use_covariance);
        if (use_covariance && ImGui::TreeNode("Covariance: Q, depth, radius, diffuseness")) {
          for (int row=0;row<4;++row) for (int col=0;col<4;++col) {
            std::string label="C["+std::to_string(row)+","+std::to_string(col)+"]";
            options_changed |= ImGui::InputDouble(label.c_str(),&covariance[row*4+col],0,0,"%.5g");
          }
          ImGui::TreePop();
        }
        options_changed |= ImGui::InputInt("Random seed",&analysis_seed);
        options_changed |= ImGui::InputInt("Samples",&analysis_draws);
        if(options_changed) {analysis_cancel->store(true); discard_analysis=true; analysis_result.reset();}
        if (analysis_job.valid()) {
          ImGui::TextUnformatted("Calculating in background...");
          if (ImGui::Button("Cancel analysis")) { analysis_cancel->store(true); discard_analysis=true; }
        } else if (valid && analysis_seed>=0 && analysis_draws>0 && analysis_draws<=1000000 && ImGui::Button("Run numerical analysis")) {
          auto input=make_spherical(); auto options=numerical_options; auto deviations=sigma;
          alpha::UncertaintyRequest u; u.baseline=input; u.standard_deviations=deviations; if(use_covariance) u.covariance=covariance; u.seed=static_cast<std::uint64_t>(analysis_seed); u.draws=static_cast<std::size_t>(analysis_draws);
          analysis_cancel=std::make_shared<std::atomic_bool>(false); auto token=analysis_cancel; discard_analysis=false; analysis_result.reset();
          analysis_job=std::async(std::launch::async,[input,options,u,token] {
            Analysis x; x.numerov=alpha::calculate_numerov(input,options,token.get());
            if (!token->load()) x.uncertainty=alpha::sample_spherical_uncertainty(u,[token]{return token->load();});
            return x;
          });
        }
        if (analysis_result) {
          auto &n=analysis_result->numerov; auto &u=analysis_result->uncertainty;
          ImGui::Text("Numerov: %s",n.status.c_str()); ImGui::TextWrapped("%s",n.message.c_str());
          if(n.log10_transmission) ImGui::Text("log10 transmission: %.8g",*n.log10_transmission);
          if(n.step_error_decades && n.match_error_decades) ImGui::Text("Step / exterior error: %.3g / %.3g decades",*n.step_error_decades,*n.match_error_decades);
          if(n.wkb_difference_decades) ImGui::Text("Numerov minus WKB: %.5g decades",*n.wkb_difference_decades);
          if(convention==1) ImGui::TextWrapped("Comparison uses different centrifugal terms: Numerov physical, WKB Langer.");
          if(n.log10_half_life_s) ImGui::Text("Numerov log10 half-life: %.8g",*n.log10_half_life_s);
          ImGui::Text("Finite-rate draws: %zu retained; %zu excluded (%.1f%%)",u.accepted,u.rejected,100*u.rejection_fraction);
          ImGui::Text("Excluded: %zu closed, %zu zero rate, %zu invalid, %zu numerical, %zu unsupported",u.closed,u.zero_rate,u.invalid,u.numerical_failure,u.unsupported);
          if(u.log10_half_life_p50) ImGui::Text("log10 half-life p16 / p50 / p84: %.4g / %.4g / %.4g",*u.log10_half_life_p16,*u.log10_half_life_p50,*u.log10_half_life_p84);
          ImGui::TextWrapped("%s",u.message.c_str());
          ImGui::TextWrapped("Spherical ground-state comparison; deformed channel transmission is not included.");
        ImGui::TextWrapped("Inner-lead radius is a model assumption. Physical centrifugal term; same preformation and assault frequency. Not a resonance width.");
        }
        ImGui::TreePop();
      }
#endif
      const char *models[] = {"Selected WKB", "Qi (2009)", "Ren (2026)"};
      if (ImGui::Combo("Playback model", &model, models, 3)) {
        playing = false;
        elapsed = 0;
      }
      bool available = selected_log().has_value();
      if (!available)
        ImGui::TextWrapped("Selected model unavailable. Calculate valid "
                           "even-even l=0 inputs for Qi/Ren.");
      if (available) {
        if (ImGui::Button(playing ? "Pause" : "Play"))
          playing = !playing;
        ImGui::SameLine();
        if (ImGui::Button("Reset")) {
          elapsed = 0;
          playing = false;
        }
        ImGui::SliderFloat("Elapsed half-lives", &elapsed, 0, 10);
        double log_half = *selected_log();
        ImGui::Text("Selected half-life: %s", time_label(log_half).c_str());
        if (elapsed > 0)
          ImGui::Text("Elapsed: %s",
                      time_label(log_half + std::log10(elapsed)).c_str());
        else
          ImGui::TextUnformatted("Elapsed: 0 s");
        double survival = std::exp(-std::log(2.0) * elapsed);
        int sampled = (int)std::count_if(
            points.begin(), points.end(),
            [&](const auto &point) { return point[3] > 1 - survival; });
        ImGui::Text("Survivors: %d sampled / %.1f expected of 180", sampled,
                    180 * survival);
      }
      if(valid && potential_model && advanced && ImGui::TreeNode("Channel results")) {
        ImGui::TextWrapped(competing_enabled ? "Branches include the supplied competing rate." : "Branches are conditional on alpha decay.");
        if(combined_log_half) ImGui::Text(competing_enabled ? "Total log10 half-life: %.8g" : "Alpha-only log10 half-life: %.8g",*combined_log_half);
        for(const auto& row:channel_result["channels"]) {
          ImGui::Separator(); ImGui::Text("Channel %s: %s",row["name"].get<std::string>().c_str(),row["status"].get<std::string>().c_str());
          ImGui::Text("Q = %.6g MeV",row["q_mev"].get<double>());
          if(!row["l"].is_null()) ImGui::Text("l = %d",row["l"].get<int>());
          if(!row["log10_partial_half_life_s"].is_null()) ImGui::Text("Partial log10 half-life = %.8g",row["log10_partial_half_life_s"].get<double>());
          if(!row["conditional_alpha_branch"].is_null()) ImGui::Text("Conditional alpha branch = %.8g",row["conditional_alpha_branch"].get<double>());
          if(!row["total_branch"].is_null()) ImGui::Text("Overall branch = %.8g",row["total_branch"].get<double>());
          if(row.contains("angular_error_log10")) ImGui::Text("Angular / charge log errors: %.3g / %.3g",row["angular_error_log10"].get<double>(),row["charge_error_log10"].get<double>());
          if(row.contains("barrier_message") && !row["barrier_message"].get<std::string>().empty()) ImGui::TextWrapped("%s",row["barrier_message"].get<std::string>().c_str());
        }
        ImGui::TreePop();
      }
      if (valid) {
        if (potential_model && ImGui::TreeNode("Radial potential (spherical reference)")) {
          // Avoid r=0, where the centrifugal term may diverge. The displayed
          // interval is bounded so very low-Q cases remain readable.
          const auto v = make_spherical().potential;
          const double end = std::min(1000.0, std::max(nuclear_radius * 2.0,
              alternative.outer_turning_fm.value_or(nuclear_radius * 2.0) * 1.1));
          constexpr int count = 256;
          std::array<float, count> values{};
          const double start = std::min(0.2, end / count);
          for (int i = 0; i < count; ++i) {
            const double r = start + (end - start) * i / (count - 1);
            values[i] = static_cast<float>(alpha::spherical_potential(v, r));
          }
          ImGui::Text("Total potential (MeV), radius %.3g to %.3g fm; Q = %.5g MeV", start, end, request.q_mev);
          ImGui::PlotLines("##potential", values.data(), count, 0, nullptr, FLT_MAX, FLT_MAX, ImVec2(0, 130));
          if (advanced) ImGui::TextWrapped("This is the spherical reference, not the deformed orientation-dependent barrier.");
          ImGui::TreePop();
        }
        if (!potential_model) {
          ImGui::Text("External WKB log10(t / s): %.8g", result.log10_half_life_s);
          ImGui::Text("External WKB half-life: %s", time_label(result.log10_half_life_s).c_str());
        } else {
          ImGui::Text("Spherical reference status: %s", alternative.status.c_str());
          if (!alternative.message.empty()) ImGui::TextWrapped("%s", alternative.message.c_str());
          if (alternative.log10_half_life_s) ImGui::Text("Spherical reference log10(t / s): %.8g", *alternative.log10_half_life_s);
          if (alternative.log10_penetrability) ImGui::Text("Spherical reference log10 penetrability: %.8g", *alternative.log10_penetrability);
          if (alternative.inner_turning_fm) ImGui::Text("Inner barrier boundary: %.5g fm", *alternative.inner_turning_fm);
          if (reference_available) ImGui::Text("External reference log10(t / s): %.8g", result.log10_half_life_s);
        }
        if (result.research_log10_half_life_s)
          ImGui::Text("Qi log10(t / s): %.8g",
                      *result.research_log10_half_life_s);
        if (result.shell_log10_half_life_s)
          ImGui::Text("Ren log10(t / s): %.8g",
                      *result.shell_log10_half_life_s);
        if (!potential_model) ImGui::Text("Outer turning point: %.5g fm", result.outer_turning_fm);
        else if (alternative.outer_turning_fm) ImGui::Text("Outer turning point: %.5g fm", *alternative.outer_turning_fm);
        auto &x = catalog[selected];
        bool match = request.parent_z == x["z"].get<int>() &&
                     request.parent_a == x["a"].get<int>() &&
                     request.q_mev == x["q_mev"].get<double>() &&
                     request.l == x.value("l", 0);
        if (match && !(advanced && potential_model)) {
          double measured = x["partial_half_life_s"].get<double>();
          ImGui::Text("Measured partial half-life: %.6g s", measured);
          ImGui::Text("Measured: %s", time_label(std::log10(measured)).c_str());
          if (x.contains("half_life_uncertainty_s"))
            ImGui::Text("Half-life uncertainty: %.4g s",
                        x["half_life_uncertainty_s"].get<double>());
          if (x.contains("half_life_uncertainty_minus_s") && x.contains("half_life_uncertainty_plus_s"))
            ImGui::Text("Half-life uncertainty: +%.4g / -%.4g s", x["half_life_uncertainty_plus_s"].get<double>(), x["half_life_uncertainty_minus_s"].get<double>());
          if (x.contains("q_uncertainty_mev"))
            ImGui::Text("Q uncertainty: %.4g MeV",
                        x["q_uncertainty_mev"].get<double>());
          if (available && !(advanced && potential_model)) {
            double pred = *selected_log();
            ImGui::Text("Residual log10(predicted / measured): %.5g",
                        pred - std::log10(measured));
          }
          if (ImGui::TreeNode("Sources and historical observations")) {
          ImGui::TextWrapped("Source: %s",
                             x["source_url"].get<std::string>().c_str());
          for (const char *key :
               {"branch_source_url", "binding_source_url", "branch_note",
                "binding_note", "holdout_reason", "q_source_url"})
            if (x.contains(key))
              ImGui::TextWrapped("%s: %s", key,
                                 x[key].get<std::string>().c_str());
          if (x.contains("historical_observations")) for (const auto &history : x["historical_observations"])
            ImGui::TextWrapped("%d historical upper limit: < %.4g s; %s", history["year"].get<int>(), history["half_life_upper_limit_s"].get<double>(), history["source_url"].get<std::string>().c_str());
          if (x.contains("theoretical_predictions")) for (const auto &prediction : x["theoretical_predictions"])
            ImGui::TextWrapped("Theory (%s %d): Q %.4g +/- %.3g MeV, T ~ %.3g s; %s", prediction["author"].get<std::string>().c_str(), prediction["year"].get<int>(), prediction["q_mev"].get<double>(), prediction["q_uncertainty_mev"].get<double>(), prediction["half_life_s_approx"].get<double>(), prediction["source_url"].get<std::string>().c_str());
          ImGui::TreePop();
          }
          if (x.value("validation_group", std::string{}) == "ren_holdout")
            ImGui::TextWrapped("Ren holdout: excluded by the published "
                               "half-life uncertainty criterion.");
        } else if (!(advanced && potential_model))
          ImGui::TextWrapped(
              "Custom channel: no catalog measurement associated.");
      }
      ImGui::Checkbox("Single nucleus / barrier view", &barrier);
      ImGui::PopItemWidth();
      if (barrier && advanced && potential_model)
        ImGui::TextWrapped("Blue: volume-normalized nuclear radius R(theta). Gold: spherical-reference outer turning radius; barrier and orientation-dependent turning surfaces are schematic.");
      else if (barrier)
        ImGui::TextWrapped("Blue: inner barrier boundary; gold wireframe: outer turning "
                           "radius b. Radii use the physical R/b ratio when R "
                           "< b. For R >= b there is no external barrier.");
      else
        ImGui::TextWrapped(
            "Each sphere is an independently sampled nucleus; positions and "
            "sizes are illustrative. Competing branches are excluded.");
      ImGui::TextWrapped("Drag the scene to orbit; scroll to zoom.");
      ImGui::End();
      ImGuiIO &io = ImGui::GetIO();
      if (!io.WantCaptureMouse) {
        if (ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
          scene.yaw += io.MouseDelta.x * .01f;
          scene.pitch =
              std::clamp(scene.pitch + io.MouseDelta.y * .01f, -1.5f, 1.5f);
        }
        scene.zoom = std::clamp(scene.zoom + io.MouseWheel * .1f, .4f, 2.5f);
      }
      ImGui::Render();
      ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
      if (preview && ++frames >= 2) {
        png(argv[2], w, h);
        break;
      }
      glfwSwapBuffers(window);
    }
    channel_cancel->store(true);
#ifdef ALPHA_ENABLE_NUMEROV
    analysis_cancel->store(true);
#endif
  }
  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();
  glfwDestroyWindow(window);
  glfwTerminate();
  return 0;
}









