#include "viewer.hpp"
#include "alpha/core.hpp"
#include "alpha/spherical_wkb.hpp"
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
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <random>
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
 uniform mat4 transform; uniform vec3 center; uniform float size; out vec3 n;
 void main(){n=normal; gl_Position=transform*vec4(center+size*pos,1);})";
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
            double fraction, bool barrier, double ratio) {
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
    if (barrier) {
      glUniform3f(glGetUniformLocation(program, "center"), 0, 0, 0);
      glUniform1f(glGetUniformLocation(program, "size"),
                  5.0f * (float)std::min(1.0, ratio));
      glUniform3f(glGetUniformLocation(program, "color"), .23f, .75f, .95f);
      glDrawElements(GL_TRIANGLES, count, GL_UNSIGNED_INT, nullptr);
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
      (preview && (argc < 3 || argc > 7 || argc == 5)))
    throw std::invalid_argument(
        "usage: AlphaDecay [--run|--self-test|--render-preview path "
        "[catalog-id [width height [external|square_well|woods_saxon]]]]");
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
    std::string error;
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
    auto calculate = [&] {
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
    if (argc == 7) {
      const std::string preview_model=argv[6];
      if (preview_model == "square_well") potential_model=1;
      else if (preview_model == "woods_saxon") potential_model=2;
      else if (preview_model != "external") throw std::invalid_argument("unknown preview potential");
    }
    calculate();
    auto selected_log = [&]() -> std::optional<double> {
      if (!valid) return {};
      if (model == 1) return reference_available ? result.research_log10_half_life_s : std::nullopt;
      if (model == 2) return reference_available ? result.shell_log10_half_life_s : std::nullopt;
      return potential_model ? alternative.log10_half_life_s : std::optional<double>{result.log10_half_life_s};
    };
    double last = glfwGetTime();
    int frames = 0;
    while (!glfwWindowShouldClose(window)) {
      glfwPollEvents();
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
                     (potential_model ? alternative.inner_turning_fm.value_or(nuclear_radius) / *alternative.outer_turning_fm : request.radius_fm / result.outer_turning_fm) : 0);
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
          "Spherical WKB models | one alpha channel");
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
        valid = false;
        playing = false;
        error.clear();
      }
      if (ImGui::Button("Calculate"))
        calculate();
      ImGui::SameLine();
      if (!error.empty())
        ImGui::TextWrapped("%s", error.c_str());
      ImGui::Separator();
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
      if (valid) {
        if (!potential_model) {
          ImGui::Text("External WKB log10(t / s): %.8g", result.log10_half_life_s);
          ImGui::Text("External WKB half-life: %s", time_label(result.log10_half_life_s).c_str());
        } else {
          ImGui::Text("Spherical status: %s", alternative.status.c_str());
          if (!alternative.message.empty()) ImGui::TextWrapped("%s", alternative.message.c_str());
          if (alternative.log10_half_life_s) ImGui::Text("Spherical log10(t / s): %.8g", *alternative.log10_half_life_s);
          if (alternative.log10_penetrability) ImGui::Text("log10 penetrability: %.8g", *alternative.log10_penetrability);
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
        if (match) {
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
          if (available) {
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
        } else
          ImGui::TextWrapped(
              "Custom channel: no catalog measurement associated.");
      }
      ImGui::Checkbox("Single nucleus / barrier view", &barrier);
      ImGui::PopItemWidth();
      if (barrier)
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
  }
  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();
  glfwDestroyWindow(window);
  glfwTerminate();
  return 0;
}
