// Mondo: zone, collisioni, raycast, disegno della geometria statica.
#include "game.h"

World W;

static const float CHUNK = 64.0f;

// ---------------------------------------------------------------------------
//  Zone
// ---------------------------------------------------------------------------
MeshBuilder& Zone::B(Vector3 p) {
    long long cx = (long long)floorf(p.x / CHUNK), cz = (long long)floorf(p.z / CHUNK);
    long long key = (cx << 32) ^ (cz & 0xffffffffll);
    return builders[key];
}

void Zone::AddBox(Vector3 center, Vector3 size, Color col, bool collide, float yaw, bool metal) {
    B(center).BoxAt(center, size, col, yaw);
    if (!collide) return;
    if (fabsf(yaw) < 0.001f || fabsf(fabsf(yaw) - PI) < 0.001f) {
        AddCollider(center - size * 0.5f, center + size * 0.5f, metal);
    } else if (fabsf(fabsf(yaw) - PI / 2) < 0.001f) {
        Vector3 s = V3(size.z, size.y, size.x);
        AddCollider(center - s * 0.5f, center + s * 0.5f, metal);
    } else {
        // box ruotato: approssimato con cilindri lungo l'asse maggiore
        Vector3 ax = size.x > size.z ? V3(cosf(yaw), 0, -sinf(yaw)) : V3(sinf(yaw), 0, cosf(yaw));
        float len = std::max(size.x, size.z), r = std::min(size.x, size.z) * 0.5f;
        int n = std::max(1, (int)ceilf(len / std::max(r * 2, 0.6f)));
        for (int i = 0; i < n; i++) {
            float t = (n == 1) ? 0 : ((float)i / (n - 1) - 0.5f) * (len - r * 2);
            Vector3 p = center + ax * t;
            AddCyl(p.x, p.z, std::max(r, 0.3f), center.y - size.y * 0.5f, center.y + size.y * 0.5f, metal);
        }
    }
}

int Zone::AddCollider(Vector3 mn, Vector3 mx, bool metal) {
    Box b;
    b.mn = mn;
    b.mx = mx;
    b.metal = metal;
    boxes.push_back(b);
    return (int)boxes.size() - 1;
}

void Zone::AddCyl(float x, float z, float r, float y0, float y1, bool metal) {
    Cyl c{x, z, r, y0, y1, metal};
    cyls.push_back(c);
}

void Zone::Finalize() {
    for (auto& kv : builders) {
        MeshBuilder& b = kv.second;
        if (b.VertexCount() == 0) continue;
        // suddivide se troppo grande
        StaticChunk ch;
        Mesh m = b.Build();
        ch.model = LoadModelFromMesh(m);
        ch.model.materials[0].shader = R.lit;
        ch.model.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = R.detail;
        ch.bb = GetMeshBoundingBox(m);
        chunks.push_back(ch);
    }
    builders.clear();

    gridMinX = bmin.x;
    gridMinZ = bmin.z;
    gridW = (int)ceilf((bmax.x - bmin.x) / gridCell) + 1;
    gridH = (int)ceilf((bmax.z - bmin.z) / gridCell) + 1;
    gridBoxes.assign(gridW * gridH, {});
    gridCyls.assign(gridW * gridH, {});
    auto cellRange = [&](float x0, float z0, float x1, float z1, int& a, int& b, int& c, int& d) {
        a = Clamp((int)floorf((x0 - gridMinX) / gridCell), 0, gridW - 1);
        b = Clamp((int)floorf((z0 - gridMinZ) / gridCell), 0, gridH - 1);
        c = Clamp((int)floorf((x1 - gridMinX) / gridCell), 0, gridW - 1);
        d = Clamp((int)floorf((z1 - gridMinZ) / gridCell), 0, gridH - 1);
    };
    for (int i = 0; i < (int)boxes.size(); i++) {
        int a, b, c, d;
        cellRange(boxes[i].mn.x, boxes[i].mn.z, boxes[i].mx.x, boxes[i].mx.z, a, b, c, d);
        for (int z = b; z <= d; z++)
            for (int x = a; x <= c; x++) gridBoxes[z * gridW + x].push_back(i);
    }
    for (int i = 0; i < (int)cyls.size(); i++) {
        int a, b, c, d;
        cellRange(cyls[i].x - cyls[i].r, cyls[i].z - cyls[i].r, cyls[i].x + cyls[i].r, cyls[i].z + cyls[i].r, a, b, c, d);
        for (int z = b; z <= d; z++)
            for (int x = a; x <= c; x++) gridCyls[z * gridW + x].push_back(i);
    }
}

// ---------------------------------------------------------------------------
//  Terreno
// ---------------------------------------------------------------------------
float Terrain::Height(float x, float z) const {
    if (h.empty()) return 0;
    float gx = (x + WORLD_HALF) / TER_CELL, gz = (z + WORLD_HALF) / TER_CELL;
    gx = Clamp(gx, 0.0f, TER_N - 0.001f);
    gz = Clamp(gz, 0.0f, TER_N - 0.001f);
    int i = (int)gx, j = (int)gz;
    float fx = gx - i, fz = gz - j;
    float h00 = H(i, j), h10 = H(i + 1, j), h01 = H(i, j + 1), h11 = H(i + 1, j + 1);
    if (fx + fz <= 1.0f) return h00 + (h10 - h00) * fx + (h01 - h00) * fz;
    return h11 + (h01 - h11) * (1 - fx) + (h10 - h11) * (1 - fz);
}

Vector3 Terrain::Normal(float x, float z) const {
    float e = TER_CELL * 0.5f;
    float hl = Height(x - e, z), hr = Height(x + e, z), hd = Height(x, z - e), hu = Height(x, z + e);
    return Vector3Normalize(V3(hl - hr, 2 * e, hd - hu));
}

float TerrainHeight(float x, float z) { return W.terrain.Height(x, z); }

// ---------------------------------------------------------------------------
//  Collisioni
// ---------------------------------------------------------------------------
static inline int CellIndex(const Zone& z, float x, float zz) {
    int cx = (int)floorf((x - z.gridMinX) / z.gridCell), cz = (int)floorf((zz - z.gridMinZ) / z.gridCell);
    if (cx < 0 || cz < 0 || cx >= z.gridW || cz >= z.gridH) return -1;
    return cz * z.gridW + cx;
}

float GroundHeight(int zone, Vector3 p, float stepUp) {
    Zone& z = W.zones[zone];
    float g = z.outdoor ? W.terrain.Height(p.x, p.z) : z.floorY;
    int ci = CellIndex(z, p.x, p.z);
    if (ci < 0) return g;
    for (int bi : z.gridBoxes[ci]) {
        const Box& b = z.boxes[bi];
        if (!b.enabled) continue;
        if (p.x < b.mn.x || p.x > b.mx.x || p.z < b.mn.z || p.z > b.mx.z) continue;
        if (b.mx.y <= p.y + stepUp && b.mx.y > g) g = b.mx.y;
    }
    for (int ci2 : z.gridCyls[ci]) {
        const Cyl& c = z.cyls[ci2];
        float dx = p.x - c.x, dz = p.z - c.z;
        if (dx * dx + dz * dz > c.r * c.r) continue;
        if (c.y1 <= p.y + stepUp && c.y1 > g) g = c.y1;
    }
    return g;
}

static bool PushOut(int zone, Vector3& pos, float radius, float height, float stepUp, bool apply) {
    Zone& z = W.zones[zone];
    bool any = false;
    int x0 = Clamp((int)floorf((pos.x - radius - z.gridMinX) / z.gridCell), 0, z.gridW - 1);
    int x1 = Clamp((int)floorf((pos.x + radius - z.gridMinX) / z.gridCell), 0, z.gridW - 1);
    int z0 = Clamp((int)floorf((pos.z - radius - z.gridMinZ) / z.gridCell), 0, z.gridH - 1);
    int z1 = Clamp((int)floorf((pos.z + radius - z.gridMinZ) / z.gridCell), 0, z.gridH - 1);
    for (int cz = z0; cz <= z1; cz++)
        for (int cx = x0; cx <= x1; cx++) {
            int ci = cz * z.gridW + cx;
            for (int bi : z.gridBoxes[ci]) {
                const Box& b = z.boxes[bi];
                if (!b.enabled) continue;
                if (b.mx.y <= pos.y + stepUp || b.mn.y >= pos.y + height) continue;
                float qx = Clamp(pos.x, b.mn.x, b.mx.x), qz = Clamp(pos.z, b.mn.z, b.mx.z);
                float dx = pos.x - qx, dz = pos.z - qz;
                float d2 = dx * dx + dz * dz;
                if (d2 >= radius * radius) continue;
                any = true;
                if (!apply) return true;
                if (d2 > 1e-8f) {
                    float d = sqrtf(d2);
                    pos.x = qx + dx / d * radius;
                    pos.z = qz + dz / d * radius;
                } else {
                    // centro dentro il box: esce dal lato più vicino
                    float l = pos.x - b.mn.x, r = b.mx.x - pos.x, f = pos.z - b.mn.z, k = b.mx.z - pos.z;
                    float m = std::min(std::min(l, r), std::min(f, k));
                    if (m == l) pos.x = b.mn.x - radius;
                    else if (m == r) pos.x = b.mx.x + radius;
                    else if (m == f) pos.z = b.mn.z - radius;
                    else pos.z = b.mx.z + radius;
                }
            }
            for (int ci2 : z.gridCyls[ci]) {
                const Cyl& c = z.cyls[ci2];
                if (c.y1 <= pos.y + stepUp || c.y0 >= pos.y + height) continue;
                float dx = pos.x - c.x, dz = pos.z - c.z;
                float d2 = dx * dx + dz * dz, rr = radius + c.r;
                if (d2 >= rr * rr) continue;
                any = true;
                if (!apply) return true;
                float d = sqrtf(std::max(d2, 1e-8f));
                if (d < 1e-4f) { dx = 1; dz = 0; d = 1; }
                pos.x = c.x + dx / d * rr;
                pos.z = c.z + dz / d * rr;
            }
        }
    return any;
}

void ResolveCollision(int zone, Vector3& pos, float radius, float height, float stepUp) {
    for (int i = 0; i < 3; i++)
        if (!PushOut(zone, pos, radius, height, stepUp, true)) break;
    Zone& z = W.zones[zone];
    if (z.outdoor) {
        float lim = WORLD_HALF - 24;
        pos.x = Clamp(pos.x, -lim, lim);
        pos.z = Clamp(pos.z, -lim, lim);
    } else {
        pos.x = Clamp(pos.x, z.bmin.x + 0.5f, z.bmax.x - 0.5f);
        pos.z = Clamp(pos.z, z.bmin.z + 0.5f, z.bmax.z - 0.5f);
    }
}

bool CircleBlocked(int zone, Vector3 pos, float radius, float height, float stepUp) {
    Vector3 p = pos;
    return PushOut(zone, p, radius, height, stepUp, false);
}

// ---------------------------------------------------------------------------
//  Raycast
// ---------------------------------------------------------------------------
static std::vector<int> sBoxStamp, sCylStamp;
static int sStamp = 0;

static bool RayBox(Vector3 o, Vector3 d, const Box& b, float& t, Vector3& n) {
    float tmin = -1e9f, tmax = 1e9f;
    Vector3 nn{};
    const float* oo = &o.x;
    const float* dd = &d.x;
    const float* mn = &b.mn.x;
    const float* mx = &b.mx.x;
    for (int a = 0; a < 3; a++) {
        if (fabsf(dd[a]) < 1e-8f) {
            if (oo[a] < mn[a] || oo[a] > mx[a]) return false;
            continue;
        }
        float inv = 1.0f / dd[a];
        float t1 = (mn[a] - oo[a]) * inv, t2 = (mx[a] - oo[a]) * inv;
        float sgn = -1;
        if (t1 > t2) { std::swap(t1, t2); sgn = 1; }
        if (t1 > tmin) {
            tmin = t1;
            nn = V3(0, 0, 0);
            (&nn.x)[a] = sgn;
        }
        tmax = std::min(tmax, t2);
        if (tmin > tmax) return false;
    }
    if (tmax < 0) return false;
    if (tmin < 0) return false; // origine dentro il box: ignorato
    t = tmin;
    n = nn;
    return true;
}

static bool RayCyl(Vector3 o, Vector3 d, const Cyl& c, float& t, Vector3& n) {
    float ox = o.x - c.x, oz = o.z - c.z;
    float a = d.x * d.x + d.z * d.z;
    if (a < 1e-8f) return false;
    float b = 2 * (ox * d.x + oz * d.z);
    float cc = ox * ox + oz * oz - c.r * c.r;
    float disc = b * b - 4 * a * cc;
    if (disc < 0) return false;
    float s = sqrtf(disc);
    float t0 = (-b - s) / (2 * a);
    if (t0 < 0) return false;
    float y = o.y + d.y * t0;
    if (y < c.y0 || y > c.y1) return false;
    t = t0;
    n = Vector3Normalize(V3(ox + d.x * t0, 0, oz + d.z * t0));
    return true;
}

static bool RayTerrain(Vector3 o, Vector3 d, float maxT, float& tOut) {
    float step = 1.0f;
    float prevT = 0;
    if (o.y < W.terrain.Height(o.x, o.z) - 0.05f) return false;
    for (float t = step; t <= maxT + step; t += step) {
        float tt = std::min(t, maxT);
        Vector3 p = o + d * tt;
        if (fabsf(p.x) > WORLD_HALF || fabsf(p.z) > WORLD_HALF) return false;
        if (p.y < W.terrain.Height(p.x, p.z)) {
            float lo = prevT, hi = tt;
            for (int i = 0; i < 10; i++) {
                float mid = (lo + hi) * 0.5f;
                Vector3 q = o + d * mid;
                if (q.y < W.terrain.Height(q.x, q.z)) hi = mid;
                else lo = mid;
            }
            tOut = hi;
            return true;
        }
        prevT = tt;
        if (tt >= maxT) break;
        // passo adattivo: più lungo se lontani dal suolo
        float clear = p.y - W.terrain.Height(p.x, p.z);
        step = Clamp(clear * 0.5f, 0.5f, 4.0f);
    }
    return false;
}

bool RaycastWorld(int zone, Vector3 o, Vector3 dir, float maxT, RayHit& hit) {
    Zone& z = W.zones[zone];
    float best = maxT;
    bool found = false;
    Vector3 bestN{};
    bool bestMetal = false, bestTerrain = false;

    if (z.outdoor) {
        float t;
        if (RayTerrain(o, dir, maxT, t) && t < best) {
            best = t;
            found = true;
            bestN = W.terrain.Normal(o.x + dir.x * t, o.z + dir.z * t);
            bestTerrain = true;
        }
    } else {
        // pavimento
        if (dir.y < -1e-5f) {
            float t = (z.floorY - o.y) / dir.y;
            if (t > 0 && t < best) { best = t; found = true; bestN = V3(0, 1, 0); }
        }
    }

    if ((int)sBoxStamp.size() < (int)z.boxes.size()) sBoxStamp.resize(z.boxes.size(), 0);
    if ((int)sCylStamp.size() < (int)z.cyls.size()) sCylStamp.resize(z.cyls.size(), 0);
    sStamp++;
    if (sStamp > 2000000000) { sStamp = 1; std::fill(sBoxStamp.begin(), sBoxStamp.end(), 0); std::fill(sCylStamp.begin(), sCylStamp.end(), 0); }

    // attraversamento DDA della griglia in XZ
    float gx = (o.x - z.gridMinX) / z.gridCell, gz = (o.z - z.gridMinZ) / z.gridCell;
    int cx = (int)floorf(gx), cz = (int)floorf(gz);
    int stepX = dir.x > 0 ? 1 : -1, stepZ = dir.z > 0 ? 1 : -1;
    float tDeltaX = fabsf(dir.x) > 1e-6f ? z.gridCell / fabsf(dir.x) : 1e9f;
    float tDeltaZ = fabsf(dir.z) > 1e-6f ? z.gridCell / fabsf(dir.z) : 1e9f;
    float tMaxX = fabsf(dir.x) > 1e-6f ? ((dir.x > 0 ? (cx + 1 - gx) : (gx - cx)) * z.gridCell) / fabsf(dir.x) : 1e9f;
    float tMaxZ = fabsf(dir.z) > 1e-6f ? ((dir.z > 0 ? (cz + 1 - gz) : (gz - cz)) * z.gridCell) / fabsf(dir.z) : 1e9f;
    float tCell = 0;
    for (int iter = 0; iter < 400; iter++) {
        if (cx >= 0 && cz >= 0 && cx < z.gridW && cz < z.gridH) {
            int ci = cz * z.gridW + cx;
            for (int bi : z.gridBoxes[ci]) {
                if (sBoxStamp[bi] == sStamp) continue;
                sBoxStamp[bi] = sStamp;
                const Box& b = z.boxes[bi];
                if (!b.enabled) continue;
                float t;
                Vector3 n;
                if (RayBox(o, dir, b, t, n) && t < best) { best = t; bestN = n; found = true; bestMetal = b.metal; bestTerrain = false; }
            }
            for (int ci2 : z.gridCyls[ci]) {
                if (sCylStamp[ci2] == sStamp) continue;
                sCylStamp[ci2] = sStamp;
                float t;
                Vector3 n;
                if (RayCyl(o, dir, z.cyls[ci2], t, n) && t < best) { best = t; bestN = n; found = true; bestMetal = z.cyls[ci2].metal; bestTerrain = false; }
            }
        }
        float tNext = std::min(tMaxX, tMaxZ);
        if (tCell > best || tCell > maxT) break;
        if (tMaxX < tMaxZ) { cx += stepX; tCell = tMaxX; tMaxX += tDeltaX; }
        else { cz += stepZ; tCell = tMaxZ; tMaxZ += tDeltaZ; }
        (void)tNext;
        if (tCell > best || tCell > maxT) break;
    }

    if (!found) return false;
    hit.hit = true;
    hit.t = best;
    hit.point = o + dir * best;
    hit.normal = bestN;
    hit.metal = bestMetal;
    hit.terrain = bestTerrain;
    hit.entity = -1;
    return true;
}

bool LineOfSight(int zone, Vector3 a, Vector3 b) {
    Vector3 d = b - a;
    float len = Vector3Length(d);
    if (len < 0.01f) return true;
    RayHit h;
    return !RaycastWorld(zone, a, d / len, len - 0.2f, h);
}

float RadiationAt(int zone, Vector3 p) {
    float r = 0;
    for (auto& s : W.zones[zone].rads) {
        float d = Vector3Distance(p, s.pos);
        if (d < s.radius) r += s.strength * (1.0f - d / s.radius);
    }
    for (auto& e : G.ents)
        if (!e.dead && e.zone == zone && e.radAura > 0) {
            float d = Vector3Distance(p, e.pos);
            if (d < 10) r += e.radAura * (1.0f - d / 10);
        }
    return r;
}

// ---------------------------------------------------------------------------
//  Disegno
// ---------------------------------------------------------------------------
void DrawZoneStatic(int zone, Camera3D cam, float viewDist) {
    Zone& z = W.zones[zone];
    Vector3 fwd = Vector3Normalize(cam.target - cam.position);
    float e = 0;
    SetShaderValue(R.lit, R.locEmissive, &e, SHADER_UNIFORM_FLOAT);
    R.curEmissive = 0;
    for (auto& ch : z.chunks) {
        Vector3 c = (ch.bb.min + ch.bb.max) * 0.5f;
        float rad = Vector3Length(ch.bb.max - ch.bb.min) * 0.5f;
        Vector3 tc = c - cam.position;
        float d = Vector3Length(tc);
        if (d - rad > viewDist) continue;
        if (Vector3DotProduct(tc, fwd) < -rad) continue;
        // test sul cono visivo
        if (d > rad) {
            float cosA = Vector3DotProduct(tc / d, fwd);
            float ang = acosf(Clamp(cosA, -1.f, 1.f)) - asinf(Clamp(rad / d, 0.f, 1.f));
            if (ang > 1.25f) continue;
        }
        DrawModel(ch.model, V3(0, 0, 0), 1.0f, WHITE);
    }
}

void DrawSigns(int zone, Camera3D cam) {
    Zone& z = W.zones[zone];
    for (auto& s : z.signs) {
        if (Vector3Distance(s.pos, cam.position) > 120) continue;
        Vector3 d = YawDir(s.yaw);
        Vector3 right = V3(cosf(s.yaw), 0, -sinf(s.yaw));
        Vector3 up = V3(0, 1, 0);
        Vector3 c = s.pos + d * 0.02f;
        Vector3 tl = c - right * (s.w * 0.5f) + up * (s.h * 0.5f);
        Vector3 tr = c + right * (s.w * 0.5f) + up * (s.h * 0.5f);
        Vector3 bl = c - right * (s.w * 0.5f) - up * (s.h * 0.5f);
        Vector3 br = c + right * (s.w * 0.5f) - up * (s.h * 0.5f);
        float lv = s.emissive ? 1.0f : R.lightLevel;
        float fogK = 1.0f - expf(-powf(Vector3Distance(s.pos, cam.position) * R.fogDensity, 1.5f));
        Color tint = ColShade(s.tint, lv);
        Color fc = Color{(unsigned char)(R.fogColor.x * 255), (unsigned char)(R.fogColor.y * 255), (unsigned char)(R.fogColor.z * 255), 255};
        tint = ColLerp(tint, fc, fogK);
        rlSetTexture(s.tex.id);
        rlBegin(RL_QUADS);
        rlColor4ub(tint.r, tint.g, tint.b, 255);
        rlTexCoord2f(0, 0); rlVertex3f(tl.x, tl.y, tl.z);
        rlTexCoord2f(0, 1); rlVertex3f(bl.x, bl.y, bl.z);
        rlTexCoord2f(1, 1); rlVertex3f(br.x, br.y, br.z);
        rlTexCoord2f(1, 0); rlVertex3f(tr.x, tr.y, tr.z);
        rlEnd();
        rlSetTexture(0);
    }
}

int NearestLocation(Vector3 p, float* dist) {
    int best = -1;
    float bd = 1e9f;
    for (int i = 0; i < (int)W.locations.size(); i++) {
        float d = DistXZ(p, W.locations[i].pos);
        if (d < bd) { bd = d; best = i; }
    }
    if (dist) *dist = bd;
    return best;
}
