#include "flatten.hpp"

#include <algorithm>
#include <cmath>
#include <map>

namespace s2s {

namespace {

Vec3 linear(const Transform& t, Vec3 v) { return t.xaxis * v.x + t.yaxis * v.y + t.zaxis * v.z; }

Transform compose(const Transform& outer, const Transform& inner) {
    return {outer.apply(inner.origin), linear(outer, inner.xaxis), linear(outer, inner.yaxis), linear(outer, inner.zaxis)};
}

bool is_category(const std::string& name) {
    return name == "Walls" || name == "Floors" || name == "Ceilings" || name == "Doors" || name == "Windows" ||
           name == "Dimensions";
}

struct Walker {
    const Scene& scene;
    bool includeHidden;
    std::vector<FlatPart> parts;

    bool hidden(const std::string& tag) const {
        const Tag* t = scene.find_tag(tag);
        return t && !t->visible;
    }

    void walk(const Group& g, const Transform& xf, const std::string& path, const std::string& inheritedTag,
              std::string kind) {
        const std::string tag = g.tag.empty() ? inheritedTag : g.tag;
        if (!includeHidden && hidden(tag)) return;
        if (kind.empty() && is_category(g.name)) kind = g.name;
        const bool mirrored = xf.determinant() < 0;

        std::map<std::string, FlatPart> byMaterial;
        for (const Face& f : g.faces) {
            FlatPart& part = byMaterial[f.material];
            for (Triangle t : triangulate(f)) {
                for (Vec3& p : t) p = xf.apply(p);
                if (mirrored) std::swap(t[1], t[2]);
                part.triangles.push_back(t);
            }
        }
        std::vector<std::vector<Vec3>> lines;
        for (const auto& e : g.edges) {
            std::vector<Vec3> l;
            for (Vec3 p : e) l.push_back(xf.apply(p));
            lines.push_back(l);
        }
        for (const Arc& a : g.arcs) {
            std::vector<Vec3> l;
            for (Vec3 p : sample_arc(a)) l.push_back(xf.apply(p));
            lines.push_back(l);
        }
        if (!lines.empty()) byMaterial[""].lines = std::move(lines);
        for (auto& [material, part] : byMaterial) {
            part.path = path;
            part.tag = tag;
            part.material = material;
            part.kind = kind;
            parts.push_back(std::move(part));
        }

        for (const Group& child : g.groups) walk(child, xf, path + "/" + child.name, tag, kind);
        for (const Instance& inst : g.instances) {
            const ComponentDefinition* def = scene.find_definition(inst.definition);
            if (!def) continue;
            const std::string instTag = inst.tag.empty() ? tag : inst.tag;
            if (!includeHidden && hidden(instTag)) continue;
            walk(def->contents, compose(xf, inst.transform), path + "/" + inst.definition, instTag, kind);
        }
    }
};

}  // namespace

std::vector<Vec3> sample_arc(const Arc& a) {
    const Vec3 y = cross(a.normal, a.xaxis);
    std::vector<Vec3> pts;
    for (int i = 0; i <= a.segments; ++i) {
        const double t = a.startAngle + (a.endAngle - a.startAngle) * i / a.segments;
        pts.push_back(a.center + a.xaxis * (a.radius * std::cos(t)) + y * (a.radius * std::sin(t)));
    }
    return pts;
}

std::vector<FlatPart> flatten(const Scene& scene, bool includeHidden) {
    Walker w{scene, includeHidden, {}};
    for (const Group& g : scene.groups) w.walk(g, Transform{}, g.name, "", "");
    return std::move(w.parts);
}

}  // namespace s2s
