#pragma once

#include <functional>
#include <string>
#include <vector>

#include "build.hpp"
#include "plan.hpp"
#include "scene.hpp"

namespace test {

inline s2s::Plan fixture(const std::string& name) {
    return s2s::load_plan(std::string(S2S_FIXTURES_DIR) + "/plans/" + name + ".json");
}

inline const s2s::Group& level_group(const s2s::Scene& scene, size_t i = 0) { return scene.groups.at(0).groups.at(i); }

inline s2s::Wall& wall(s2s::Plan& plan, const std::string& id) {
    for (s2s::Wall& w : plan.levels[0].walls)
        if (w.id == id) return w;
    throw std::runtime_error("no wall " + id);
}

inline s2s::Opening& opening(s2s::Plan& plan, const std::string& id) {
    for (s2s::Opening& o : plan.levels[0].openings)
        if (o.id == id) return o;
    throw std::runtime_error("no opening " + id);
}

// Visits every group in the scene with its parent chain depth.
inline void visit(const s2s::Group& g, const std::function<void(const s2s::Group&, int)>& fn, int depth = 0) {
    fn(g, depth);
    for (const s2s::Group& c : g.groups) visit(c, fn, depth + 1);
}

}  // namespace test
