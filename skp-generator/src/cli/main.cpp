// housegen: HousePlan JSON -> SketchUp model (and previews).
//
//   housegen PLAN.json [--out DIR] [--obj] [--svg] [--html] [--report] [--skp]
//
// With no format flags, writes every preview. --skp needs a build configured with the SketchUp C SDK.
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "build.hpp"
#include "exporters.hpp"
#if S2S_HAVE_SKP
#include "skp_writer.hpp"
#endif

namespace fs = std::filesystem;

namespace {

int usage() {
    std::cerr << "usage: housegen PLAN.json [--out DIR] [--obj] [--svg] [--html] [--report] [--skp]\n";
    return 2;
}

void write_text(const fs::path& path, const std::string& text) {
    std::ofstream out(path, std::ios::binary);
    if (!out) throw std::runtime_error("cannot write " + path.string());
    out << text;
}

}  // namespace

int main(int argc, char** argv) {
    std::vector<std::string> args(argv + 1, argv + argc);
    std::string planPath;
    fs::path outDir = ".";
    bool obj = false, svg = false, html = false, report = false, skp = false;
    for (size_t i = 0; i < args.size(); ++i) {
        const std::string& a = args[i];
        if (a == "--out" && i + 1 < args.size()) outDir = args[++i];
        else if (a == "--obj") obj = true;
        else if (a == "--svg") svg = true;
        else if (a == "--html") html = true;
        else if (a == "--report") report = true;
        else if (a == "--skp") skp = true;
        else if (!a.empty() && a[0] == '-') return usage();
        else if (planPath.empty()) planPath = a;
        else return usage();
    }
    if (planPath.empty()) return usage();
    if (!obj && !svg && !html && !report && !skp) obj = svg = html = report = true;

    try {
        const s2s::Plan plan = s2s::load_plan(planPath);
        const s2s::Scene scene = s2s::build_scene(plan);
        fs::create_directories(outDir);
        const std::string stem = fs::path(planPath).stem().string();

        if (skp) {
#if S2S_HAVE_SKP
            const fs::path p = outDir / (stem + ".skp");
            s2s::write_skp(scene, p.string());
            std::cout << "wrote " << p.string() << "\n";
#else
            std::cerr << "housegen: .skp output needs the SketchUp C SDK; configure with -DS2S_SKETCHUP_SDK_DIR=<sdk>\n";
            return 3;
#endif
        }
        if (obj) {
            const fs::path p = outDir / (stem + ".obj");
            s2s::write_obj(scene, p.string());
            std::cout << "wrote " << p.string() << "\n";
        }
        if (svg)
            for (size_t i = 0; i < plan.levels.size(); ++i) {
                const fs::path p = outDir / (stem + "-plan-L" + std::to_string(i + 1) + ".svg");
                write_text(p, s2s::plan_svg(plan, scene, i));
                std::cout << "wrote " << p.string() << "\n";
            }
        if (html) {
            const fs::path p = outDir / (stem + "-3d.html");
            write_text(p, s2s::viewer_html(scene, stem));
            std::cout << "wrote " << p.string() << "\n";
        }
        if (report) {
            const fs::path p = outDir / (stem + "-report.txt");
            write_text(p, s2s::scene_report(scene));
            std::cout << "wrote " << p.string() << "\n";
        }
        for (const std::string& w : scene.warnings) std::cout << "warning: " << w << "\n";
        return 0;
    } catch (const s2s::PlanError& e) {
        std::cerr << "housegen: " << planPath << ": " << e.what() << "\n";
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "housegen: " << e.what() << "\n";
        return 1;
    }
}
