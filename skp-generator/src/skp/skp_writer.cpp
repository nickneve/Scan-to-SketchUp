// Written against the SketchUp C API reference (extensions.sketchup.com/developers/sketchup_c_api)
// before SDK access was granted, so it has not been compiled yet. Items to confirm on first
// build are marked VERIFY.
#include "skp_writer.hpp"

#include <cmath>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

#include <SketchUpAPI/sketchup.h>

namespace s2s {

namespace {

constexpr double kInchesPerMm = 1.0 / 25.4;  // SketchUp stores lengths in inches

void check(SUResult r, const char* call) {
    if (r != SU_ERROR_NONE) throw std::runtime_error(std::string("SketchUp SDK: ") + call + " failed (" + std::to_string(r) + ")");
}
#define SU(call) check(call, #call)

SUPoint3D pt(Vec3 p) { return {p.x * kInchesPerMm, p.y * kInchesPerMm, p.z * kInchesPerMm}; }
SUVector3D vec(Vec3 v) { return {v.x, v.y, v.z}; }

SUTransformation transformation(const Transform& t) {
    // Column-major 4x4: columns are the x, y, z axes and the translation (in inches).
    const Vec3 o = t.origin * kInchesPerMm;
    return {{t.xaxis.x, t.xaxis.y, t.xaxis.z, 0,  //
             t.yaxis.x, t.yaxis.y, t.yaxis.z, 0,  //
             t.zaxis.x, t.zaxis.y, t.zaxis.z, 0,  //
             o.x,       o.y,       o.z,       1}};
}

class TypedValue {
public:
    TypedValue() { SU(SUTypedValueCreate(&ref_)); }
    ~TypedValue() { SUTypedValueRelease(&ref_); }
    TypedValue(const TypedValue&) = delete;
    TypedValue& operator=(const TypedValue&) = delete;
    SUTypedValueRef get() const { return ref_; }

private:
    SUTypedValueRef ref_ = SU_INVALID;
};

class Writer {
public:
    explicit Writer(const Scene& scene) : scene_(scene) {}

    void write(const std::string& path) {
        SUInitialize();
        SU(SUModelCreate(&model_));
        try {
            add_tags();
            add_materials();
            add_definitions();
            SUEntitiesRef root = SU_INVALID;
            SU(SUModelGetEntities(model_, &root));
            for (const Group& g : scene_.groups) add_group(root, g);
            set_location();
            set_units();
            SU(SUModelSaveToFile(model_, path.c_str()));
        } catch (...) {
            SUModelRelease(&model_);
            SUTerminate();
            throw;
        }
        SUModelRelease(&model_);
        SUTerminate();
    }

private:
    const Scene& scene_;
    SUModelRef model_ = SU_INVALID;
    std::map<std::string, SULayerRef> layers_;
    std::map<std::string, SUMaterialRef> materials_;
    std::map<std::string, SUComponentDefinitionRef> definitions_;

    void add_tags() {
        for (const Tag& t : scene_.tags) {
            SULayerRef layer = SU_INVALID;
            SU(SULayerCreate(&layer));
            SU(SULayerSetName(layer, t.name.c_str()));
            SU(SULayerSetVisibility(layer, t.visible));
            SU(SUModelAddLayers(model_, 1, &layer));
            layers_[t.name] = layer;
        }
    }

    void add_materials() {
        for (const Material& m : scene_.materials) {
            SUMaterialRef mat = SU_INVALID;
            SU(SUMaterialCreate(&mat));
            SU(SUMaterialSetName(mat, m.name.c_str()));
            SUColor color{static_cast<SUByte>(m.r), static_cast<SUByte>(m.g), static_cast<SUByte>(m.b), 255};
            SU(SUMaterialSetColor(mat, &color));
            if (m.alpha < 1) {
                SU(SUMaterialSetOpacity(mat, m.alpha));
                SU(SUMaterialSetUseOpacity(mat, true));
            }
            SU(SUModelAddMaterials(model_, 1, &mat));
            materials_[m.name] = mat;
        }
    }

    void add_definitions() {
        for (const ComponentDefinition& d : scene_.definitions) {
            SUComponentDefinitionRef def = SU_INVALID;
            SU(SUComponentDefinitionCreate(&def));
            SU(SUComponentDefinitionSetName(def, d.name.c_str()));
            SU(SUComponentDefinitionSetDescription(def, d.description.c_str()));
            // The definition must belong to the model before its entities are touched.
            SU(SUModelAddComponentDefinitions(model_, 1, &def));
            SUEntitiesRef entities = SU_INVALID;
            SU(SUComponentDefinitionGetEntities(def, &entities));
            fill(entities, d.contents);
            definitions_[d.name] = def;
        }
    }

    void set_tag(SUDrawingElementRef element, const std::string& tag) {
        if (tag.empty()) return;
        SU(SUDrawingElementSetLayer(element, layers_.at(tag)));
    }

    void set_attributes(SUEntityRef entity, const std::map<std::string, std::string>& attributes) {
        if (attributes.empty()) return;
        SUAttributeDictionaryRef dict = SU_INVALID;
        SU(SUEntityGetAttributeDictionary(entity, "HousePlan", &dict));
        for (const auto& [key, value] : attributes) {
            TypedValue v;
            SU(SUTypedValueSetString(v.get(), value.c_str()));
            SU(SUAttributeDictionarySetValue(dict, key.c_str(), v.get()));
        }
    }

    void add_group(SUEntitiesRef parent, const Group& g) {
        SUGroupRef group = SU_INVALID;
        SU(SUGroupCreate(&group));
        SU(SUEntitiesAddGroup(parent, group));
        SU(SUGroupSetName(group, g.name.c_str()));
        set_tag(SUGroupToDrawingElement(group), g.tag);
        set_attributes(SUGroupToEntity(group), g.attributes);
        SUEntitiesRef entities = SU_INVALID;
        SU(SUGroupGetEntities(group, &entities));
        fill(entities, g);
    }

    void fill(SUEntitiesRef entities, const Group& g) {
        add_geometry(entities, g);
        for (const Group& child : g.groups) add_group(entities, child);
        for (const Instance& i : g.instances) add_instance(entities, i);
        for (const Dimension& d : g.dimensions) add_dimension(entities, d);
    }

    // Faces (with holes), loose edges and arcs in one SUEntitiesFill so shared vertices weld.
    void add_geometry(SUEntitiesRef entities, const Group& g) {
        if (g.faces.empty() && g.edges.empty() && g.arcs.empty()) return;
        SUGeometryInputRef input = SU_INVALID;
        SU(SUGeometryInputCreate(&input));
        size_t next = 0;
        auto vertex = [&](Vec3 p) {
            const SUPoint3D q = pt(p);
            SU(SUGeometryInputAddVertex(input, &q));
            return next++;
        };
        auto loop = [&](const std::vector<Vec3>& points) {
            SULoopInputRef l = SU_INVALID;
            SU(SULoopInputCreate(&l));
            for (Vec3 p : points) SU(SULoopInputAddVertexIndex(l, vertex(p)));
            return l;
        };

        for (const Face& f : g.faces) {
            SULoopInputRef outer = loop(f.loops[0]);
            size_t face = 0;
            SU(SUGeometryInputAddFace(input, &outer, &face));  // takes ownership of the loop
            for (size_t i = 1; i < f.loops.size(); ++i) {
                SULoopInputRef inner = loop(f.loops[i]);
                SU(SUGeometryInputFaceAddInnerLoop(input, face, &inner));
            }
            if (!f.material.empty()) {
                SUMaterialInput m{};
                m.num_uv_coords = 0;
                m.material = materials_.at(f.material);
                SU(SUGeometryInputFaceSetFrontMaterial(input, face, &m));
                SU(SUGeometryInputFaceSetBackMaterial(input, face, &m));
            }
        }
        for (const auto& line : g.edges)
            for (size_t i = 0; i + 1 < line.size(); ++i) {
                size_t a = vertex(line[i]), b = vertex(line[i + 1]), edge = 0;
                SU(SUGeometryInputAddEdge(input, a, b, &edge));
            }
        for (const Arc& a : g.arcs) {
            const Vec3 y = cross(a.normal, a.xaxis);
            auto at = [&](double t) { return a.center + a.xaxis * (a.radius * std::cos(t)) + y * (a.radius * std::sin(t)); };
            size_t s = vertex(at(a.startAngle)), e = vertex(at(a.endAngle)), curve = 0, control = 0;
            const SUPoint3D c = pt(a.center);
            const SUVector3D n = vec(a.normal);
            // VERIFY: the arc runs counter-clockwise about the normal from start to end.
            SU(SUGeometryInputAddArcCurve(input, s, e, &c, &n, static_cast<size_t>(a.segments), &curve, &control));
        }
        SU(SUEntitiesFill(entities, input, true));
        SU(SUGeometryInputRelease(&input));
    }

    void add_instance(SUEntitiesRef entities, const Instance& i) {
        SUComponentInstanceRef inst = SU_INVALID;
        SU(SUComponentDefinitionCreateInstance(definitions_.at(i.definition), &inst));
        const SUTransformation t = transformation(i.transform);
        SU(SUComponentInstanceSetTransform(inst, &t));
        SU(SUEntitiesAddInstance(entities, inst, nullptr));
        if (!i.name.empty()) SU(SUComponentInstanceSetName(inst, i.name.c_str()));
        set_tag(SUComponentInstanceToDrawingElement(inst), i.tag);
        set_attributes(SUComponentInstanceToEntity(inst), i.attributes);
    }

    void add_dimension(SUEntitiesRef entities, const Dimension& d) {
        const SUPoint3D s = pt(d.start), e = pt(d.end);
        // The dimension lies in the plane with this normal; its line is offset toward
        // normal x direction. VERIFY the sign against the rendered model.
        const Vec3 dir = normalized(d.end - d.start);
        Vec3 normal{0, 0, 1};
        if (std::abs(dot(dir, normal)) > 0.99) normal = normalized(cross(dir, d.offset));
        const Vec3 side = cross(normal, dir);
        const double offset = dot(d.offset, side) * kInchesPerMm;

        SUInstancePathRef none = SU_INVALID;
        SUDimensionLinearRef dim = SU_INVALID;
        SU(SUDimensionLinearCreate(&dim, &s, none, &e, none, offset));
        const SUVector3D n = vec(normal), x = vec(dir);
        SU(SUDimensionLinearSetNormal(dim, &n));
        SU(SUDimensionLinearSetXAxis(dim, &x));
        SUDimensionRef base = SUDimensionLinearToDimension(dim);
        SU(SUEntitiesAddDimensions(entities, 1, &base));
    }

    void set_location() {
        SULocationRef location = SU_INVALID;
        SU(SUModelGetLocation(model_, &location));
        if (scene_.location) {
            SU(SULocationSetLatLong(location, scene_.location->lat, scene_.location->lon));
            SU(SUModelSetGeoReference(model_, scene_.location->lat, scene_.location->lon, 0, false, false));
        }
        // VERIFY: SketchUp's north angle direction matches ours (clockwise from green).
        SU(SULocationSetNorthAngle(location, scene_.northAngleDeg));
    }

    void set_units() {
        SUOptionsManagerRef options = SU_INVALID;
        SU(SUModelGetOptionsManager(model_, &options));
        SUOptionsProviderRef units = SU_INVALID;
        SU(SUOptionsManagerGetOptionsProviderByName(options, "UnitsOptions", &units));
        const bool metric = scene_.units == DisplayUnits::Metric;
        auto set_int = [&](const char* key, int32_t value) {
            TypedValue v;
            SU(SUTypedValueSetInt32(v.get(), value));
            SU(SUOptionsProviderSetValue(units, key, v.get()));
        };
        set_int("LengthFormat", metric ? 0 : 1);   // Decimal : Architectural
        set_int("LengthUnit", metric ? 2 : 0);     // Millimeter : Inches
        set_int("LengthPrecision", metric ? 0 : 4);  // 1 mm : 1/16"
    }
};

}  // namespace

void write_skp(const Scene& scene, const std::string& path) { Writer(scene).write(path); }

}  // namespace s2s
