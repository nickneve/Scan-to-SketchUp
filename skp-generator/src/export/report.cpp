#include <cstdio>
#include <functional>
#include <map>
#include <sstream>

#include "exporters.hpp"
#include "mesh.hpp"
#include "units.hpp"

namespace s2s {

namespace {

std::string num(double v, const char* f = "%.2f") {
    char buf[32];
    std::snprintf(buf, sizeof buf, f, v);
    return buf;
}

std::string solid_note(const std::vector<Face>& faces) {
    if (faces.size() < 4) return "";
    SolidReport r = check_solid(faces);
    return r.closed ? ", solid" : ", NOT SOLID (" + r.problems.front() + ")";
}

}  // namespace

std::string scene_report(const Scene& scene) {
    std::ostringstream out;
    out << "Scan-to-SketchUp scene report\n=============================\n";
    out << "Display units: " << (scene.units == DisplayUnits::Metric ? "metric" : "imperial") << "\n";
    out << "North angle: " << num(scene.northAngleDeg, "%.1f") << " deg clockwise from green (+Y)\n";
    out << "Geo-location: "
        << (scene.location ? num(scene.location->lat, "%.6f") + ", " + num(scene.location->lon, "%.6f") : "not set")
        << "\n\nTags\n";
    for (const Tag& t : scene.tags) out << "  " << t.name << (t.visible ? "" : "  (hidden)") << "\n";

    std::map<std::string, int> instanceCount;
    std::function<void(const Group&)> count = [&](const Group& g) {
        for (const Instance& i : g.instances) ++instanceCount[i.definition];
        for (const Group& c : g.groups) count(c);
    };
    for (const Group& g : scene.groups) count(g);

    out << "\nOutliner\n";
    std::function<void(const Group&, int)> tree = [&](const Group& g, int depth) {
        const std::string pad(2 * depth + 2, ' ');
        out << pad << g.name;
        if (!g.tag.empty()) out << "  [" << g.tag << "]";
        std::vector<std::string> notes;
        if (!g.faces.empty()) {
            double area = 0;
            for (const Face& f : g.faces) area += face_area(f);
            notes.push_back(std::to_string(g.faces.size()) + (g.faces.size() == 1 ? " face" : " faces") +
                            solid_note(g.faces));
            if (g.faces.size() == 1) notes.push_back(format_area(area, scene.units));
        }
        if (!g.dimensions.empty()) notes.push_back(std::to_string(g.dimensions.size()) + " dimensions");
        if (auto it = g.attributes.find("id"); it != g.attributes.end()) notes.push_back("id " + it->second);
        if (!notes.empty()) {
            out << "  --";
            for (size_t i = 0; i < notes.size(); ++i) out << (i ? ", " : " ") << notes[i];
        }
        out << "\n";
        for (const Group& c : g.groups) tree(c, depth + 1);
        for (const Instance& i : g.instances) {
            out << pad << "  <" << i.definition << ">  [" << i.tag << "]";
            if (auto it = i.attributes.find("id"); it != i.attributes.end()) out << "  -- id " << it->second;
            if (i.transform.determinant() < 0) out << ", mirrored";
            out << "\n";
        }
    };
    for (const Group& g : scene.groups) tree(g, 0);

    out << "\nComponent definitions\n";
    for (const ComponentDefinition& d : scene.definitions) {
        out << "  " << d.name << " -- " << d.description << "; " << instanceCount[d.name] << " instance(s)\n";
        for (const Group& g : d.contents.groups) {
            out << "      " << g.name;
            if (!g.tag.empty()) out << "  [" << g.tag << "]";
            if (!g.faces.empty()) out << "  -- " << g.faces.size() << " faces" << solid_note(g.faces);
            else if (!g.arcs.empty() || !g.edges.empty()) out << "  -- plan symbol (edges)";
            out << "\n";
        }
    }

    out << "\nWarnings\n";
    if (scene.warnings.empty()) out << "  (none)\n";
    for (const std::string& w : scene.warnings) out << "  - " << w << "\n";
    return out.str();
}

}  // namespace s2s
