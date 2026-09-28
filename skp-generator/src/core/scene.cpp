#include "scene.hpp"

namespace s2s {

const Group* Group::find(const std::string& childName) const {
    for (const Group& g : groups)
        if (g.name == childName) return &g;
    return nullptr;
}

const Tag* Scene::find_tag(const std::string& name) const {
    for (const Tag& t : tags)
        if (t.name == name) return &t;
    return nullptr;
}

const ComponentDefinition* Scene::find_definition(const std::string& name) const {
    for (const ComponentDefinition& d : definitions)
        if (d.name == name) return &d;
    return nullptr;
}

}  // namespace s2s
