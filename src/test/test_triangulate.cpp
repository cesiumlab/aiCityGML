#include <iostream>
#include <iomanip>
#include <vector>
#include <list>
#include <cmath>
#include <cstdint>
#include <algorithm>

struct Vec3 { double x, y, z; };
struct Vec2 { double u, v; };

static Vec3 operator-(const Vec3& a, const Vec3& b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
static Vec3 operator+(const Vec3& a, const Vec3& b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
static Vec3 operator*(const Vec3& a, double s) { return {a.x * s, a.y * s, a.z * s}; }

static Vec3 cross(const Vec3& a, const Vec3& b) {
    return { a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x };
}
static double dot(const Vec3& a, const Vec3& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}
static double len(const Vec3& v) {
    return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
}
static Vec3 normalized(const Vec3& v) {
    double l = len(v);
    if (l < 1e-12) return {0, 0, 0};
    return {v.x / l, v.y / l, v.z / l};
}

// Build local coordinate system from first 3 non-collinear points:
//   origin = p0
//   x_axis = normalize(p1 - p0)
//   z_axis = normalize((p1-p0) x (p2-p0))  [polygon normal]
//   y_axis = z_axis x x_axis
// Project all points onto this plane, returning parallel 3D+2D arrays.
static void buildLocalFrame(const std::vector<Vec3>& pts,
                           Vec3& origin, Vec3& x_axis, Vec3& y_axis, Vec3& z_axis,
                           std::vector<Vec3>& out3d,
                           std::vector<Vec2>& out2d) {
    out3d.clear();
    out2d.clear();
    size_t n = pts.size();
    if (n < 3) return;

    // Skip closing duplicate point (first == last).
    size_t start = 0;
    if (n >= 2 &&
        std::abs(pts[0].x - pts[n-1].x) < 1e-8 &&
        std::abs(pts[0].y - pts[n-1].y) < 1e-8 &&
        std::abs(pts[0].z - pts[n-1].z) < 1e-8) {
        start = 1;
    }
    if (n - start < 3) return;

    // Build frame from first 3 unique points.
    origin = pts[start];
    Vec3 e1 = normalized(pts[start + 1] - origin);
    Vec3 e2 = pts[start + 2] - origin;

    z_axis = normalized(cross(e1, e2));
    // Degenerate: points are collinear.
    if (len(z_axis) < 1e-10) return;

    x_axis = e1;
    y_axis = cross(z_axis, x_axis);
    y_axis = normalized(y_axis);

    // Project all unique points onto the frame.
    for (size_t i = start; i < n; ++i) {
        Vec3 d = pts[i] - origin;
        double u = dot(d, x_axis);
        double v = dot(d, y_axis);
        out3d.push_back(pts[i]);
        out2d.push_back({u, v});
    }
}

static double triangleArea2D(double ax, double ay, double bx, double by, double cx, double cy) {
    return (bx - ax) * (cy - ay) - (by - ay) * (cx - ax);
}

static bool pointInTriangle2D(double ax, double ay, double bx, double by,
                              double cx, double cy, double px, double py) {
    double areaABC = triangleArea2D(ax, ay, bx, by, cx, cy);
    if (std::abs(areaABC) < 1e-12) return false;
    double areaPBC = triangleArea2D(px, py, bx, by, cx, cy);
    double areaAPC = triangleArea2D(ax, ay, px, py, cx, cy);
    double areaABP = triangleArea2D(ax, ay, bx, by, px, py);
    return (areaPBC > 0) && (areaAPC > 0) && (areaABP > 0);
}

// Ear-clipping triangulation for a simple polygon.
// Input: 2D points in any winding order.
// Output: N-2 triangles as flat index array.
static std::vector<uint32_t> earClipTriangulate(const std::vector<Vec2>& pts2d) {
    std::vector<uint32_t> result;
    size_t n = pts2d.size();
    if (n < 3) return result;

    // Detect winding order via shoelace formula.
    double totalArea = 0;
    for (size_t i = 0; i < n; ++i) {
        const Vec2& a = pts2d[i];
        const Vec2& b = pts2d[(i + 1) % n];
        totalArea += a.u * b.v - b.u * a.v;
    }
    bool isCCW = (totalArea > 0);

    std::list<uint32_t> remaining;
    for (uint32_t i = 0; i < static_cast<uint32_t>(n); ++i) remaining.push_back(i);

    while (remaining.size() > 3) {
        bool earFound = false;
        for (auto it = remaining.begin(); it != remaining.end(); ++it) {
            auto prevIt = (it == remaining.begin()) ? std::prev(remaining.end()) : std::prev(it);
            auto nextIt = std::next(it);
            if (nextIt == remaining.end()) nextIt = remaining.begin();

            uint32_t vi = *it, vp = *prevIt, vn = *nextIt;
            double area = triangleArea2D(
                pts2d[vp].u, pts2d[vp].v,
                pts2d[vi].u, pts2d[vi].v,
                pts2d[vn].u, pts2d[vn].v);

            bool isConvex = isCCW ? (area > 0) : (area < 0);
            if (!isConvex) continue;

            bool hasInterior = false;
            for (uint32_t vk : remaining) {
                if (vk == vp || vk == vi || vk == vn) continue;
                if (vk >= static_cast<uint32_t>(n)) continue;
                if (pointInTriangle2D(
                        pts2d[vp].u, pts2d[vp].v,
                        pts2d[vi].u, pts2d[vi].v,
                        pts2d[vn].u, pts2d[vn].v,
                        pts2d[vk].u, pts2d[vk].v)) { hasInterior = true; break; }
            }
            if (hasInterior) continue;

            result.push_back(vp); result.push_back(vi); result.push_back(vn);
            auto eraseIt = it;
            if (it == remaining.begin()) it = std::prev(remaining.end());
            else --it;
            remaining.erase(eraseIt);
            earFound = true;
            break;
        }
        if (!earFound) break;
    }

    if (remaining.size() == 3) {
        for (uint32_t idx : remaining) result.push_back(idx);
    }
    return result;
}

// Triangulate a LinearRing.
// Outputs triangles using ear-clipping in the local frame.
static bool triangulateRing(const std::vector<Vec3>& pts,
                           std::vector<Vec3>& outVerts,
                           std::vector<uint32_t>& outIndices,
                           Vec3& outNormal) {
    if (pts.size() < 3) return false;

    Vec3 origin, x_axis, y_axis, z_axis;
    std::vector<Vec3> clean3d;
    std::vector<Vec2> clean2d;
    buildLocalFrame(pts, origin, x_axis, y_axis, z_axis, clean3d, clean2d);

    if (clean3d.size() < 3) return false;

    std::vector<uint32_t> indices = earClipTriangulate(clean2d);
    if (indices.empty()) return false;

    outNormal = z_axis;
    uint32_t base = static_cast<uint32_t>(outVerts.size());
    for (const Vec3& p : clean3d) outVerts.push_back(p);
    for (uint32_t idx : indices) outIndices.push_back(base + idx);
    return true;
}

int main() {
    std::cerr << std::setprecision(6);

    // 5 test polygons from 2.gml
    std::vector<std::vector<Vec3>> tests = {
        // Polygon 2: wall (立面), z changes 3.803 -> 10.736
        {{6712.249,2408.467,3.803},{6703.384,2382.407,3.803},{6703.384,2382.407,10.736},{6712.249,2408.467,10.736},{6712.249,2408.467,3.803}},
        // Polygon 5: roof (屋顶), z=10.736, concave quad
        {{6700.398,2412.48,10.736},{6712.249,2408.467,10.736},{6711.493,2408.094,10.736},{6700.771,2411.726,10.736},{6700.398,2412.48,10.736}},
        // Polygon 7: roof 2, z=10.736
        {{6703.384,2382.407,10.736},{6691.544,2386.432,10.736},{6692.298,2386.804,10.736},{6703.013,2383.163,10.736},{6703.384,2382.407,10.736}},
        // Polygon 9: wall, z from 10.736 to 10.342
        {{6711.493,2408.094,10.736},{6703.013,2383.163,10.736},{6703.013,2383.163,10.342},{6711.493,2408.094,10.342},{6711.493,2408.094,10.736}},
        // Polygon 14: horizontal floor, z=8.646
        {{6712.249,2408.467,8.646},{6713.727,2407.988,8.646},{6704.809,2381.923,8.646},{6703.384,2382.407,8.646},{6712.249,2408.467,8.646}},
    };

    const char* names[] = {"Polygon2(wall)", "Polygon5(roof)", "Polygon7(roof)", "Polygon9(wall)", "Polygon14(floor)"};

    int pass = 0, fail = 0;
    for (size_t t = 0; t < tests.size(); ++t) {
        const auto& pts = tests[t];
        std::cerr << "\n=== " << names[t] << " ===\n";
        for (size_t i = 0; i < pts.size(); ++i)
            std::cerr << "  [" << i << "] (" << pts[i].x << "," << pts[i].y << "," << pts[i].z << ")\n";

        std::vector<Vec3> verts;
        std::vector<uint32_t> indices;
        Vec3 normal;
        bool ok = triangulateRing(pts, verts, indices, normal);

        if (!ok) {
            std::cerr << "  RESULT: FAILED\n";
            ++fail;
        } else {
            std::cerr << "  RESULT: OK - " << indices.size()/3 << " triangles, " << verts.size() << " vertices\n";
            std::cerr << "  Normal: (" << normal.x << "," << normal.y << "," << normal.z << ")\n";
            for (size_t i = 0; i < indices.size(); i += 3) {
                std::cerr << "  T" << (i/3) << ": "
                          << indices[i] << " " << indices[i+1] << " " << indices[i+2] << "\n";
            }
            ++pass;
        }
    }

    std::cerr << "\n========================================\n";
    std::cerr << "Summary: " << pass << " passed, " << fail << " failed\n";
    return fail > 0 ? 1 : 0;
}
