// Silicon LogiX / SLX SolarPilot
// Copyright (c) 2026 Marco Pezzullo. Author: Marco Pezzullo.
// License: Silicon LogiX Evaluation License 1.0. See LICENSE.
#include "renderer.hpp"
#include "gl_api.hpp"
#define CGLTF_IMPLEMENTATION
#include <cgltf.h>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <fstream>
#include <map>
#include <set>
#include <stb_image_write.h>
namespace slx::desktop {
namespace {
struct V3 {
  float x = 0, y = 0, z = 0;
  V3 operator+(V3 b) const {
    return {x + b.x, y + b.y, z + b.z};
  }
  V3 operator-(V3 b) const {
    return {x - b.x, y - b.y, z - b.z};
  }
  V3 operator*(float s) const {
    return {x * s, y * s, z * s};
  }
};
float dot(V3 a, V3 b) {
  return a.x * b.x + a.y * b.y + a.z * b.z;
}
V3 cross(V3 a, V3 b) {
  return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
V3 norm(V3 v) {
  return v * (1 / std::sqrt(std::max(1e-9f, dot(v, v))));
}
struct M4 {
  float m[16]{};
  static M4 identity() {
    M4 v;
    v.m[0] = v.m[5] = v.m[10] = v.m[15] = 1;
    return v;
  }
};
M4 mul(const M4 &a, const M4 &b) {
  M4 c;
  for (int col = 0; col < 4; ++col)
    for (int row = 0; row < 4; ++row)
      for (int k = 0; k < 4; ++k)
        c.m[col * 4 + row] += a.m[k * 4 + row] * b.m[col * 4 + k];
  return c;
}
V3 point(const M4 &m, V3 v) {
  return {m.m[0] * v.x + m.m[4] * v.y + m.m[8] * v.z + m.m[12],
          m.m[1] * v.x + m.m[5] * v.y + m.m[9] * v.z + m.m[13],
          m.m[2] * v.x + m.m[6] * v.y + m.m[10] * v.z + m.m[14]};
}
M4 look(V3 eye, V3 center) {
  V3 f = norm(center - eye), s = norm(cross(f, {0, 1, 0})), u = cross(s, f);
  auto a = M4::identity();
  a.m[0] = s.x;
  a.m[4] = s.y;
  a.m[8] = s.z;
  a.m[1] = u.x;
  a.m[5] = u.y;
  a.m[9] = u.z;
  a.m[2] = -f.x;
  a.m[6] = -f.y;
  a.m[10] = -f.z;
  a.m[12] = -dot(s, eye);
  a.m[13] = -dot(u, eye);
  a.m[14] = dot(f, eye);
  return a;
}
M4 perspective(float aspect) {
  M4 a;
  const float f = 1 / std::tan(.392699f);
  a.m[0] = f / aspect;
  a.m[5] = f;
  a.m[10] = -(300.f + .5f) / (300.f - .5f);
  a.m[11] = -1;
  a.m[14] = -2 * 300.f * .5f / (300.f - .5f);
  return a;
}
unsigned shader_program(const char *vs, const char *fs) {
  auto shader = [](unsigned type, const char *text) {
    auto s = gl::CreateShader(type);
    gl::ShaderSource(s, 1, &text, nullptr);
    gl::CompileShader(s);
    int ok;
    gl::GetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
      char e[4096];
      gl::GetShaderInfoLog(s, 4096, nullptr, e);
      throw std::runtime_error(e);
    }
    return s;
  };
  auto v = shader(GL_VERTEX_SHADER, vs), f = shader(GL_FRAGMENT_SHADER, fs),
       p = gl::CreateProgram();
  gl::AttachShader(p, v);
  gl::AttachShader(p, f);
  gl::LinkProgram(p);
  gl::DeleteShader(v);
  gl::DeleteShader(f);
  int ok;
  gl::GetProgramiv(p, GL_LINK_STATUS, &ok);
  if (!ok)
    throw std::runtime_error("GLSL link failed");
  return p;
}
const char *vertex = R"(#version 330 core
layout(location=0)in vec3 pos;layout(location=1)in vec3 normal;layout(location=2)in vec2 uv;
uniform mat4 model,vp;out vec3 N,W;out vec2 UV;
void main(){vec4 w=model*vec4(pos,1);W=w.xyz;N=mat3(transpose(inverse(model)))*normal;UV=uv;gl_Position=vp*w;})";
const char *fragment = R"(#version 330 core
in vec3 N,W;in vec2 UV;out vec4 color;uniform vec4 base;uniform vec3 eye,emission;
uniform float metallic,roughness,daylight,cutoff,exposure;uniform sampler2D tex;uniform int hasTex;
vec3 aces(vec3 x){return clamp((x*(2.51*x+.03))/(x*(2.43*x+.59)+.14),0.,1.);}
void main(){vec4 b=base*(hasTex==1?texture(tex,UV):vec4(1));if(b.a<cutoff)discard;
vec3 n=normalize(N);if(!gl_FrontFacing)n=-n;vec3 l=normalize(vec3(-.45,1,.65)),v=normalize(eye-W),h=normalize(l+v);
float nl=max(dot(n,l),0.),nv=max(dot(n,v),.001),nh=max(dot(n,h),0.),vh=max(dot(v,h),0.);
float a=max(.06,roughness*roughness),a2=a*a,d=a2/(3.14159*pow(nh*nh*(a2-1.)+1.,2.));
float k=pow(roughness+1.,2.)/8.,g=(nv/(nv*(1.-k)+k))*(nl/(nl*(1.-k)+k));
vec3 f0=mix(vec3(.04),b.rgb,metallic),f=f0+(1.-f0)*pow(1.-vh,5.);
vec3 ambient=mix(vec3(.10,.12,.15),vec3(.23,.26,.29),n.y*.5+.5);
vec3 diffuse=(1.-f)*(1.-metallic)*b.rgb/3.14159265;
vec3 specular=d*g*f/max(.004,4.*nv*nl);
vec3 lit=b.rgb*ambient*(.45+.55*daylight)+(diffuse+specular)*nl*1.55*daylight+emission;
color=vec4(pow(aces(lit*exposure),vec3(1./2.2)),b.a);})";
json read_json(const std::filesystem::path &p) {
  std::ifstream f(p);
  if (!f)
    throw std::runtime_error("Missing contract: " + p.string());
  return json::parse(f);
}
} // namespace
struct RendererImpl {
  struct Model {
    cgltf_data *data = nullptr;
    M4 placement = M4::identity();
    std::string kind;
    std::vector<M4> world;
    std::vector<bool> visible;
    std::map<std::string, bool> overrides;
    std::vector<std::array<float, 3>> positions, scales;
    std::vector<std::array<float, 4>> rotations;
    ~Model() {
      cgltf_free(data);
    }
  };
  struct Draw {
    Model *model = nullptr;
    size_t node = 0;
    unsigned vao = 0, vbo = 0, ibo = 0, tex = 0;
    int count = 0;
    float base[4]{1, 1, 1, 1};
    float metal = 0, rough = .8f, cutoff = .01f;
    V3 emission, min, max;
    bool transparent = false, double_sided = false;
    std::string material;
  };
  std::vector<std::unique_ptr<Model>> models;
  std::vector<Draw> draws;
  std::map<const cgltf_texture *, unsigned> textures;
  std::vector<unsigned> product_images;
  std::set<std::string> hidden, south10;
  json visuals;
  float exposure = .95f;
  int repaired_vertices = 0, recolored_draws = 0;
  std::map<std::string, int> surface_repair_counts;
  json articulations = json::array();
  unsigned shader = 0, fbo = 0, target = 0, depth = 0, ms_fbo = 0, ms_color = 0;
  int width = 0, height = 0, triangles = 0;
  double render_ms = 0, load_ms = 0;
  V3 center{0, 2, 0}, eye;
  float yaw = .6f, pitch = .45f, radius = 36;
  M4 vp;
  bool cutaway = false;
  json state;
  void load(const std::filesystem::path &path, std::string kind, M4 placement) {
    auto owned = std::make_unique<Model>();
    auto &m = *owned;
    cgltf_options options{};
    if (cgltf_parse_file(&options, path.string().c_str(), &m.data) != cgltf_result_success)
      throw std::runtime_error("Cannot load GLB: " + path.string());
    if (cgltf_load_buffers(&options, m.data, path.string().c_str()) != cgltf_result_success ||
        cgltf_validate(m.data) != cgltf_result_success)
      throw std::runtime_error("Invalid GLB: " + path.string());
    m.kind = std::move(kind);
    m.placement = placement;
    m.world.resize(m.data->nodes_count);
    m.visible.resize(m.data->nodes_count);
    for (size_t i = 0; i < m.data->nodes_count; ++i) {
      auto &n = m.data->nodes[i];
      m.positions.push_back({n.translation[0], n.translation[1], n.translation[2]});
      m.scales.push_back({n.scale[0], n.scale[1], n.scale[2]});
      m.rotations.push_back({n.rotation[0], n.rotation[1], n.rotation[2], n.rotation[3]});
    }
    for (size_t i = 0; i < m.data->textures_count; ++i) {
      const auto &t = m.data->textures[i];
      if (t.image && t.image->buffer_view) {
        int w, h, c;
        auto *px = stbi_load_from_memory(cgltf_buffer_view_data(t.image->buffer_view),
                                         int(t.image->buffer_view->size), &w, &h, &c, 4);
        if (!px)
          throw std::runtime_error("GLB texture decode failed");
        unsigned id;
        gl::GenTextures(1, &id);
        gl::BindTexture(GL_TEXTURE_2D, id);
        gl::TexImage2D(GL_TEXTURE_2D, 0, GL_SRGB8_ALPHA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, px);
        gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        gl::GenerateMipmap(GL_TEXTURE_2D);
        textures[&t] = id;
        stbi_image_free(px);
      }
    }
    for (size_t ni = 0; ni < m.data->nodes_count; ++ni) {
      auto &n = m.data->nodes[ni];
      if (!n.mesh)
        continue;
      for (size_t pi = 0; pi < n.mesh->primitives_count; ++pi) {
        auto &primitive = n.mesh->primitives[pi];
        if (primitive.type != cgltf_primitive_type_triangles)
          throw std::runtime_error("Only triangle primitives supported");
        cgltf_accessor *pos = nullptr;
        cgltf_accessor *normal = nullptr;
        cgltf_accessor *uv = nullptr;
        for (size_t ai = 0; ai < primitive.attributes_count; ++ai) {
          auto &a = primitive.attributes[ai];
          if (a.type == cgltf_attribute_type_position)
            pos = a.data;
          else if (a.type == cgltf_attribute_type_normal)
            normal = a.data;
          else if (a.type == cgltf_attribute_type_texcoord && a.index == 0)
            uv = a.data;
        }
        if (!pos)
          continue;
        std::vector<float> vertices(pos->count * 8);
        Draw d;
        d.model = &m;
        d.node = ni;
        d.min = {1e9f, 1e9f, 1e9f};
        d.max = {-1e9f, -1e9f, -1e9f};
        for (size_t i = 0; i < pos->count; ++i) {
          auto *v = &vertices[i * 8];
          cgltf_accessor_read_float(pos, i, v, 3);
          if (normal)
            cgltf_accessor_read_float(normal, i, v + 3, 3);
          if (uv)
            cgltf_accessor_read_float(uv, i, v + 6, 2);
          d.min = {std::min(d.min.x, v[0]), std::min(d.min.y, v[1]), std::min(d.min.z, v[2])};
          d.max = {std::max(d.max.x, v[0]), std::max(d.max.y, v[1]), std::max(d.max.z, v[2])};
        }
        std::vector<unsigned> indices(primitive.indices ? primitive.indices->count : pos->count);
        for (size_t i = 0; i < indices.size(); ++i)
          indices[i] = primitive.indices ? unsigned(cgltf_accessor_read_index(primitive.indices, i))
                                         : unsigned(i);
        d.count = int(indices.size());
        if (primitive.material) {
          const auto &mat = *primitive.material;
          const auto &pbr = mat.pbr_metallic_roughness;
          std::copy(pbr.base_color_factor, pbr.base_color_factor + 4, d.base);
          d.metal = pbr.metallic_factor;
          d.rough = pbr.roughness_factor;
          d.transparent = mat.alpha_mode == cgltf_alpha_mode_blend;
          d.double_sided = mat.double_sided;
          d.cutoff = mat.alpha_mode == cgltf_alpha_mode_mask ? mat.alpha_cutoff : .01f;
          d.emission = {mat.emissive_factor[0], mat.emissive_factor[1], mat.emissive_factor[2]};
          d.material = mat.name ? mat.name : "";
          if (pbr.base_color_texture.texture)
            d.tex = textures[pbr.base_color_texture.texture];
        }
        // Resolve the asset's coincident finish surfaces in the uploaded vertex
        // buffer. Semantic transforms, original GLB and anchors stay untouched.
        M4 rest;
        cgltf_node_transform_world(&n, rest.m);
        rest = mul(m.placement, rest);
        const V3 ca{rest.m[0], rest.m[1], rest.m[2]}, cb{rest.m[4], rest.m[5], rest.m[6]},
            cc{rest.m[8], rest.m[9], rest.m[10]};
        const float determinant = dot(ca, cross(cb, cc));
        for (const auto &repair : visuals.at("surface_repairs")) {
          if (repair.at("node") != (n.name ? n.name : "") || repair.at("material") != d.material)
            continue;
          for (size_t vi = 0; vi < vertices.size(); vi += 8) {
            auto *v = vertices.data() + vi;
            const auto wp = point(rest, {v[0], v[1], v[2]});
            const auto wn =
                norm((cross(cb, cc) * v[3] + cross(cc, ca) * v[4] + cross(ca, cb) * v[5]) *
                     (1 / determinant));
            float displacement = repair.value("translation_y_m", 0.f);
            if (repair.contains("top_y_m") && wn.y > .99f &&
                std::abs(wp.y - repair["top_y_m"].get<float>()) < .00005f)
              displacement = repair.at("lift_m");
            if (displacement != 0) {
              const V3 delta{0, displacement, 0};
              v[0] += dot(cross(cb, cc), delta) / determinant;
              v[1] += dot(cross(cc, ca), delta) / determinant;
              v[2] += dot(cross(ca, cb), delta) / determinant;
              ++repaired_vertices;
              ++surface_repair_counts[n.name ? n.name : ""];
            }
          }
        }
        bool recolored = false;
        if (visuals.at("materials").contains(d.material)) {
          const auto &rgb = visuals["materials"][d.material];
          for (int k = 0; k < 3; ++k)
            d.base[k] = rgb[k];
          recolored = true;
        }
        if (d.material == "SP_white" || d.material == "SP_Interior_WarmWhite") {
          for (const cgltf_node *ancestor = &n; ancestor; ancestor = ancestor->parent) {
            const std::string name = ancestor->name ? ancestor->name : "";
            if (visuals.at("semantic_body_colors").contains(name)) {
              const auto &rgb = visuals["semantic_body_colors"][name];
              for (int k = 0; k < 3; ++k)
                d.base[k] = rgb[k];
              d.rough = std::max(d.rough, .58f);
              recolored = true;
              break;
            }
          }
        }
        if (recolored)
          ++recolored_draws;
        // Bounds used by picking must match the repaired GPU geometry.
        d.min = {1e9f, 1e9f, 1e9f};
        d.max = {-1e9f, -1e9f, -1e9f};
        for (size_t vi = 0; vi < vertices.size(); vi += 8) {
          const auto *v = vertices.data() + vi;
          d.min = {std::min(d.min.x, v[0]), std::min(d.min.y, v[1]), std::min(d.min.z, v[2])};
          d.max = {std::max(d.max.x, v[0]), std::max(d.max.y, v[1]), std::max(d.max.z, v[2])};
        }
        gl::GenVertexArrays(1, &d.vao);
        gl::BindVertexArray(d.vao);
        gl::GenBuffers(1, &d.vbo);
        gl::BindBuffer(GL_ARRAY_BUFFER, d.vbo);
        gl::BufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(),
                       GL_STATIC_DRAW);
        gl::GenBuffers(1, &d.ibo);
        gl::BindBuffer(GL_ELEMENT_ARRAY_BUFFER, d.ibo);
        gl::BufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned), indices.data(),
                       GL_STATIC_DRAW);
        for (int a = 0; a < 3; ++a) {
          gl::EnableVertexAttribArray(a);
          gl::VertexAttribPointer(
              a, a == 2 ? 2 : 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float),
              reinterpret_cast<void *>(size_t(a == 2 ? 6 : a * 3) * sizeof(float)));
        }
        triangles += d.count / 3;
        draws.push_back(d);
      }
    }
    models.push_back(std::move(owned));
  }
  void reset(Model &m) {
    for (size_t i = 0; i < m.data->nodes_count; ++i) {
      auto &n = m.data->nodes[i];
      std::copy(m.positions[i].begin(), m.positions[i].end(), n.translation);
      std::copy(m.rotations[i].begin(), m.rotations[i].end(), n.rotation);
      std::copy(m.scales[i].begin(), m.scales[i].end(), n.scale);
    }
  }
  void rotate(Model &m, const std::string &name, int axis, float angle) {
    for (size_t i = 0; i < m.data->nodes_count; ++i) {
      auto &n = m.data->nodes[i];
      if (!n.name || name != n.name)
        continue;
      const auto q = m.rotations[i];
      float r[4]{0, 0, 0, std::cos(angle / 2)};
      r[axis] = std::sin(angle / 2);
      n.rotation[0] = q[3] * r[0] + q[0] * r[3] + q[1] * r[2] - q[2] * r[1];
      n.rotation[1] = q[3] * r[1] - q[0] * r[2] + q[1] * r[3] + q[2] * r[0];
      n.rotation[2] = q[3] * r[2] + q[0] * r[1] - q[1] * r[0] + q[2] * r[3];
      n.rotation[3] = q[3] * r[3] - q[0] * r[0] - q[1] * r[1] - q[2] * r[2];
    }
  }
  // Showcase samples glTF channels independently of simulation time. STEP and
  // LINEAR (including shortest-arc quaternion interpolation) cover this asset.
  void sample(Model &m, float time) {
    if (!m.data->animations_count)
      return;
    auto &anim = m.data->animations[0];
    for (size_t i = 0; i < anim.channels_count; ++i) {
      auto &ch = anim.channels[i];
      auto &s = *ch.sampler;
      if (s.interpolation == cgltf_interpolation_type_cubic_spline)
        throw std::runtime_error("Unsupported CUBICSPLINE asset");
      size_t a = 0;
      float t0 = 0, t1 = 0;
      while (a + 1 < s.input->count) {
        cgltf_accessor_read_float(s.input, a + 1, &t1, 1);
        if (t1 > time)
          break;
        ++a;
      }
      size_t b = std::min(a + 1, s.input->count - 1);
      cgltf_accessor_read_float(s.input, a, &t0, 1);
      cgltf_accessor_read_float(s.input, b, &t1, 1);
      float u = s.interpolation == cgltf_interpolation_type_step
                    ? 0
                    : std::clamp((time - t0) / std::max(1e-6f, t1 - t0), 0.f, 1.f);
      float av[4]{}, bv[4]{}, v[4]{};
      int count = ch.target_path == cgltf_animation_path_type_rotation ? 4 : 3;
      cgltf_accessor_read_float(s.output, a, av, count);
      cgltf_accessor_read_float(s.output, b, bv, count);
      if (count == 4) {
        float d = 0;
        for (int k = 0; k < 4; ++k)
          d += av[k] * bv[k];
        if (d < 0) {
          for (float &f : bv)
            f = -f;
          d = -d;
        }
        if (d < .9995f) {
          float th = std::acos(std::clamp(d, -1.f, 1.f));
          for (int k = 0; k < 4; ++k)
            v[k] = (std::sin((1 - u) * th) * av[k] + std::sin(u * th) * bv[k]) / std::sin(th);
        } else {
          float len = 0;
          for (int k = 0; k < 4; ++k) {
            v[k] = (1 - u) * av[k] + u * bv[k];
            len += v[k] * v[k];
          }
          for (float &f : v)
            f /= std::sqrt(std::max(len, 1e-9f));
        }
      } else
        for (int k = 0; k < 3; ++k)
          v[k] = (1 - u) * av[k] + u * bv[k];
      if (ch.target_path == cgltf_animation_path_type_rotation)
        std::copy(v, v + 4, ch.target_node->rotation);
      else if (ch.target_path == cgltf_animation_path_type_translation)
        std::copy(v, v + 3, ch.target_node->translation);
      else if (ch.target_path == cgltf_animation_path_type_scale)
        std::copy(v, v + 3, ch.target_node->scale);
    }
  }
  void compute(Model &m, size_t index, const M4 &parent, bool shown) {
    auto &n = m.data->nodes[index];
    M4 local;
    cgltf_node_transform_local(&n, local.m);
    m.world[index] = mul(parent, local);
    const std::string name = n.name ? n.name : "";
    bool on = shown && !hidden.count(name);
    if (n.extras.data) {
      const auto e = json::parse(n.extras.data, nullptr, false);
      if (e.is_object() && e.contains("slx_default_visible") &&
          !e["slx_default_visible"].get<bool>())
        on = false;
    }
    // These are the two explicit optional roots activated by the chosen preset.
    if (m.overrides.count(name))
      on = shown && m.overrides.at(name) && !hidden.count(name);
    m.visible[index] = on;
    for (size_t i = 0; i < n.children_count; ++i)
      compute(m, size_t(n.children[i] - m.data->nodes), m.world[index], on);
  }
};
Renderer::Renderer(const std::filesystem::path &root) : p(std::make_unique<RendererImpl>()) {
  const auto start = std::chrono::steady_clock::now();
  p->visuals = read_json(root.parent_path() / "config/visuals.json");
  p->exposure = p->visuals.value("exposure", .95f);
  p->shader = shader_program(vertex, fragment);
  p->load(root / "scene/SLX_SolarPilot_Runtime.glb", "runtime", M4::identity());
  const auto contract = read_json(root / "config/scene_contract.json");
  std::set<std::string> names;
  for (size_t i = 0; i < p->models[0]->data->nodes_count; ++i)
    if (p->models[0]->data->nodes[i].name)
      names.insert(p->models[0]->data->nodes[i].name);
  for (const auto &n : contract["runtime_requirements"]["required_nodes"])
    if (!names.count(n.get<std::string>()))
      throw std::runtime_error("Missing scene node: " + n.get<std::string>());
  const auto appliances = read_json(root / "config/appliance_asset_contract.json");
  for (const auto &[id, device] : appliances["devices"].items())
    for (const auto &a : device["articulations"])
      if (a["role"] != "washer_drum_rotation")
        p->articulations.push_back(a);
  const auto equipment = read_json(root / "config/equipment_asset_contract.json");
  for (const auto &a : equipment["mechanisms"])
    if (a.value("maintenance_only", false) && a.contains("range_deg"))
      p->articulations.push_back({{"node", a["node"]},
                                  {"min_deg", a["range_deg"][0]},
                                  {"max_deg", a["range_deg"][1]},
                                  {"axis_local_gltf", {0, 1, 0}}});
  const auto manifest = read_json(root / "config/components_manifest.json");
  for (const auto &c : manifest["components"]) {
    const std::string file = c["file"];
    const bool north = file.find("PV_North16") != std::string::npos,
               south = file.find("PV_South10_Wiring") != std::string::npos;
    if (!north && !south)
      continue;
    M4 placement;
    for (int i = 0; i < 16; ++i)
      placement.m[i] = c["scene_placement_gltf_matrix_column_major"][i];
    p->load(root / file, north ? "north" : "south10", placement);
    for (const auto &[name, value] : c["visibility_overrides_when_activated"].items())
      p->models.back()->overrides[name] = value.get<bool>();
  }
  const auto preset = read_json(root / "config/pv_configuration_presets.json");
  for (const auto &n : preset["presets"]["south_10"]["module_roots"])
    p->south10.insert(n.get<std::string>());
  gl::GenFramebuffers(1, &p->fbo);
  gl::GenFramebuffers(1, &p->ms_fbo);
  gl::GenTextures(1, &p->target);
  gl::GenRenderbuffers(1, &p->depth);
  gl::GenRenderbuffers(1, &p->ms_color);
  p->load_ms =
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
}
Renderer::~Renderer() {
  for (auto &d : p->draws) {
    gl::DeleteVertexArrays(1, &d.vao);
    gl::DeleteBuffers(1, &d.vbo);
    gl::DeleteBuffers(1, &d.ibo);
  }
  for (auto [key, id] : p->textures)
    gl::DeleteTextures(1, &id);
  for (unsigned id : p->product_images)
    gl::DeleteTextures(1, &id);
  gl::DeleteTextures(1, &p->target);
  gl::DeleteRenderbuffers(1, &p->depth);
  gl::DeleteRenderbuffers(1, &p->ms_color);
  gl::DeleteFramebuffers(1, &p->fbo);
  gl::DeleteFramebuffers(1, &p->ms_fbo);
  gl::DeleteProgram(p->shader);
}
unsigned Renderer::image_file(const std::filesystem::path &path) {
  int w = 0, h = 0, c = 0;
  auto *px = stbi_load(path.string().c_str(), &w, &h, &c, 4);
  if (!px)
    throw std::runtime_error("Product image unavailable: " + path.string());
  unsigned id = 0;
  gl::GenTextures(1, &id);
  gl::BindTexture(GL_TEXTURE_2D, id);
  gl::TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, px);
  gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  stbi_image_free(px);
  p->product_images.push_back(id);
  return id;
}
unsigned Renderer::render(int width, int height, const json &state, bool cutaway, bool showcase,
                          double clip_s, const json &mechanism_pose) {
  const auto start = std::chrono::steady_clock::now();
  width = std::clamp(width, 64, 2560);
  height = std::clamp(height, 64, 1600);
  p->state = state;
  p->cutaway = cutaway;
  if (width != p->width || height != p->height) {
    p->width = width;
    p->height = height;
    gl::BindTexture(GL_TEXTURE_2D, p->target);
    gl::TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                   nullptr);
    gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    gl::BindFramebuffer(GL_FRAMEBUFFER, p->fbo);
    gl::FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, p->target, 0);
    gl::BindRenderbuffer(GL_RENDERBUFFER, p->depth);
    gl::RenderbufferStorageMultisample(GL_RENDERBUFFER, 4, GL_DEPTH_COMPONENT32F, width, height);
    gl::BindRenderbuffer(GL_RENDERBUFFER, p->ms_color);
    gl::RenderbufferStorageMultisample(GL_RENDERBUFFER, 4, GL_RGBA8, width, height);
    gl::BindFramebuffer(GL_FRAMEBUFFER, p->ms_fbo);
    gl::FramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, p->ms_color);
    gl::FramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, p->depth);
  }
  p->hidden.clear();
  if (cutaway)
    for (const char *n : {"roof_south_root", "roof_north_root", "upper_floor_root",
                          "house_front_facade_root", "garage_roof_root", "pv_system_root"})
      p->hidden.insert(n);
  const auto &cfg = state.at("config");
  const bool small = cfg["south_count"] == 10;
  p->hidden.insert(small ? "pv_wiring_south_16_root" : "pv_wiring_south_10_root");
  if (small)
    for (int i = 1; i <= 16; ++i) {
      const std::string name =
          "pv_south_" + (i < 10 ? std::string("0") : std::string()) + std::to_string(i) + "_root";
      if (!p->south10.count(name))
        p->hidden.insert(name);
    }
  for (auto &owned : p->models) {
    auto &m = *owned;
    p->reset(m);
    if (showcase && m.kind == "runtime")
      p->sample(m, float(std::fmod(clip_s, 60.)));
    if (!showcase && m.kind == "runtime") {
      for (const auto &a : p->articulations) {
        const std::string name = a["node"];
        if (!mechanism_pose.contains(name))
          continue;
        bool locked = false;
        for (const auto &l : state["devices"])
          if (name.starts_with(l["id"].get<std::string>()) && l["running"].get<bool>())
            locked = true;
        if (locked)
          continue;
        const auto axis = a["axis_local_gltf"].get<std::array<float, 3>>();
        int component = std::abs(axis[0]) > .5f ? 0 : std::abs(axis[1]) > .5f ? 1 : 2;
        const float angle = std::clamp(mechanism_pose[name].get<float>(), a["min_deg"].get<float>(),
                                       a["max_deg"].get<float>());
        p->rotate(m, name, component, angle * .01745329252f * axis[component]);
      }
      const bool live = state["link"].value("fresh", false);
      if (live) {
        if (state["devices"][1]["running"].get<bool>())
          p->rotate(
              m, "washer_drum_pivot", 2,
              float(std::fmod(-state["devices"][1]["elapsed_s"].get<double>() * 2, 6.283185307)));
        if (state["pv_w"].get<double>() > 100)
          p->rotate(m, "inverter_fan_rotor", 2,
                    float(std::fmod(-state["sim_s"].get<double>() * 3, 6.283185307)));
      }
    }
    const bool active = m.kind == "runtime" || (m.kind == "south10" && small && !cutaway) ||
                        (m.kind == "north" && cfg["north"].get<bool>() && !cutaway);
    for (size_t i = 0; i < m.data->nodes_count; ++i)
      if (!m.data->nodes[i].parent)
        p->compute(m, i, m.placement, active);
  }
  p->eye = p->center + V3{std::sin(p->yaw) * std::cos(p->pitch), std::sin(p->pitch),
                          std::cos(p->yaw) * std::cos(p->pitch)} *
                           p->radius;
  p->eye.y = std::max(.2f, p->eye.y);
  p->vp = mul(perspective(float(width) / height), look(p->eye, p->center));
  const double hour = state["hour"];
  const float daylight = float(std::clamp(std::sin((hour - 6) * 3.14159265 / 12), .06, 1.));
  gl::BindFramebuffer(GL_FRAMEBUFFER, p->ms_fbo);
  gl::Viewport(0, 0, width, height);
  gl::Disable(GL_SCISSOR_TEST);
  gl::Enable(GL_DEPTH_TEST);
  gl::DepthFunc(GL_LESS);
  gl::Disable(GL_BLEND);
  gl::DepthMask(GL_TRUE);
  gl::ClearColor(.075f + .31f * daylight, .10f + .37f * daylight, .15f + .43f * daylight, 1);
  gl::Clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  gl::UseProgram(p->shader);
  gl::UniformMatrix4fv(gl::GetUniformLocation(p->shader, "vp"), 1, GL_FALSE, p->vp.m);
  gl::Uniform3f(gl::GetUniformLocation(p->shader, "eye"), p->eye.x, p->eye.y, p->eye.z);
  gl::Uniform1f(gl::GetUniformLocation(p->shader, "daylight"), daylight);
  gl::Uniform1f(gl::GetUniformLocation(p->shader, "exposure"), p->exposure);
  gl::Uniform1i(gl::GetUniformLocation(p->shader, "tex"), 0);
  gl::ActiveTexture(GL_TEXTURE0);
  std::vector<const RendererImpl::Draw *> ordered;
  for (const auto &d : p->draws)
    if (d.model->visible[d.node])
      ordered.push_back(&d);
  std::stable_sort(ordered.begin(), ordered.end(), [&](auto *a, auto *b) {
    if (a->transparent != b->transparent)
      return !a->transparent;
    if (!a->transparent)
      return false;
    const auto av = point(a->model->world[a->node], (a->min + a->max) * .5f) - p->eye,
               bv = point(b->model->world[b->node], (b->min + b->max) * .5f) - p->eye;
    return dot(av, av) > dot(bv, bv);
  });
  for (const auto *d : ordered) {
    if (d->transparent) {
      gl::Enable(GL_BLEND);
      gl::BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
      gl::DepthMask(GL_FALSE);
    }
    if (d->double_sided)
      gl::Disable(GL_CULL_FACE);
    else {
      gl::Enable(GL_CULL_FACE);
      gl::CullFace(GL_BACK);
    }
    float base[4];
    std::copy(d->base, d->base + 4, base);
    V3 emission = d->emission;
    for (const auto &load : state["devices"]) {
      const std::string mat = "SP_" + load["id"].get<std::string>() + "_root_status";
      if (mat == d->material) {
        const bool on = load["power_w"].get<double>() > 10;
        const bool available = load["available"];
        base[0] = available ? .05f : .85f;
        base[1] = available ? (on ? .75f : .13f) : .03f;
        base[2] = .1f;
        emission = V3{base[0], base[1], base[2]} * (on ? .6f : .1f);
      }
    }
    gl::UniformMatrix4fv(gl::GetUniformLocation(p->shader, "model"), 1, GL_FALSE,
                         d->model->world[d->node].m);
    gl::Uniform4fv(gl::GetUniformLocation(p->shader, "base"), 1, base);
    gl::Uniform3f(gl::GetUniformLocation(p->shader, "emission"), emission.x, emission.y,
                  emission.z);
    gl::Uniform1f(gl::GetUniformLocation(p->shader, "metallic"), d->metal);
    gl::Uniform1f(gl::GetUniformLocation(p->shader, "roughness"), d->rough);
    gl::Uniform1f(gl::GetUniformLocation(p->shader, "cutoff"), d->cutoff);
    gl::Uniform1i(gl::GetUniformLocation(p->shader, "hasTex"), d->tex ? 1 : 0);
    gl::BindTexture(GL_TEXTURE_2D, d->tex);
    gl::BindVertexArray(d->vao);
    gl::DrawElements(GL_TRIANGLES, d->count, GL_UNSIGNED_INT, nullptr);
  }
  gl::Disable(GL_CULL_FACE);
  gl::Disable(GL_BLEND);
  gl::DepthMask(GL_TRUE);
  gl::BindVertexArray(0);
  gl::BindFramebuffer(GL_READ_FRAMEBUFFER, p->ms_fbo);
  gl::BindFramebuffer(GL_DRAW_FRAMEBUFFER, p->fbo);
  gl::BlitFramebuffer(0, 0, width, height, 0, 0, width, height, GL_COLOR_BUFFER_BIT, GL_NEAREST);
  gl::BindFramebuffer(GL_FRAMEBUFFER, 0);
  p->render_ms =
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
  return p->target;
}
void Renderer::exposure(float value) {
  p->exposure = std::clamp(value, .45f, 1.6f);
}
void Renderer::orbit(float dx, float dy) {
  p->yaw -= dx * .007f;
  p->pitch = std::clamp(p->pitch + dy * .007f, -.15f, 1.5f);
}
void Renderer::zoom(float amount) {
  p->radius = std::clamp(p->radius * std::exp(-amount * .12f), 2.f, 100.f);
}
void Renderer::focus(int preset) {
  p->yaw = .6f;
  p->pitch = .45f;
  if (preset == 1) {
    p->center = {-1, 6, 0};
    p->radius = 20;
    p->pitch = 1.0f;
  } else if (preset == 2) {
    p->center = {4, 1.3f, -3.5f};
    p->radius = 6;
    p->pitch = .65f;
    p->yaw = 0;
  } else if (preset == 3) {
    p->center = {-4.5f, 1.1f, -3.5f};
    p->radius = 7;
    p->pitch = .7f;
    p->yaw = 0;
  } else if (preset == 4) {
    p->center = {9.5f, 1, 3.5f};
    p->radius = 10;
    p->yaw = -.7f;
  } else if (preset == 5) {
    p->center = {.65f, 1.7f, -2.2f};
    p->radius = 7;
    p->pitch = .65f;
    p->yaw = 0;
  } else {
    p->center = {0, 2, 0};
    p->radius = 36;
  }
}
std::string Renderer::pick(float x, float y) const {
  const auto f = norm(p->center - p->eye), right = norm(cross(f, {0, 1, 0})), up = cross(right, f);
  const auto ray = norm(f + right * ((2 * x - 1) * std::tan(.392699f) * p->width / p->height) +
                        up * ((1 - 2 * y) * std::tan(.392699f)));
  float closest = 1e9f;
  std::string selected;
  for (const auto &d : p->draws) {
    if (!d.model->visible[d.node])
      continue;
    std::string semantic;
    for (auto *node = &d.model->data->nodes[d.node]; node; node = node->parent) {
      const std::string n = node->name ? node->name : "";
      for (const char *id : {"fridge", "washer", "dishwasher", "oven", "cooktop", "kettle",
                             "battery", "inverter", "wallbox", "ev"})
        if (n == std::string(id) + "_root")
          semantic = id;
      if (n == "pv_system_root" || n == "pv_south_array_root" || n == "pv_north_array_root")
        semantic = "pv";
    }
    V3 lo{1e9f, 1e9f, 1e9f}, hi{-1e9f, -1e9f, -1e9f};
    for (int k = 0; k < 8; ++k) {
      auto v = point(d.model->world[d.node], {k & 1 ? d.max.x : d.min.x, k & 2 ? d.max.y : d.min.y,
                                              k & 4 ? d.max.z : d.min.z});
      lo = {std::min(lo.x, v.x), std::min(lo.y, v.y), std::min(lo.z, v.z)};
      hi = {std::max(hi.x, v.x), std::max(hi.y, v.y), std::max(hi.z, v.z)};
    }
    const float origin[3]{p->eye.x, p->eye.y, p->eye.z}, dir[3]{ray.x, ray.y, ray.z},
        l[3]{lo.x, lo.y, lo.z}, h[3]{hi.x, hi.y, hi.z};
    float t0 = 0, t1 = 1e9f;
    for (int a = 0; a < 3; ++a) {
      if (std::abs(dir[a]) < 1e-8f) {
        if (origin[a] < l[a] || origin[a] > h[a])
          t1 = -1;
      } else {
        float a0 = (l[a] - origin[a]) / dir[a], a1 = (h[a] - origin[a]) / dir[a];
        if (a0 > a1)
          std::swap(a0, a1);
        t0 = std::max(t0, a0);
        t1 = std::min(t1, a1);
      }
    }
    if (t0 <= t1 && t0 < closest) {
      closest = t0;
      selected = semantic;
    }
  }
  return selected;
}
json Renderer::diagnostics() const {
  int south = 0, north = 0, visible = 0;
  bool wiring10 = false, wiring16 = false;
  for (const auto &m : p->models)
    for (size_t i = 0; i < m->data->nodes_count; ++i) {
      if (!m->visible[i])
        continue;
      const std::string name = m->data->nodes[i].name ? m->data->nodes[i].name : "";
      if (name.starts_with("pv_south_") && name.ends_with("_root") && name.size() == 16 &&
          m->kind == "runtime")
        ++south;
      if (name.starts_with("pv_north_") && name.ends_with("_root") && name.size() == 16 &&
          m->kind == "north")
        ++north;
      if (name == "pv_wiring_south_10_root" && m->kind == "south10")
        wiring10 = true;
      if (name == "pv_wiring_south_16_root" && m->kind == "runtime")
        wiring16 = true;
    }
  for (const auto &d : p->draws)
    if (d.model->visible[d.node])
      ++visible;
  return {{"surface_repair_vertices", p->repaired_vertices},
          {"surface_repairs", p->surface_repair_counts},
          {"recolored_draws", p->recolored_draws},
          {"depth_format", "D32F"},
          {"exposure", p->exposure},
          {"models", p->models.size()},
          {"draws", p->draws.size()},
          {"visible_draws", visible},
          {"visible_south_modules", south},
          {"visible_north_modules", north},
          {"south10_wiring", wiring10},
          {"south16_wiring", wiring16},
          {"triangles", p->triangles},
          {"load_ms", p->load_ms},
          {"cpu_submit_ms", p->render_ms},
          {"gl_vendor", reinterpret_cast<const char *>(gl::GetString(GL_VENDOR))},
          {"gl_renderer", reinterpret_cast<const char *>(gl::GetString(GL_RENDERER))}};
}
void Renderer::screenshot(const std::filesystem::path &path, int width, int height) {
  std::vector<unsigned char> pixels(size_t(width) * height * 4);
  gl::BindFramebuffer(GL_FRAMEBUFFER, 0);
  gl::ReadBuffer(GL_BACK);
  gl::PixelStorei(GL_PACK_ALIGNMENT, 1);
  gl::ReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
  stbi_flip_vertically_on_write(1);
  if (!stbi_write_png(path.string().c_str(), width, height, 4, pixels.data(), width * 4))
    throw std::runtime_error("Screenshot write failed");
}
} // namespace slx::desktop
