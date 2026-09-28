#include <cmath>
#include <functional>

#include <nlohmann/json.hpp>

#include "exporters.hpp"
#include "flatten.hpp"
#include "units.hpp"

namespace s2s {

namespace {

using nlohmann::json;

json mm(Vec3 p) { return json::array({std::lround(p.x), std::lround(p.y), std::lround(p.z)}); }

const char* kPage = R"HTML(<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>__TITLE__</title>
<style>
  :root { --bg:#eef0f2; --panel:#ffffff; --text:#1d2329; --muted:#5c6670; --line:#d6dbe0; --accent:#1f5fa8; }
  * { box-sizing: border-box; }
  html, body { margin: 0; height: 100%; background: var(--bg); color: var(--text);
    font: 14px/1.4 "Segoe UI", system-ui, -apple-system, Helvetica, Arial, sans-serif; }
  #view { position: fixed; inset: 0; }
  #panel { position: fixed; top: 16px; left: 16px; width: 260px; max-height: calc(100% - 32px); overflow: auto;
    background: var(--panel); border: 1px solid var(--line); border-radius: 10px; padding: 14px 16px;
    box-shadow: 0 4px 18px rgba(0,0,0,.08); }
  h1 { font-size: 15px; margin: 0 0 2px; }
  .sub { color: var(--muted); font-size: 12px; margin-bottom: 12px; }
  h2 { font-size: 12px; text-transform: uppercase; letter-spacing: .06em; color: var(--muted); margin: 14px 0 6px; }
  label { display: flex; align-items: center; gap: 8px; padding: 3px 0; cursor: pointer; }
  .sw { width: 12px; height: 12px; border-radius: 3px; border: 1px solid rgba(0,0,0,.2); }
  ul { margin: 0; padding-left: 18px; color: var(--muted); font-size: 12px; }
  .hint { color: var(--muted); font-size: 12px; margin-top: 12px; }
  button { font: inherit; font-size: 12px; padding: 4px 10px; border-radius: 6px; border: 1px solid var(--line);
    background: #f7f8f9; cursor: pointer; margin: 2px 4px 0 0; }
  @media (max-width: 600px) { #panel { width: auto; right: 16px; max-height: 40%; } }
</style>
<script type="importmap">
{ "imports": {
  "three": "https://cdn.jsdelivr.net/npm/three@0.170.0/build/three.module.js",
  "three/addons/": "https://cdn.jsdelivr.net/npm/three@0.170.0/examples/jsm/"
} }
</script>
</head>
<body>
<div id="view"></div>
<div id="panel">
  <h1>__TITLE__</h1>
  <div class="sub">Generated preview of the SketchUp model</div>
  <h2>Tags</h2>
  <div id="tags"></div>
  <h2>View</h2>
  <button data-view="iso">3D</button><button data-view="top">Top</button><button data-view="front">Front</button>
  <div id="warn"></div>
  <div class="hint">Drag to orbit · right-drag to pan · scroll to zoom</div>
</div>
<script id="scene-data" type="application/json">__DATA__</script>
<script type="module">
import * as THREE from 'three';
import { OrbitControls } from 'three/addons/controls/OrbitControls.js';

const data = JSON.parse(document.getElementById('scene-data').textContent);
const M = 0.001; // mm -> m

const renderer = new THREE.WebGLRenderer({ antialias: true });
renderer.setPixelRatio(window.devicePixelRatio);
document.getElementById('view').appendChild(renderer.domElement);
const scene = new THREE.Scene();
scene.background = new THREE.Color(0xeef0f2);
const camera = new THREE.PerspectiveCamera(40, 1, 0.05, 500);
camera.up.set(0, 0, 1);
const controls = new OrbitControls(camera, renderer.domElement);
scene.add(new THREE.HemisphereLight(0xffffff, 0x8a8f96, 2.2));
const sun = new THREE.DirectionalLight(0xffffff, 1.6);
sun.position.set(-6, -10, 14);
scene.add(sun);

function colorFor(p) {
  if (p.material === 'Glass') return { color: 0x9fc3e0, opacity: 0.35 };
  if (/\/Leaf( Left| Right)?$/.test(p.path)) return { color: 0xb08a5a };
  if (/\/Frame$/.test(p.path)) return { color: 0xfaf8f4 };
  switch (p.kind) {
    case 'Floors': return { color: 0xcaa982 };
    case 'Ceilings': return { color: 0xfbfbfb, opacity: 0.85 };
    default: return { color: 0xece8e1 };
  }
}

const byTag = new Map();
const addToTag = (tag, obj) => { if (!byTag.has(tag)) byTag.set(tag, []); byTag.get(tag).push(obj); };
const box = new THREE.Box3();

for (const p of data.parts) {
  if (p.tris.length) {
    const g = new THREE.BufferGeometry();
    g.setAttribute('position', new THREE.Float32BufferAttribute(p.tris.map(v => v * M), 3));
    g.computeVertexNormals();
    const c = colorFor(p);
    const mat = new THREE.MeshLambertMaterial({ color: c.color, side: THREE.DoubleSide,
      transparent: c.opacity !== undefined, opacity: c.opacity ?? 1, depthWrite: c.opacity === undefined,
      polygonOffset: true, polygonOffsetFactor: 1, polygonOffsetUnits: 1 });
    const mesh = new THREE.Mesh(g, mat);
    const edges = new THREE.LineSegments(new THREE.EdgesGeometry(g, 25),
      new THREE.LineBasicMaterial({ color: 0x3a3f45, transparent: true, opacity: 0.55 }));
    const obj = new THREE.Group(); obj.add(mesh, edges);
    scene.add(obj); addToTag(p.tag, obj);
    if (p.kind !== 'Ceilings') box.expandByObject(obj);
  }
  for (const l of p.lines) {
    const g = new THREE.BufferGeometry().setFromPoints(l.map(v => new THREE.Vector3(v[0]*M, v[1]*M, v[2]*M + 0.002)));
    const line = new THREE.Line(g, new THREE.LineBasicMaterial({ color: 0x444a50 }));
    scene.add(line); addToTag(p.tag, line);
  }
}

function label(text) {
  const c = document.createElement('canvas'); const ctx = c.getContext('2d');
  const fs = 44; ctx.font = `600 ${fs}px Segoe UI, Helvetica, Arial, sans-serif`;
  c.width = Math.ceil(ctx.measureText(text).width) + 16; c.height = fs + 12;
  ctx.font = `600 ${fs}px Segoe UI, Helvetica, Arial, sans-serif`;
  ctx.fillStyle = 'rgba(255,255,255,.85)'; ctx.fillRect(0, 0, c.width, c.height);
  ctx.fillStyle = '#1f5fa8'; ctx.textBaseline = 'middle'; ctx.fillText(text, 8, c.height / 2);
  const s = new THREE.Sprite(new THREE.SpriteMaterial({ map: new THREE.CanvasTexture(c), depthTest: false }));
  const h = 0.16; s.scale.set(h * c.width / c.height, h, 1); s.renderOrder = 10;
  return s;
}
const dimMat = new THREE.LineBasicMaterial({ color: 0x1f5fa8 });
for (const d of data.dimensions) {
  const v = a => new THREE.Vector3(a[0]*M, a[1]*M, a[2]*M + 0.003);
  const s = v(d.start), e = v(d.end), o = v(d.offset).setZ(0);
  const so = s.clone().add(o), eo = e.clone().add(o);
  const g = new THREE.BufferGeometry().setFromPoints([s, so, e, eo, so, eo]);
  const obj = new THREE.Group();
  obj.add(new THREE.LineSegments(g, dimMat));
  const t = label(d.text); t.position.copy(so.clone().add(eo).multiplyScalar(0.5)); obj.add(t);
  scene.add(obj); addToTag(data.dimensionTag, obj);
}

const tagsEl = document.getElementById('tags');
const swatch = { Walls: '#ece8e1', Floors: '#caa982', Ceilings: '#fbfbfb', Doors: '#b08a5a', Windows: '#9fc3e0',
  'Door Swings': '#444a50', Dimensions: '#1f5fa8' };
for (const t of data.tags) {
  const objs = byTag.get(t.name) || [];
  objs.forEach(o => o.visible = t.visible);
  const lab = document.createElement('label');
  lab.innerHTML = `<input type="checkbox" ${t.visible ? 'checked' : ''}><span class="sw" style="background:${swatch[t.name] || '#ccc'}"></span>${t.name}${objs.length ? '' : ' <span style="color:#999">(empty)</span>'}`;
  lab.querySelector('input').addEventListener('change', e => objs.forEach(o => o.visible = e.target.checked));
  tagsEl.appendChild(lab);
}
if (data.warnings.length) {
  document.getElementById('warn').innerHTML = '<h2>Warnings</h2><ul>' +
    data.warnings.map(w => `<li>${w.replace(/[&<>]/g, c => ({'&':'&amp;','<':'&lt;','>':'&gt;'}[c]))}</li>`).join('') + '</ul>';
}

const centre = box.getCenter(new THREE.Vector3()), size = box.getSize(new THREE.Vector3());
const radius = Math.max(size.x, size.y, size.z);
function setView(kind) {
  controls.target.copy(centre);
  const d = radius * 1.6;
  if (kind === 'top') camera.position.set(centre.x, centre.y - 0.001, centre.z + d * 1.2);
  else if (kind === 'front') camera.position.set(centre.x, centre.y - d * 1.3, centre.z + size.z * 0.3);
  else camera.position.set(centre.x - d * 0.6, centre.y - d * 1.0, centre.z + d * 0.8);
  controls.update();
}
document.querySelectorAll('[data-view]').forEach(b => b.addEventListener('click', () => setView(b.dataset.view)));
setView('iso');

function resize() {
  const w = window.innerWidth, h = window.innerHeight;
  renderer.setSize(w, h); camera.aspect = w / h; camera.updateProjectionMatrix();
}
window.addEventListener('resize', resize); resize();
renderer.setAnimationLoop(() => { controls.update(); renderer.render(scene, camera); });
</script>
</body>
</html>
)HTML";

void replace_all(std::string& s, const std::string& from, const std::string& to) {
    for (size_t pos = s.find(from); pos != std::string::npos; pos = s.find(from, pos + to.size())) s.replace(pos, from.size(), to);
}

}  // namespace

std::string viewer_html(const Scene& scene, const std::string& title) {
    json data;
    data["parts"] = json::array();
    for (const FlatPart& p : flatten(scene, true)) {
        json tris = json::array();
        for (const Triangle& t : p.triangles)
            for (Vec3 v : t) {
                tris.push_back(std::lround(v.x));
                tris.push_back(std::lround(v.y));
                tris.push_back(std::lround(v.z));
            }
        json lines = json::array();
        for (const auto& l : p.lines) {
            json pts = json::array();
            for (Vec3 v : l) pts.push_back(mm(v));
            lines.push_back(pts);
        }
        data["parts"].push_back({{"path", p.path}, {"tag", p.tag}, {"kind", p.kind}, {"material", p.material},
                                 {"tris", tris}, {"lines", lines}});
    }

    data["dimensions"] = json::array();
    std::function<void(const Group&)> dims = [&](const Group& g) {
        for (const Dimension& d : g.dimensions)
            data["dimensions"].push_back({{"start", mm(d.start)}, {"end", mm(d.end)}, {"offset", mm(d.offset)},
                                          {"text", format_length(distance(d.start, d.end), scene.units)}});
        for (const Group& c : g.groups) dims(c);
    };
    for (const Group& g : scene.groups) dims(g);
    data["dimensionTag"] = "Dimensions";

    data["tags"] = json::array();
    for (const Tag& t : scene.tags) data["tags"].push_back({{"name", t.name}, {"visible", t.visible}});
    data["warnings"] = scene.warnings;

    std::string page = kPage;
    std::string safeTitle;
    for (char c : title) safeTitle += (c == '<' || c == '>' || c == '&') ? '_' : c;
    replace_all(page, "__TITLE__", safeTitle);
    std::string payload = data.dump();
    replace_all(payload, "</", "<\\/");  // keep the JSON from closing the script tag
    replace_all(page, "__DATA__", payload);
    return page;
}

}  // namespace s2s
