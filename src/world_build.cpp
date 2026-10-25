// Generazione procedurale del mondo: terreno, luoghi, interni, PNG e contenitori.
#include "game.h"

std::vector<Entity> gEntTemplates;
std::vector<Interactable> gInterTemplates;
std::vector<Vendor> gVendorTemplates;

namespace {

Zone* Z = nullptr;
Rng gen(20771);

const Color C_CONCRETE = Col(150, 146, 138), C_CONCRETE_D = Col(110, 106, 98), C_RUST = Col(128, 72, 46),
            C_RUST_L = Col(152, 98, 62), C_METAL = Col(104, 108, 112), C_METAL_D = Col(62, 64, 68),
            C_WOOD = Col(112, 84, 60), C_WOOD_D = Col(84, 66, 50), C_BRICK = Col(132, 76, 60),
            C_BONE = Col(225, 215, 190), C_VAULT_WALL = Col(122, 132, 144), C_VAULT_PANEL = Col(58, 76, 104),
            C_VAULT_YEL = Col(225, 180, 40), C_VAULT_FLOOR = Col(84, 88, 94), C_BLACK = Col(18, 18, 20);
const Color CORR[] = {Col(140, 86, 56), Col(92, 104, 112), Col(98, 108, 84), Col(118, 116, 108), Col(150, 120, 80), Col(120, 70, 50)};

// --- luoghi principali ---
const Vector3 L_VAULT = {-240, 0, 250};
const Vector3 L_FARM = {-120, 0, 170};
const Vector3 L_GAS = {60, 0, 180};
const Vector3 L_TOWN = {0, 0, 20};
const Vector3 L_RADIO = {-280, 0, -10};
const Vector3 L_CRATER = {-170, 0, -120};
const Vector3 L_CANYON = {-262, 0, -282};
const Vector3 L_RUINS = {75, 0, -235};
const Vector3 L_RAIDERS = {230, 0, -90};
const Vector3 L_BASE = {250, 0, 220};

float GY(float x, float z) { return W.terrain.Height(x, z); }

void Vis(Vector3 c, Vector3 s, Color col, float yaw = 0) { Z->B(c).BoxAt(c, s, col, yaw); }
void Solid(Vector3 c, Vector3 s, Color col, bool metal = false) { Z->AddBox(c, s, col, true, 0, metal); }
void Coll(Vector3 c, Vector3 s, bool metal = false) { Z->AddCollider(c - s * 0.5f, c + s * 0.5f, metal); }
void VisM(const Matrix& m, Color col, Vector3 at) { Z->B(at).Box(m, col); }
void Cyl(Vector3 base, float r, float h, Color col, int seg = 10, bool collide = true, float top = 1.0f) {
    Vector3 c = base + V3(0, h * 0.5f, 0);
    Z->B(c).Cylinder(MatTRS(c, V3(0, 0, 0), V3(r * 2, h, r * 2)), seg, col, true, top);
    if (collide) Z->AddCyl(base.x, base.z, r, base.y, base.y + h);
}
void Light(Vector3 p, Color c, float range, float intensity = 1.0f, float flicker = 0, bool night = false) {
    LightSrc l;
    l.pos = p;
    l.color = c;
    l.range = range;
    l.intensity = intensity;
    l.flicker = flicker;
    l.nightOnly = night;
    Z->lights.push_back(l);
}
void Emit(Vector3 p, EmitterType t, float rate, float size = 1) {
    Emitter e;
    e.pos = p;
    e.type = t;
    e.rate = rate;
    e.size = size;
    Z->emitters.push_back(e);
}
Color Glow(Color c, float amount = 1.0f) { c.a = (unsigned char)(255 * (1.0f - amount)); return c; }

// ---------------------------------------------------------------------------
//  Muri con aperture
// ---------------------------------------------------------------------------
struct Gap { float pos, w, bottom, h; };
enum WallStyle { WS_PLAIN, WS_RIBBED, WS_RUIN, WS_VAULT, WS_BUNKER, WS_BRICK };

void Wall(float x0, float z0, float x1, float z1, float y0, float h, float t, Color col, std::vector<Gap> gaps = {}, int style = WS_PLAIN) {
    bool alongX = fabsf(z1 - z0) < 0.001f;
    float len = alongX ? fabsf(x1 - x0) : fabsf(z1 - z0);
    float dir = alongX ? (x1 > x0 ? 1.f : -1.f) : (z1 > z0 ? 1.f : -1.f);
    auto P = [&](float s, float y) { return alongX ? V3(x0 + dir * s, y, z0) : V3(x0, y, z0 + dir * s); };
    auto Size = [&](float l, float hh, float th) { return alongX ? V3(l, hh, th) : V3(th, hh, l); };
    auto Piece = [&](float s0, float s1, float ya, float yb, bool topPiece) {
        if (s1 - s0 < 0.01f || yb - ya < 0.01f) return;
        Coll(P((s0 + s1) * 0.5f, (ya + yb) * 0.5f), Size(s1 - s0, yb - ya, t));
        if (style == WS_RIBBED || style == WS_RUIN || style == WS_BRICK) {
            float colW = style == WS_RIBBED ? 0.9f : 1.2f;
            int n = std::max(1, (int)roundf((s1 - s0) / colW));
            float w = (s1 - s0) / n;
            for (int i = 0; i < n; i++) {
                float a = s0 + i * w, b = a + w;
                float top = yb;
                if (style == WS_RUIN && topPiece) {
                    float k = gen.f();
                    if (k < 0.15f) top = ya + (yb - ya) * gen.range(0.05f, 0.3f);
                    else if (k < 0.6f) top = ya + (yb - ya) * gen.range(0.45f, 0.95f);
                }
                if (top - ya < 0.05f) continue;
                Color cc = style == WS_RIBBED ? ColJitter(col, gen, 0.10f) : ColJitter(col, gen, 0.05f);
                float th = style == WS_RIBBED ? t + ((i & 1) ? 0.05f : 0.0f) : t;
                Vis(P((a + b) * 0.5f, (ya + top) * 0.5f), Size(w + 0.002f, top - ya, th), cc);
                if (style == WS_RUIN && top < yb - 0.3f && gen.chance(0.3f)) {
                    // tondini di ferro sporgenti
                    Vector3 base = P(a + w * 0.5f, top);
                    Z->B(base).Cylinder(MatTRS(base + V3(0, 0.35f, 0), V3(gen.range(-0.3f, 0.3f), 0, gen.range(-0.3f, 0.3f)), V3(0.04f, 0.7f, 0.04f)), 4, C_RUST);
                }
            }
        } else if (style == WS_VAULT || style == WS_BUNKER) {
            float p1 = y0 + 1.1f, p2 = y0 + 1.28f;
            Color low = style == WS_VAULT ? C_VAULT_PANEL : C_CONCRETE_D;
            Color stripe = style == WS_VAULT ? C_VAULT_YEL : Col(200, 160, 40);
            auto Band = [&](float a, float b, Color c, float th) {
                a = std::max(a, ya);
                b = std::min(b, yb);
                if (b - a > 0.01f) Vis(P((s0 + s1) * 0.5f, (a + b) * 0.5f), Size(s1 - s0, b - a, th), c);
            };
            Band(ya, p1, low, t + 0.04f);
            Band(p1, p2, stripe, t + 0.05f);
            Band(p2, yb, col, t);
        } else {
            Vis(P((s0 + s1) * 0.5f, (ya + yb) * 0.5f), Size(s1 - s0, yb - ya, t), col);
        }
    };
    std::sort(gaps.begin(), gaps.end(), [](const Gap& a, const Gap& b) { return a.pos < b.pos; });
    float cur = 0;
    for (auto& g : gaps) {
        float s0 = g.pos - g.w * 0.5f, s1 = g.pos + g.w * 0.5f;
        Piece(cur, s0, y0, y0 + h, true);
        if (g.bottom > 0) Piece(s0, s1, y0, y0 + g.bottom, false);
        if (g.bottom + g.h < h) Piece(s0, s1, y0 + g.bottom + g.h, y0 + h, true);
        cur = s1;
    }
    Piece(cur, len, y0, y0 + h, true);
}

void Slab(float x0, float z0, float x1, float z1, float y, float th, Color col, bool collide = true) {
    Vector3 c = V3((x0 + x1) * 0.5f, y - th * 0.5f, (z0 + z1) * 0.5f);
    Vector3 s = V3(fabsf(x1 - x0), th, fabsf(z1 - z0));
    // suddivide i pavimenti grandi per i chunk
    int nx = std::max(1, (int)ceilf(s.x / 24)), nz = std::max(1, (int)ceilf(s.z / 24));
    float wx = s.x / nx, wz = s.z / nz;
    for (int i = 0; i < nx; i++)
        for (int j = 0; j < nz; j++) {
            Vector3 cc = V3(std::min(x0, x1) + wx * (i + 0.5f), c.y, std::min(z0, z1) + wz * (j + 0.5f));
            Vis(cc, V3(wx, th, wz), ColJitter(col, gen, 0.03f));
        }
    if (collide) Coll(c, s);
}

// stanza per interni: muri sui 4 lati con aperture, pavimento e soffitto
struct RGap { int side; float pos; float w; float h; };
enum { SIDE_N, SIDE_S, SIDE_W, SIDE_E };
void Room(float x0, float z0, float x1, float z1, float h, int style, Color wall, Color floor, Color ceil, std::vector<RGap> gaps, int openMask = 0, Color lamp = Col(205, 220, 255), float lampSpacing = 7.0f, float flicker = 0) {
    float t = 0.3f;
    for (int side = 0; side < 4; side++) {
        if (openMask & (1 << side)) continue;
        std::vector<Gap> g;
        for (auto& rg : gaps)
            if (rg.side == side) g.push_back({rg.pos, rg.w, 0, rg.h});
        switch (side) {
            case SIDE_N: Wall(x0 - t * 0.5f, z0, x1 + t * 0.5f, z0, 0, h, t, wall, [&] { for (auto& q : g) q.pos += t * 0.5f; return g; }(), style); break;
            case SIDE_S: Wall(x0 - t * 0.5f, z1, x1 + t * 0.5f, z1, 0, h, t, wall, [&] { for (auto& q : g) q.pos += t * 0.5f; return g; }(), style); break;
            case SIDE_W: Wall(x0, z0, x0, z1, 0, h, t, wall, g, style); break;
            case SIDE_E: Wall(x1, z0, x1, z1, 0, h, t, wall, g, style); break;
        }
    }
    Slab(x0, z0, x1, z1, 0.0f, 0.2f, floor, false);
    Slab(x0, z0, x1, z1, h + 0.25f, 0.25f, ceil, false);
    // lampade a soffitto
    int nx = std::max(1, (int)((x1 - x0) / lampSpacing)), nz = std::max(1, (int)((z1 - z0) / lampSpacing));
    for (int i = 0; i < nx; i++)
        for (int j = 0; j < nz; j++) {
            float lx = x0 + (x1 - x0) * (i + 0.5f) / nx, lz = z0 + (z1 - z0) * (j + 0.5f) / nz;
            Vis(V3(lx, h - 0.05f, lz), V3(1.0f, 0.1f, 0.35f), Glow(lamp, 0.9f));
            Light(V3(lx, h - 0.6f, lz), lamp, std::max(9.0f, lampSpacing * 1.45f), 0.9f, flicker > 0 && gen.chance(0.35f) ? flicker : 0);
        }
}

// ---------------------------------------------------------------------------
//  Cartelli con testo
// ---------------------------------------------------------------------------
void AddSign(Vector3 pos, float yaw, float w, float h, const char* text, Color bg, Color fg, bool emissive = false) {
    int pw = (int)(w * 64), ph = (int)(h * 64);
    pw = std::max(pw, 64);
    ph = std::max(ph, 32);
    Image img = GenImageColor(pw, ph, bg);
    // bordo e usura
    ImageDrawRectangleLines(&img, Rectangle{2, 2, (float)pw - 4, (float)ph - 4}, 3, ColShade(bg, 0.7f));
    Font& f = UIFontFor(ph * 0.62f, false);
    float fs = ph * 0.62f;
    Vector2 m = MeasureTextEx(f, text, fs, 2);
    if (m.x > pw - 16) {
        fs *= (pw - 16) / m.x;
        m = MeasureTextEx(f, text, fs, 2);
    }
    ImageDrawTextEx(&img, f, text, Vector2{(pw - m.x) * 0.5f, (ph - m.y) * 0.5f}, fs, 2, fg);
    Rng r((uint64_t)(pos.x * 31 + pos.z * 17));
    Color* px = (Color*)img.data;
    for (int i = 0; i < pw * ph; i++) {
        float n = r.f();
        if (n < 0.08f) px[i] = ColShade(px[i], 0.75f);
        else if (n > 0.995f) px[i] = ColShade(bg, 0.5f);
    }
    Sign s;
    s.pos = pos;
    s.yaw = yaw;
    s.w = w;
    s.h = h;
    s.tex = LoadTextureFromImage(img);
    GenTextureMipmaps(&s.tex);
    SetTextureFilter(s.tex, TEXTURE_FILTER_TRILINEAR);
    s.emissive = emissive;
    UnloadImage(img);
    Z->signs.push_back(s);
    // supporto dietro al cartello
    Vector3 back = pos - YawDir(yaw) * 0.06f;
    Vis(back, V3(w + 0.1f, h + 0.1f, 0.08f), C_WOOD_D, yaw);
}

// ---------------------------------------------------------------------------
//  Oggetti di scena
// ---------------------------------------------------------------------------
void Car(float x, float z, float yaw, Color paint) {
    float g = GY(x, z);
    Vector3 base = V3(x, g, z);
    Matrix rot = MatrixRotateY(yaw);
    auto L = [&](float lx, float ly, float lz) { return base + Vector3Transform(V3(lx, ly, lz), rot); };
    Color rust = ColLerp(paint, C_RUST, 0.45f);
    // carrozzeria
    VisM(MatTRS(V3(0, 0, 0), V3(0, 0, 0), V3(1.9f, 0.62f, 4.8f)) * rot * MatrixTranslate(L(0, 0.62f, 0).x, L(0, 0.62f, 0).y, L(0, 0.62f, 0).z), paint, base);
    VisM(MatTRS(V3(0, 0, 0), V3(0, 0, 0), V3(1.75f, 0.22f, 4.95f)) * rot * MatrixTranslate(L(0, 0.38f, 0).x, L(0, 0.38f, 0).y, L(0, 0.38f, 0).z), rust, base);
    // abitacolo
    VisM(MatTRS(V3(0, 0, 0), V3(0, 0, 0), V3(1.65f, 0.6f, 2.3f)) * rot * MatrixTranslate(L(0, 1.2f, -0.25f).x, L(0, 1.2f, -0.25f).y, L(0, 1.2f, -0.25f).z), ColShade(paint, 0.9f), base);
    VisM(MatTRS(V3(0, 0, 0), V3(0, 0, 0), V3(1.68f, 0.42f, 2.0f)) * rot * MatrixTranslate(L(0, 1.18f, -0.25f).x, L(0, 1.18f, -0.25f).y, L(0, 1.18f, -0.25f).z), Col(30, 34, 36), base);
    // pinne e paraurti cromati
    VisM(MatTRS(V3(0, 0, 0), V3(0, 0, 0), V3(2.0f, 0.18f, 0.15f)) * rot * MatrixTranslate(L(0, 0.45f, 2.45f).x, L(0, 0.45f, 2.45f).y, L(0, 0.45f, 2.45f).z), Col(150, 150, 145), base);
    VisM(MatTRS(V3(0, 0, 0), V3(0, 0, 0), V3(2.0f, 0.18f, 0.15f)) * rot * MatrixTranslate(L(0, 0.45f, -2.45f).x, L(0, 0.45f, -2.45f).y, L(0, 0.45f, -2.45f).z), Col(150, 150, 145), base);
    for (int s = -1; s <= 1; s += 2) {
        Vector3 fin = L(s * 0.85f, 1.0f, -2.0f);
        VisM(MatTRS(V3(0, 0, 0), V3(0.3f, 0, 0), V3(0.12f, 0.35f, 0.9f)) * rot * MatrixTranslate(fin.x, fin.y, fin.z), paint, base);
    }
    // ruote (alcune mancanti)
    for (int i = 0; i < 4; i++) {
        if (gen.chance(0.25f)) continue;
        float wx = (i & 1) ? 0.9f : -0.9f, wz = (i & 2) ? 1.55f : -1.55f;
        Vector3 wp = L(wx, 0.35f, wz);
        Matrix m = MatTRS(V3(0, 0, 0), V3(0, 0, PI / 2), V3(0.7f, 0.25f, 0.7f)) * rot * MatrixTranslate(wp.x, wp.y, wp.z);
        Z->B(base).Cylinder(m, 10, Col(30, 30, 30));
    }
    Z->AddBox(L(0, 0.75f, 0), V3(1.9f, 1.5f, 4.8f), paint, false, yaw);
    // collider
    Vector3 ax = Vector3Transform(V3(0, 0, 1), rot);
    for (int i = -1; i <= 1; i++) {
        Vector3 p = base + ax * (i * 1.6f);
        Z->AddCyl(p.x, p.z, 1.0f, g, g + 1.5f, true);
    }
}

void DeadTree(float x, float z, float h) {
    float g = GY(x, z) - 0.2f;
    Color bark = ColJitter(Col(78, 66, 54), gen, 0.12f);
    Vector3 b = V3(x, g, z);
    Z->B(b).Cylinder(MatTRS(b + V3(0, h * 0.5f, 0), V3(gen.range(-0.06f, 0.06f), 0, gen.range(-0.06f, 0.06f)), V3(0.32f, h, 0.32f)), 7, bark, true, 0.45f);
    int nb = gen.irange(2, 5);
    for (int i = 0; i < nb; i++) {
        float y = g + h * gen.range(0.45f, 0.9f);
        float a = gen.range(0, 2 * PI), tilt = gen.range(0.5f, 1.1f);
        float l = h * gen.range(0.25f, 0.45f);
        Vector3 s = V3(x, y, z);
        Vector3 e = s + V3(sinf(a) * sinf(tilt), cosf(tilt), cosf(a) * sinf(tilt)) * l;
        Z->B(b).Cylinder(MatAlignY(s, e, 0.07f), 5, bark, false, 0.4f);
    }
    Z->AddCyl(x, z, 0.25f, g, g + h);
}

void RockProp(float x, float z, float size, Color col = Col(120, 112, 100)) {
    float g = GY(x, z);
    Vector3 c = V3(x, g + size * 0.15f, z);
    Matrix m = MatTRS(c, V3(gen.range(-0.3f, 0.3f), gen.range(0, 6.28f), gen.range(-0.3f, 0.3f)), V3(size, size * gen.range(0.5f, 0.8f), size * gen.range(0.8f, 1.2f)));
    Z->B(c).Rock(m, gen, ColJitter(col, gen, 0.1f));
    if (size > 0.9f) Z->AddCyl(x, z, size * 0.42f, g - 1, g + size * 0.45f);
}

void Barrel(float x, float z, Color col, bool glowing = false, float y = -1000) {
    float g = y > -999 ? y : GY(x, z);
    Vector3 b = V3(x, g, z);
    Z->B(b).Cylinder(MatTRS(b + V3(0, 0.45f, 0), V3(0, 0, 0), V3(0.6f, 0.9f, 0.6f)), 10, col);
    Z->B(b).Cylinder(MatTRS(b + V3(0, 0.25f, 0), V3(0, 0, 0), V3(0.63f, 0.06f, 0.63f)), 10, ColShade(col, 0.7f));
    Z->B(b).Cylinder(MatTRS(b + V3(0, 0.68f, 0), V3(0, 0, 0), V3(0.63f, 0.06f, 0.63f)), 10, ColShade(col, 0.7f));
    if (glowing) {
        Z->B(b).Cylinder(MatTRS(b + V3(0, 0.91f, 0), V3(0, 0, 0), V3(0.5f, 0.02f, 0.5f)), 10, Glow(Col(120, 255, 60)));
        Light(b + V3(0, 1.2f, 0), Col(90, 255, 60), 5, 0.6f);
    }
    Z->AddCyl(x, z, 0.32f, g, g + 0.9f, true);
}

void FireBarrel(float x, float z, float y = -1000) {
    float g = y > -999 ? y : GY(x, z);
    Barrel(x, z, C_RUST, false, g);
    Emit(V3(x, g + 0.95f, z), EM_FIRE, 18, 0.8f);
    Emit(V3(x, g + 1.8f, z), EM_SMOKE, 2.5f, 1.0f);
    Light(V3(x, g + 1.6f, z), Col(255, 140, 50), 11, 1.3f, 0.35f);
}

void Crate(float x, float z, float s, float y = -1000, float yaw = 0) {
    float g = y > -999 ? y : GY(x, z);
    Color c = ColJitter(C_WOOD, gen, 0.12f);
    Vector3 p = V3(x, g + s * 0.5f, z);
    Z->AddBox(p, V3(s, s, s), c, true, yaw);
    Vis(p, V3(s * 1.02f, s * 0.12f, s * 1.02f), ColShade(c, 0.75f), yaw);
}

void Pole(float x, float z, float h = 8) {
    float g = GY(x, z);
    Cyl(V3(x, g, z), 0.13f, h, C_WOOD_D, 6);
    Vis(V3(x, g + h - 0.6f, z), V3(1.8f, 0.12f, 0.12f), C_WOOD_D);
}

void Tire(float x, float z) {
    float g = GY(x, z);
    Vector3 b = V3(x, g + 0.12f, z);
    Z->B(b).Torus(MatTRS(b, V3(0, gen.range(0, 3), 0), V3(1, 1, 1)), 0.32f, 0.12f, 10, 6, Col(28, 28, 28));
}

// sterpaglia: rami secchi e sassi sparsi
void Brush(float x, float z, int n) {
    for (int i = 0; i < n; i++) {
        float bx = x + gen.range(-2.5f, 2.5f), bz = z + gen.range(-2.5f, 2.5f);
        float g = GY(bx, bz);
        Vector3 a = V3(bx, g + 0.06f, bz);
        float ang = gen.range(0, 6.28f), l = gen.range(0.4f, 1.1f);
        Z->B(a).Cylinder(MatAlignY(a, a + V3(sinf(ang) * l, gen.range(0.0f, 0.3f), cosf(ang) * l), 0.04f), 4, C_WOOD_D);
        if (gen.chance(0.3f)) Z->B(a).Sphere(MatTRS(a + V3(0, 0.05f, 0), V3(0, gen.range(0, 3), 0), V3(0.3f, 0.18f, 0.25f)), 3, 5, Col(120, 110, 96));
    }
}

void Sandbags(float x, float z, float len, float yaw) {
    float g = GY(x, z);
    Z->AddBox(V3(x, g + 0.45f, z), V3(len, 0.9f, 0.7f), Col(150, 135, 100), true, yaw);
    Vis(V3(x, g + 0.95f, z), V3(len - 0.2f, 0.2f, 0.6f), Col(140, 125, 92), yaw);
}

// baracca in lamiera con porta e tetto inclinato
void Shack(float cx, float cz, float w, float d, float h, int doorSide, float doorOff = 0, Color tint = WHITE, bool windows = true) {
    float g = GY(cx, cz);
    float x0 = cx - w / 2, x1 = cx + w / 2, z0 = cz - d / 2, z1 = cz + d / 2;
    Color col = CORR[gen.irange(0, 5)];
    if (tint.r != 255 || tint.g != 255) col = tint;
    float t = 0.15f;
    auto G = [&](int side) {
        std::vector<Gap> gaps;
        float len = (side < 2) ? w : d;
        if (side == doorSide) gaps.push_back({len / 2 + doorOff, 1.4f, 0, 2.3f});
        if (windows && side != doorSide && len > 5 && gen.chance(0.7f)) gaps.push_back({len * 0.3f, 1.2f, 1.2f, 0.9f});
        return gaps;
    };
    Wall(x0, z0, x1, z0, g - 0.3f, h + 0.3f, t, col, G(SIDE_N), WS_RIBBED);
    Wall(x0, z1, x1, z1, g - 0.3f, h + 0.3f, t, ColJitter(col, gen, 0.1f), G(SIDE_S), WS_RIBBED);
    Wall(x0, z0, x0, z1, g - 0.3f, h + 0.3f, t, ColJitter(col, gen, 0.1f), G(SIDE_W), WS_RIBBED);
    Wall(x1, z0, x1, z1, g - 0.3f, h + 0.3f, t, ColJitter(col, gen, 0.1f), G(SIDE_E), WS_RIBBED);
    // pavimento in assi
    Slab(x0, z0, x1, z1, g + 0.12f, 0.3f, C_WOOD_D, true);
    // tetto inclinato
    Color roof = CORR[gen.irange(0, 5)];
    float slope = 0.12f;
    Vector3 rc = V3(cx, g + h + 0.35f, cz);
    VisM(MatTRS(V3(0, 0, 0), V3(slope, 0, 0), V3(w + 0.8f, 0.12f, d + 0.8f)) * MatrixTranslate(rc.x, rc.y, rc.z), roof, rc);
    // lampada interna
    Light(V3(cx, g + h - 0.4f, cz), Col(255, 190, 120), std::max(w, d) * 0.9f, 0.7f, 0.1f);
    Vis(V3(cx, g + h - 0.15f, cz), V3(0.25f, 0.25f, 0.25f), Glow(Col(255, 200, 120)));
}

// ---------------------------------------------------------------------------
//  Interattivi e PNG (modelli dinamici)
// ---------------------------------------------------------------------------
Interactable& AddInter(InterType type, int zone, Vector3 pos, float yaw, const char* name, const char* tag = "") {
    Interactable it;
    it.type = type;
    it.zone = zone;
    it.pos = pos;
    it.yaw = yaw;
    it.name = name;
    it.tag = tag;
    gInterTemplates.push_back(it);
    return gInterTemplates.back();
}

Vector3 ContSize(int model) {
    switch (model) {
        case CM_CRATE: return V3(0.9f, 0.7f, 0.7f);
        case CM_AMMOBOX: return V3(0.6f, 0.35f, 0.35f);
        case CM_FIRSTAID: return V3(0.5f, 0.4f, 0.15f);
        case CM_FRIDGE: return V3(0.8f, 1.7f, 0.7f);
        case CM_DESK: return V3(1.6f, 0.8f, 0.8f);
        case CM_LOCKER: return V3(0.6f, 1.9f, 0.5f);
        case CM_SAFE: return V3(0.7f, 0.8f, 0.7f);
        case CM_FOOTLOCKER: return V3(1.0f, 0.5f, 0.55f);
        case CM_TOOLBOX: return V3(0.6f, 0.3f, 0.3f);
        case CM_TRASH: return V3(0.7f, 0.9f, 0.7f);
        default: return V3(0.5f, 0.5f, 0.5f);
    }
}

const char* ContName(int model) {
    switch (model) {
        case CM_CRATE: return "Cassa";
        case CM_AMMOBOX: return "Cassetta militare";
        case CM_FIRSTAID: return "Cassetta di pronto soccorso";
        case CM_FRIDGE: return "Frigorifero";
        case CM_DESK: return "Scrivania";
        case CM_LOCKER: return "Armadietto";
        case CM_SAFE: return "Cassaforte";
        case CM_FOOTLOCKER: return "Baule";
        case CM_TOOLBOX: return "Cassetta degli attrezzi";
        case CM_TRASH: return "Bidone";
        default: return "Contenitore";
    }
}

Interactable& Cont(int zone, float x, float z, float yaw, int model, int tier, int lock = 0, const char* tag = "", float y = -1000) {
    float g = y > -999 ? y : (zone == ZONE_WASTE ? GY(x, z) : W.zones[zone].floorY);
    if (model == CM_FIRSTAID) g += 1.2f;
    Interactable& it = AddInter(INT_CONTAINER, zone, V3(x, g, z), yaw, ContName(model), tag);
    it.model = model;
    it.size = ContSize(model);
    it.lock = lock;
    it.locked = lock > 0;
    GenContainerLoot(it.inv, model, gen, tier);
    if (model == CM_FRIDGE || model == CM_LOCKER || model == CM_SAFE || model == CM_DESK) {
        Vector3 s = it.size;
        if (fabsf(sinf(yaw)) > 0.7f) std::swap(s.x, s.z);
        Zone& zz = W.zones[zone];
        zz.AddCollider(V3(x - s.x / 2, g, z - s.z / 2), V3(x + s.x / 2, g + s.y, z + s.z / 2), true);
    }
    return it;
}

Entity& Spawn(EntKind k, int zone, float x, float z, float yaw = 0) {
    float y = zone == ZONE_WASTE ? GY(x, z) : W.zones[zone].floorY;
    gEntTemplates.push_back(CreateEntity(k, zone, V3(x, y, z), yaw));
    return gEntTemplates.back();
}

Entity& NPC(int zone, float x, float z, float yaw, const char* name, const char* dlg, int outfit, bool female, Faction fac) {
    Entity& e = Spawn(EK_HUMAN, zone, x, z, yaw);
    e.name = name;
    e.dlg = dlg;
    e.look.outfit = outfit;
    e.look.female = female;
    e.fac = fac;
    e.hostile = false;
    e.wanderRadius = 3;
    static const Color skins[] = {Col(232, 190, 160), Col(205, 160, 130), Col(170, 120, 90), Col(120, 82, 60), Col(225, 175, 140)};
    static const Color hairs[] = {Col(40, 30, 25), Col(90, 60, 35), Col(150, 110, 60), Col(160, 160, 155), Col(25, 22, 20), Col(120, 50, 30)};
    e.look.skin = skins[gen.irange(0, 4)];
    e.look.hair = hairs[gen.irange(0, 5)];
    e.look.hairStyle = female ? 2 : gen.irange(0, 1) + (gen.chance(0.7f) ? 1 : 0);
    if (e.look.hairStyle > 2) e.look.hairStyle = 1;
    return e;
}

Entity& Raider(float x, float z, const char* tag = "") {
    Entity& e = Spawn(EK_HUMAN, ZONE_WASTE, x, z, gen.range(0, 6.28f));
    e.name = "Predone";
    e.tag = tag;
    e.fac = FAC_RAIDER;
    e.hostile = true;
    e.look.outfit = OF_RAIDER;
    e.look.female = gen.chance(0.3f);
    e.look.hairStyle = gen.chance(0.5f) ? 3 : gen.irange(0, 2);
    e.look.hair = gen.chance(0.5f) ? Col(170, 40, 30) : Col(40, 30, 25);
    e.look.skin = ColJitter(Col(200, 155, 125), gen, 0.15f);
    e.look.hat = gen.chance(0.3f) ? 3 : 0;
    e.detect = 28;
    e.speed = 4.6f;
    e.territory = 34;
    e.wanderRadius = 7;
    return e;
}

// ---------------------------------------------------------------------------
//  Terreno
// ---------------------------------------------------------------------------
struct Flat { float x, z, r, target; bool rect; float x0, z0, x1, z1; float blend; };
std::vector<Flat> flats;
struct Road { std::vector<Vector3> pts; float width; bool asphalt; };
std::vector<Road> roads;

float DistSeg(float px, float pz, float ax, float az, float bx, float bz, float* tOut) {
    float vx = bx - ax, vz = bz - az;
    float l2 = vx * vx + vz * vz;
    float t = l2 > 0 ? Clamp(((px - ax) * vx + (pz - az) * vz) / l2, 0.f, 1.f) : 0;
    if (tOut) *tOut = t;
    float dx = px - (ax + vx * t), dz = pz - (az + vz * t);
    return sqrtf(dx * dx + dz * dz);
}

float BaseHeight(float x, float z) {
    float h = Fbm(x / 170.f, z / 170.f, 4, 11) * 26 - 12;
    h += (Fbm(x / 55.f, z / 55.f, 3, 23) - 0.5f) * 7;
    h += (ValueNoise(x / 11.f, z / 11.f, 5) - 0.5f) * 0.8f;
    // montagne ai bordi
    float e = std::max(fabsf(x), fabsf(z)) / WORLD_HALF;
    if (e > 0.72f) {
        float k = (e - 0.72f) / 0.28f;
        h += k * k * 95 * (0.65f + 0.7f * Fbm(x / 38.f, z / 38.f, 3, 31));
    }
    // collina del Rifugio
    {
        float dx = x - L_VAULT.x, dz = z - (L_VAULT.z + 34);
        float d2 = dx * dx + dz * dz;
        h += 38 * expf(-d2 / (2 * 26 * 26));
    }
    // cratere
    {
        float d = DistXZ(V3(x, 0, z), L_CRATER);
        float r = 48;
        if (d < r) h -= 12 * (1 - (d / r) * (d / r));
        h += 4.5f * expf(-((d - r) / 7) * ((d - r) / 7));
    }
    // gola (canyon) verso la tana dello Squarciatore
    {
        float t;
        float d = DistSeg(x, z, -185, -195, L_CANYON.x, L_CANYON.z, &t);
        float k = SmoothStepF(0.0f, 0.35f, t);
        if (d < 15) h -= 13 * k * (1 - SmoothStepF(6, 15, d));
        h += 20 * k * expf(-((d - 25) / 9) * ((d - 25) / 9));
    }
    return h;
}

void GenTerrain() {
    Terrain& T = W.terrain;
    const int N1 = TER_N + 1;
    T.h.assign(N1 * N1, 0);
    T.col.assign(N1 * N1, WHITE);
    for (int j = 0; j < N1; j++)
        for (int i = 0; i < N1; i++) T.h[j * N1 + i] = BaseHeight(-WORLD_HALF + i * TER_CELL, -WORLD_HALF + j * TER_CELL);

    auto F = [&](Vector3 c, float r, float blend, float sx, float sz) {
        Flat f{};
        f.x = c.x; f.z = c.z; f.r = r; f.blend = blend; f.rect = false;
        f.target = BaseHeight(sx, sz);
        flats.push_back(f);
    };
    auto FR = [&](float x0, float z0, float x1, float z1, float blend) {
        Flat f{};
        f.rect = true; f.x0 = x0; f.z0 = z0; f.x1 = x1; f.z1 = z1; f.blend = blend;
        f.target = BaseHeight((x0 + x1) / 2, (z0 + z1) / 2);
        flats.push_back(f);
    };
    F(L_TOWN, 54, 24, L_TOWN.x, L_TOWN.z);
    F(L_GAS, 30, 16, L_GAS.x, L_GAS.z);
    F(L_RAIDERS, 36, 16, L_RAIDERS.x, L_RAIDERS.z);
    F(L_FARM, 32, 14, L_FARM.x, L_FARM.z);
    F(L_RADIO, 16, 10, L_RADIO.x, L_RADIO.z);
    F(V3(L_VAULT.x, 0, L_VAULT.z - 10), 13, 7, L_VAULT.x, L_VAULT.z - 22);
    F(L_CANYON, 16, 8, L_CANYON.x, L_CANYON.z);
    FR(196, 166, 304, 274, 14);
    FR(0, -312, 150, -158, 12);

    for (int j = 0; j < N1; j++)
        for (int i = 0; i < N1; i++) {
            float x = -WORLD_HALF + i * TER_CELL, z = -WORLD_HALF + j * TER_CELL;
            float& h = T.h[j * N1 + i];
            for (auto& f : flats) {
                float d;
                if (f.rect) {
                    float dx = std::max(std::max(f.x0 - x, 0.0f), x - f.x1);
                    float dz = std::max(std::max(f.z0 - z, 0.0f), z - f.z1);
                    d = sqrtf(dx * dx + dz * dz);
                    float w = 1.0f - SmoothStepF(0, f.blend, d);
                    h = LerpF(h, f.target, w);
                } else {
                    d = sqrtf((x - f.x) * (x - f.x) + (z - f.z) * (z - f.z));
                    float w = 1.0f - SmoothStepF(f.r, f.r + f.blend, d);
                    h = LerpF(h, f.target, w);
                }
            }
        }

    // strade
    auto RD = [&](std::initializer_list<Vector2> pts, float w, bool asphalt) {
        Road r;
        for (auto& p : pts) r.pts.push_back(V3(p.x, 0, p.y));
        r.width = w;
        r.asphalt = asphalt;
        roads.push_back(r);
    };
    RD({{-240, 236}, {-215, 205}, {-175, 186}, {-138, 172}, {-90, 140}, {-50, 102}, {-15, 76}, {0, 66}}, 4.0f, true);
    RD({{45, 20}, {90, 5}, {140, -25}, {185, -58}, {200, -88}}, 4.0f, true);
    RD({{0, 66}, {25, 110}, {48, 150}, {60, 168}, {100, 195}, {150, 212}, {196, 220}}, 4.0f, true);
    RD({{0, -26}, {6, -70}, {22, -120}, {40, -160}, {40, -312}}, 4.0f, true);
    RD({{0, -205}, {150, -205}}, 4.0f, true);
    RD({{0, -262}, {150, -262}}, 4.0f, true);
    RD({{110, -160}, {110, -312}}, 4.0f, true);
    RD({{-45, 20}, {-100, 15}, {-160, 5}, {-220, -5}, {-268, -10}}, 3.5f, true);
    RD({{-160, 5}, {-163, -40}, {-160, -70}}, 2.5f, false);
    RD({{-138, 172}, {-120, 160}}, 2.5f, false);
    RD({{-185, -195}, {-150, -160}, {-120, -100}, {-100, -40}, {-100, 15}}, 2.5f, false);
    for (auto& r : roads) {
        for (auto& p : r.pts) p.y = T.Height(p.x, p.z);
        for (int pass = 0; pass < 2; pass++)
            for (size_t k = 1; k + 1 < r.pts.size(); k++) r.pts[k].y = (r.pts[k - 1].y + r.pts[k].y * 2 + r.pts[k + 1].y) / 4;
        W.roads.push_back(r.pts);
    }
    std::vector<float> roadW(N1 * N1, 0.f);
    std::vector<char> roadA(N1 * N1, 0);
    std::vector<float> newH = T.h;
    for (int j = 0; j < N1; j++)
        for (int i = 0; i < N1; i++) {
            float x = -WORLD_HALF + i * TER_CELL, z = -WORLD_HALF + j * TER_CELL;
            float best = 1e9f, target = 0;
            const Road* br = nullptr;
            for (auto& r : roads)
                for (size_t k = 0; k + 1 < r.pts.size(); k++) {
                    const Vector3 &a = r.pts[k], &b = r.pts[k + 1];
                    if (x < std::min(a.x, b.x) - 14 || x > std::max(a.x, b.x) + 14 || z < std::min(a.z, b.z) - 14 || z > std::max(a.z, b.z) + 14) continue;
                    float t;
                    float d = DistSeg(x, z, a.x, a.z, b.x, b.z, &t) - r.width;
                    if (d < best) { best = d; target = LerpF(a.y, b.y, t); br = &r; }
                }
            if (!br) continue;
            float w = 1.0f - SmoothStepF(0.0f, 7.0f, best);
            newH[j * N1 + i] = LerpF(T.h[j * N1 + i], target, w);
            roadW[j * N1 + i] = 1.0f - SmoothStepF(-1.0f, 1.5f, best);
            roadA[j * N1 + i] = br->asphalt;
        }
    T.h = newH;

    // colori
    for (int j = 0; j < N1; j++)
        for (int i = 0; i < N1; i++) {
            float x = -WORLD_HALF + i * TER_CELL, z = -WORLD_HALF + j * TER_CELL;
            float h = T.h[j * N1 + i];
            Vector3 n = T.Normal(x, z);
            float n1 = Fbm(x / 45.f, z / 45.f, 3, 41);
            float n2 = ValueNoise(x / 6.f, z / 6.f, 43);
            Color sand = Col(176, 150, 112), dirt = Col(140, 108, 78), rock = Col(118, 110, 100), grass = Col(124, 118, 72);
            Color c = ColLerp(sand, dirt, Clamp01(n1 * 1.8f - 0.55f));
            float gp = Fbm(x / 28.f + 100, z / 28.f, 3, 47);
            if (gp > 0.6f) c = ColLerp(c, grass, Clamp01((gp - 0.6f) * 6) * 0.8f);
            float slope = 1.0f - n.y;
            if (slope > 0.18f) c = ColLerp(c, rock, Clamp01((slope - 0.18f) * 4));
            if (h > 35) c = ColLerp(c, Col(150, 140, 125), Clamp01((h - 35) / 30));
            // cratere
            float dc = DistXZ(V3(x, 0, z), L_CRATER);
            if (dc < 60) c = ColLerp(c, Col(78, 90, 62), (1 - SmoothStepF(25, 60, dc)) * 0.85f);
            if (dc < 20) c = ColLerp(c, Col(60, 90, 40), 1 - dc / 20);
            // città e luoghi: terra battuta
            for (auto& f : flats) {
                if (f.rect) continue;
                float d = sqrtf((x - f.x) * (x - f.x) + (z - f.z) * (z - f.z));
                if (d < f.r) c = ColLerp(c, Col(128, 106, 84), (1 - d / f.r) * 0.5f);
            }
            if (x > 0 && x < 150 && z > -312 && z < -158) c = ColLerp(c, Col(105, 100, 94), 0.6f); // rovine: cemento e cenere
            if (x > 196 && x < 304 && z > 166 && z < 274) c = ColLerp(c, Col(118, 114, 104), 0.55f); // base
            float rw = roadW[j * N1 + i];
            if (rw > 0) {
                Color road = roadA[j * N1 + i] ? Col(66, 64, 62) : Col(120, 98, 74);
                if (roadA[j * N1 + i] && n2 > 0.72f) road = Col(92, 88, 80); // crepe
                c = ColLerp(c, road, rw);
            }
            c = ColShade(c, 0.92f + n2 * 0.16f);
            T.col[j * N1 + i] = c;
        }

    // mesh del terreno
    Z = &W.zones[ZONE_WASTE];
    std::vector<Vector3> normals(N1 * N1);
    for (int j = 0; j < N1; j++)
        for (int i = 0; i < N1; i++) normals[j * N1 + i] = T.Normal(-WORLD_HALF + i * TER_CELL, -WORLD_HALF + j * TER_CELL);
    for (int j = 0; j < TER_N; j++)
        for (int i = 0; i < TER_N; i++) {
            auto P = [&](int a, int b) { return V3(-WORLD_HALF + a * TER_CELL, T.h[b * N1 + a], -WORLD_HALF + b * TER_CELL); };
            int i00 = j * N1 + i, i10 = j * N1 + i + 1, i01 = (j + 1) * N1 + i, i11 = (j + 1) * N1 + i + 1;
            Vector3 p00 = P(i, j), p10 = P(i + 1, j), p01 = P(i, j + 1), p11 = P(i + 1, j + 1);
            MeshBuilder& b = Z->B((p00 + p11) * 0.5f);
            b.TriN(p00, p01, p10, normals[i00], normals[i01], normals[i10], T.col[i00], T.col[i01], T.col[i10]);
            b.TriN(p10, p01, p11, normals[i10], normals[i01], normals[i11], T.col[i10], T.col[i01], T.col[i11]);
        }
}

void GenMapTexture() {
    const int S = 512;
    Image img = GenImageColor(S, S, BLACK);
    Color* px = (Color*)img.data;
    for (int y = 0; y < S; y++)
        for (int x = 0; x < S; x++) {
            float wx = -WORLD_HALF + (x + 0.5f) / S * WORLD_HALF * 2, wz = -WORLD_HALF + (y + 0.5f) / S * WORLD_HALF * 2;
            Vector3 n = W.terrain.Normal(wx, wz);
            float shade = Clamp01(Vector3DotProduct(n, Vector3Normalize(V3(-0.6f, 0.7f, -0.4f))));
            float h = W.terrain.Height(wx, wz);
            float lum = 0.18f + shade * 0.42f + Clamp01(h / 120.f) * 0.15f;
            // linee di livello
            if (fmodf(h + 1000, 10.0f) < 0.6f) lum += 0.07f;
            px[y * S + x] = Color{(unsigned char)(lum * 40), (unsigned char)(lum * 255), (unsigned char)(lum * 110), 255};
        }
    for (auto& r : W.roads)
        for (size_t k = 0; k + 1 < r.size(); k++) {
            auto M = [&](Vector3 p) { return Vector2{(p.x + WORLD_HALF) / (WORLD_HALF * 2) * S, (p.z + WORLD_HALF) / (WORLD_HALF * 2) * S}; };
            Vector2 a = M(r[k]), b = M(r[k + 1]);
            ImageDrawLineEx(&img, a, b, 3, Color{40, 200, 110, 255});
        }
    W.mapTex = LoadTextureFromImage(img);
    SetTextureFilter(W.mapTex, TEXTURE_FILTER_BILINEAR);
    UnloadImage(img);
}

// ---------------------------------------------------------------------------
//  Luoghi esterni
// ---------------------------------------------------------------------------
void BuildVaultExterior() {
    float x = L_VAULT.x, z = L_VAULT.z;
    float g = GY(x, z - 6);
    // facciata in cemento incassata nella montagna
    Solid(V3(x, g + 6, z + 6), V3(22, 12, 12), C_CONCRETE_D);
    Solid(V3(x - 8.5f, g + 4, z - 0.5f), V3(5, 8, 2), C_CONCRETE);
    Solid(V3(x + 8.5f, g + 4, z - 0.5f), V3(5, 8, 2), C_CONCRETE);
    Solid(V3(x, g + 9.2f, z - 0.5f), V3(22, 2.4f, 2.4f), C_CONCRETE);
    Vis(V3(x, g + 4, z + 0.05f), V3(12, 8, 0.4f), C_BLACK);
    // strisce di pericolo
    for (int i = 0; i < 8; i++)
        Vis(V3(x - 10.5f + i * 3, g + 10.6f, z - 1.75f), V3(1.4f, 0.4f, 0.1f), i & 1 ? C_VAULT_YEL : C_BLACK);
    AddSign(V3(x, g + 9.2f, z - 1.72f), PI, 6, 1.1f, "RIFUGIO 33", C_VAULT_PANEL, C_VAULT_YEL);
    Light(V3(x, g + 7.5f, z - 4), Col(255, 230, 180), 14, 0.8f, 0.0f, true);
    Vis(V3(x, g + 8.2f, z - 1.9f), V3(0.6f, 0.2f, 0.4f), Glow(Col(255, 230, 180)));
    // piattaforma
    Slab(x - 10, z - 12, x + 10, z - 1.5f, g + 0.25f, 0.5f, C_CONCRETE_D);
    for (int i = 0; i < 5; i++) RockProp(x + gen.range(-14, 14), z + gen.range(-2, 6), gen.range(1.5f, 3.5f));
    Barrel(x - 7, z - 9, C_RUST);
    Crate(x + 7, z - 8, 1.0f);
    Crate(x + 7.5f, z - 6.8f, 0.8f);
    Interactable& d = AddInter(INT_TRANSITION, ZONE_WASTE, V3(x, g + 0.5f, z - 0.2f), PI, "Entra nel Rifugio 33", "vault_door_out");
    d.targetZone = ZONE_VAULT;
    d.targetPos = V3(0, 0, -20);
    d.targetYaw = 0;
    d.size = V3(10, 8, 1);
    W.locations.push_back({"Rifugio 33", V3(x, g, z), 30, false, V3(x, g, z - 14), 2});
}

void BuildTown() {
    float cx = L_TOWN.x, cz = L_TOWN.z;
    float g = GY(cx, cz);
    // mura di lamiera
    float R0 = 45;
    int N = 68;
    for (int i = 0; i < N; i++) {
        float a = (float)i / N * 2 * PI;
        bool gate = false;
        for (float ga : {0.0f, PI / 2, PI, -PI / 2, 3 * PI / 2})
            if (fabsf(AngleWrap(a - ga)) < 0.115f) gate = true;
        if (gate) continue;
        float px = cx + sinf(a) * R0, pz = cz + cosf(a) * R0;
        float h = gen.range(3.8f, 5.0f);
        float gg = GY(px, pz);
        Color c = CORR[gen.irange(0, 5)];
        Vector3 ctr = V3(px, gg + h * 0.5f - 0.3f, pz);
        Matrix m = MatTRS(V3(0, 0, 0), V3(gen.range(-0.04f, 0.04f), 0, gen.range(-0.03f, 0.03f)), V3(4.4f, h, 0.18f)) * MatrixRotateY(a) * MatrixTranslate(ctr.x, ctr.y, ctr.z);
        VisM(m, c, ctr);
        // nervature
        for (int k = -2; k <= 2; k++) {
            Vector3 rp = ctr + V3(cosf(a), 0, -sinf(a)) * (k * 0.85f);
            Matrix rm = MatTRS(V3(0, 0, 0), V3(0, 0, 0), V3(0.12f, h, 0.24f)) * MatrixRotateY(a) * MatrixTranslate(rp.x, rp.y, rp.z);
            VisM(rm, ColShade(c, 0.85f), rp);
        }
        Z->AddBox(ctr, V3(4.4f, h, 0.3f), c, false, a);
        Z->AddCyl(px, pz, 0.6f, gg - 1, gg + h);
        for (int k = -1; k <= 1; k += 2) {
            Vector3 cp = V3(px, 0, pz) + V3(cosf(a), 0, -sinf(a)) * (k * 1.5f);
            Z->AddCyl(cp.x, cp.z, 0.6f, gg - 1, gg + h);
        }
    }
    // pali ai cancelli e cartello
    for (float ga : {0.0f, PI / 2, PI, -PI / 2}) {
        for (int s = -1; s <= 1; s += 2) {
            float a = ga + s * 0.125f;
            float px = cx + sinf(a) * R0, pz = cz + cosf(a) * R0;
            Cyl(V3(px, GY(px, pz) - 0.5f, pz), 0.3f, 6.5f, C_RUST, 8);
        }
    }
    {
        Vector3 sp = V3(cx, GY(cx, cz + R0) + 5.6f, cz + R0 + 0.1f);
        Vis(sp - V3(0, 0, 0.1f), V3(9.5f, 0.3f, 0.3f), C_RUST);
        AddSign(sp + V3(0, 0, 0.12f), 0, 7.5f, 1.3f, "FERROCITTÀ", Col(150, 60, 40), Col(235, 225, 200));
        AddSign(sp + V3(0, 0, -0.32f), PI, 7.5f, 1.3f, "FERROCITTÀ", Col(150, 60, 40), Col(235, 225, 200));
    }
    // edifici principali
    // Ufficio dello sceriffo (porta a sud)
    Shack(cx - 20, cz - 16, 10, 8, 3.6f, SIDE_S, 0, Col(110, 100, 90));
    AddSign(V3(cx - 20, g + 3.3f, cz - 11.85f), 0, 3.2f, 0.7f, "SCERIFFO", Col(70, 60, 50), Col(230, 200, 120));
    Cont(ZONE_WASTE, cx - 23.5f, cz - 18.5f, 0, CM_LOCKER, 2, 2);
    Cont(ZONE_WASTE, cx - 17, cz - 18.8f, 0, CM_DESK, 1);
    // Emporio di Gus (porta a sud) con tettoia
    Shack(cx + 20, cz - 16, 11, 8, 3.8f, SIDE_S, 0, Col(140, 86, 56));
    AddSign(V3(cx + 20, g + 3.4f, cz - 11.85f), 0, 3.6f, 0.75f, "EMPORIO", Col(60, 80, 90), Col(240, 220, 150));
    VisM(MatTRS(V3(0, 0, 0), V3(-0.25f, 0, 0), V3(10, 0.1f, 3)) * MatrixTranslate(cx + 20, g + 3.0f, cz - 10.6f), Col(150, 70, 50), V3(cx + 20, 0, cz - 10.6f));
    for (int s = -1; s <= 1; s += 2) Cyl(V3(cx + 20 + s * 4.8f, g, cz - 9.3f), 0.08f, 2.6f, C_WOOD_D, 5);
    Solid(V3(cx + 20, g + 0.6f, cz - 17.5f), V3(6, 1.0f, 0.8f), C_WOOD);
    for (int i = 0; i < 4; i++) Crate(cx + 16 + i * 1.1f, cz - 19, 0.8f, g + 0.27f);
    // Bar di Tano (porta a nord)
    Shack(cx - 22, cz + 18, 12, 10, 3.8f, SIDE_N, 0, Col(98, 108, 84));
    AddSign(V3(cx - 22, g + 4.6f, cz + 12.8f), PI, 3.0f, 1.0f, "BAR", Col(30, 20, 30), Col(255, 90, 160), true);
    Light(V3(cx - 22, g + 4.6f, cz + 11.8f), Col(255, 90, 160), 9, 0.9f, 0.05f, true);
    Solid(V3(cx - 22, g + 0.65f, cz + 20.5f), V3(7, 1.1f, 0.8f), C_WOOD);
    for (int i = 0; i < 3; i++) {
        float tx = cx - 25 + i * 3.0f, tz = cz + 15.5f;
        Cyl(V3(tx, g + 0.27f, tz), 0.6f, 0.85f, C_WOOD, 8, true);
    }
    Cont(ZONE_WASTE, cx - 26.5f, cz + 21.5f, PI, CM_FRIDGE, 1);
    // Clinica (porta a nord)
    Shack(cx + 22, cz + 18, 10, 9, 3.6f, SIDE_N, 0, Col(170, 168, 160));
    AddSign(V3(cx + 22, g + 4.3f, cz + 13.3f), PI, 3.4f, 0.8f, "+ CLINICA +", Col(220, 220, 215), Col(190, 30, 30));
    Solid(V3(cx + 24.5f, g + 0.5f, cz + 19), V3(1.0f, 0.6f, 2.2f), Col(200, 200, 195));
    Cont(ZONE_WASTE, cx + 19, cz + 21.8f, PI, CM_FIRSTAID, 1, 1);
    // case
    Shack(cx - 30, cz - 2, 7, 6, 3.0f, SIDE_E);
    Shack(cx + 31, cz - 1, 7, 6, 3.0f, SIDE_W);
    Shack(cx - 6, cz + 33, 8, 6, 3.2f, SIDE_N);
    Shack(cx + 10, cz + 32, 6, 6, 3.0f, SIDE_N);
    Shack(cx - 4, cz - 33, 7, 6, 3.0f, SIDE_S, 1.5f);
    Cont(ZONE_WASTE, cx - 32, cz - 3.5f, PI / 2, CM_FOOTLOCKER, 1);
    Cont(ZONE_WASTE, cx + 33, cz - 2.5f, PI / 2, CM_FOOTLOCKER, 1, 1);
    Cont(ZONE_WASTE, cx - 8, cz + 35, 0, CM_FRIDGE, 1);
    // letto pubblico (nella casa sud-ovest)
    {
        Interactable& bed = AddInter(INT_BED, ZONE_WASTE, V3(cx - 3.5f, g + 0.27f, cz + 34.5f), PI / 2, "Materasso", "bed_town");
        bed.size = V3(2, 0.4f, 1);
    }
    // torre dell'acqua
    {
        float tx = cx + 2, tz = cz - 22;
        for (int i = 0; i < 4; i++) {
            float lx = tx + ((i & 1) ? 2.2f : -2.2f), lz = tz + ((i & 2) ? 2.2f : -2.2f);
            Cyl(V3(lx, g - 0.5f, lz), 0.18f, 10.5f, C_RUST, 6);
        }
        Vector3 tc = V3(tx, g + 12.5f, tz);
        Z->B(tc).Cylinder(MatTRS(tc, V3(0, 0, 0), V3(6.2f, 4.5f, 6.2f)), 16, Col(120, 115, 105));
        Z->B(tc).Cylinder(MatTRS(tc + V3(0, 2.9f, 0), V3(0, 0, 0), V3(6.4f, 1.4f, 6.4f)), 16, C_RUST, true, 0.2f);
        Vis(V3(tx, g + 10.1f, tz), V3(5.5f, 0.2f, 5.5f), C_RUST);
        AddSign(V3(tx, g + 12.6f, tz + 3.12f), 0, 4.0f, 1.0f, "ACQUA", Col(120, 115, 105), Col(40, 60, 90));
        // pompa guasta: la ripara il giocatore (missione "Acqua per Ferrocittà")
        float px = tx + 4.2f, pz = tz + 1.0f;
        Interactable& pump = AddInter(INT_REPAIR, ZONE_WASTE, V3(px, GY(px, pz), pz), PI / 2, "Pompa dell'acqua", "pump");
        pump.skillReq = 20;
        pump.partsReq = 2;
        pump.size = V3(1.2f, 1.8f, 1.2f);
        Coll(V3(px, GY(px, pz) + 0.9f, pz), V3(1.0f, 1.8f, 1.0f), true);
        Cyl(V3(tx + 1.8f, g + 0.2f, tz + 1.0f), 0.08f, 10.0f, C_METAL_D, 5, false);
    }
    // piazza centrale: fuochi, panche, luci
    FireBarrel(cx + 3, cz + 2);
    FireBarrel(cx - 9, cz - 4);
    for (int i = 0; i < 3; i++) {
        float a = i * 2.1f + 0.4f;
        float bx = cx + 3 + sinf(a) * 3.2f, bz = cz + 2 + cosf(a) * 3.2f;
        Z->AddBox(V3(bx, g + 0.25f, bz), V3(2.2f, 0.5f, 0.6f), C_WOOD, true, a + PI / 2);
    }
    // filo di lampadine tra pali
    std::vector<Vector3> poles = {V3(cx - 12, 0, cz + 6), V3(cx - 6, 0, cz - 8), V3(cx + 9, 0, cz - 7), V3(cx + 12, 0, cz + 8), V3(cx - 2, 0, cz + 12)};
    for (auto& p : poles) {
        p.y = GY(p.x, p.z);
        Cyl(p, 0.1f, 5.5f, C_WOOD_D, 5);
    }
    for (size_t i = 0; i < poles.size(); i++) {
        Vector3 a = poles[i] + V3(0, 5.3f, 0), b = poles[(i + 1) % poles.size()] + V3(0, 5.3f, 0);
        int n = 10;
        for (int k = 0; k <= n; k++) {
            float t = (float)k / n;
            Vector3 p = Vector3Lerp(a, b, t) - V3(0, sinf(t * PI) * 0.8f, 0);
            Color bc = (k % 3 == 0) ? Col(255, 220, 140) : (k % 3 == 1) ? Col(255, 120, 90) : Col(150, 220, 255);
            Z->B(p).Sphere(MatTRS(p, V3(0, 0, 0), V3(0.11f, 0.14f, 0.11f)), 3, 5, Glow(bc));
        }
        Light(Vector3Lerp(a, b, 0.5f) - V3(0, 1, 0), Col(255, 200, 140), 12, 0.7f, 0.05f, true);
    }
    // varie
    for (int i = 0; i < 8; i++) {
        float a = gen.range(0, 6.28f), r = gen.range(8, 40);
        float bx = cx + sinf(a) * r, bz = cz + cosf(a) * r;
        if (gen.chance(0.5f)) Crate(bx, bz, gen.range(0.6f, 1.0f));
        else Barrel(bx, bz, CORR[gen.irange(0, 5)]);
    }
    Car(cx + 12, cz + 55, 0.3f, Col(140, 160, 150));
    W.locations.push_back({"Ferrocittà", V3(cx, g, cz), 50, false, V3(cx, g, cz + 58), 1});

    // --- abitanti ---
    {
        Entity& e = NPC(ZONE_WASTE, cx - 19, cz - 9, PI * 0.1f, "Sceriffo Marta Vance", "sheriff", OF_SHERIFF, true, FAC_TOWN);
        e.look.hat = 1;
        e.look.hair = Col(150, 90, 50);
        e.essential = true;
        e.wanderRadius = 2;
    }
    {
        Entity& e = NPC(ZONE_WASTE, cx + 20, cz - 18.6f, 0, "Gus Moretti", "trader", OF_TRADER, false, FAC_TOWN);
        e.vendor = 0;
        e.wanderRadius = 0.5f;
        e.look.hairStyle = 0;
        e.essential = true;
    }
    {
        Entity& e = NPC(ZONE_WASTE, cx + 22, cz + 20.5f, PI, "Dott.ssa Elena Rosi", "doctor", OF_DOCTOR, true, FAC_TOWN);
        e.vendor = 1;
        e.wanderRadius = 1.2f;
        e.essential = true;
    }
    {
        Entity& e = NPC(ZONE_WASTE, cx - 22, cz + 21.6f, PI, "Tano", "barman", OF_BARMAN, false, FAC_TOWN);
        e.vendor = 2;
        e.wanderRadius = 1.2f;
        e.essential = true;
    }
    {
        Entity& e = NPC(ZONE_WASTE, cx + 6, cz + 4, -PI / 2, "Jack Falco, naturalista", "naturalist", OF_LEATHER, false, FAC_TOWN);
        e.look.hat = 1;
        e.essential = true;
        e.wanderRadius = 2.5f;
    }
    {
        Entity& e = NPC(ZONE_WASTE, cx - 7, cz + 29, PI, "Rita", "rita", OF_SETTLER, true, FAC_TOWN);
        e.look.shirt = Col(150, 90, 70);
        e.look.hair = Col(150, 110, 60);
        e.essential = true;
        e.wanderRadius = 2.5f;
    }
    const char* names[] = {"Pino", "Lia", "Bruno", "Carla", "Enzo", "Nina"};
    for (int i = 0; i < 6; i++) {
        float a = gen.range(0, 6.28f), r = gen.range(6, 25);
        Entity& e = NPC(ZONE_WASTE, cx + sinf(a) * r, cz + cosf(a) * r, a, names[i], "settler", OF_SETTLER, i % 2 == 1, FAC_TOWN);
        e.wanderRadius = 10;
        static const Color shirts[] = {Col(120, 100, 80), Col(90, 100, 110), Col(130, 80, 60), Col(100, 110, 80), Col(140, 130, 110), Col(80, 70, 90)};
        e.look.shirt = shirts[i];
        e.look.pants = ColJitter(Col(80, 70, 60), gen, 0.2f);
    }
    for (int i = 0; i < 3; i++) {
        float ga = (i == 0) ? 0 : (i == 1 ? PI / 2 : PI);
        float px = cx + sinf(ga) * 40, pz = cz + cosf(ga) * 40;
        Entity& e = NPC(ZONE_WASTE, px + 3, pz, ga, "Guardia di Ferrocittà", "guard", OF_GUARD, i == 1, FAC_TOWN);
        e.look.hat = 2;
        e.wanderRadius = 2;
    }
}

Interactable& ItemOnGround(int zone, Vector3 pos, int item, int count, const char* tag = "") {
    Interactable& it = AddInter(INT_ITEM, zone, pos, gen.range(0, 6.28f), ITEMS[item].name, tag);
    it.inv.Add(item, count);
    it.size = V3(0.4f, 0.3f, 0.4f);
    return it;
}

void BuildGasStation() {
    float cx = L_GAS.x, cz = L_GAS.z;
    float g = GY(cx, cz);
    // pensilina
    for (int i = 0; i < 4; i++) {
        float px = cx + ((i & 1) ? 6 : -6), pz = cz + ((i & 2) ? 3.5f : -3.5f);
        Cyl(V3(px, g, pz), 0.25f, 5, Col(200, 195, 185), 8);
    }
    Vis(V3(cx, g + 5.3f, cz), V3(15, 0.6f, 9), Col(205, 200, 190));
    Vis(V3(cx, g + 5.0f, cz), V3(15.2f, 0.2f, 9.2f), Col(170, 40, 30));
    Light(V3(cx, g + 4.5f, cz), Col(255, 240, 200), 10, 0.5f, 0.5f, true);
    // pompe
    for (int s = -1; s <= 1; s += 2) {
        Solid(V3(cx + s * 2.5f, g + 0.15f, cz), V3(1.4f, 0.3f, 3.0f), C_CONCRETE);
        Solid(V3(cx + s * 2.5f, g + 1.05f, cz), V3(0.7f, 1.5f, 0.5f), Col(190, 50, 40), true);
        Vis(V3(cx + s * 2.5f, g + 1.55f, cz + 0.26f), V3(0.45f, 0.3f, 0.02f), Glow(Col(200, 200, 150), 0.2f));
    }
    // negozio (porta a nord)
    float sx = cx - 2, sz = cz + 13;
    Wall(sx - 6, sz - 4, sx + 6, sz - 4, g, 3.6f, 0.25f, Col(190, 180, 165), {{6, 1.6f, 0, 2.4f}, {2.5f, 2.6f, 1.0f, 1.6f}, {9.5f, 2.6f, 1.0f, 1.6f}}, WS_PLAIN);
    Wall(sx - 6, sz + 4, sx + 6, sz + 4, g, 3.6f, 0.25f, Col(190, 180, 165), {}, WS_PLAIN);
    Wall(sx - 6, sz - 4, sx - 6, sz + 4, g, 3.6f, 0.25f, Col(190, 180, 165), {{4, 1.6f, 1.0f, 1.3f}}, WS_PLAIN);
    Wall(sx + 6, sz - 4, sx + 6, sz + 4, g, 3.6f, 0.25f, Col(190, 180, 165), {}, WS_PLAIN);
    Vis(V3(sx, g + 3.75f, sz), V3(12.6f, 0.3f, 8.6f), Col(150, 50, 40));
    Slab(sx - 6, sz - 4, sx + 6, sz + 4, g + 0.1f, 0.2f, Col(120, 120, 115));
    Solid(V3(sx + 2, g + 0.55f, sz), V3(3, 0.9f, 0.8f), Col(150, 140, 120));
    for (int i = 0; i < 3; i++) Solid(V3(sx - 3, g + 0.8f, sz - 1.5f + i * 1.6f), V3(2.2f, 1.4f, 0.5f), Col(120, 100, 80));
    AddSign(V3(sx, g + 3.0f, sz - 4.15f), PI, 5, 0.9f, "STAZIONE ATOMO", Col(200, 190, 170), Col(180, 40, 30));
    Cont(ZONE_WASTE, sx + 2, sz + 0.9f, 0, CM_DESK, 1, 1, "", g + 0.1f);
    Cont(ZONE_WASTE, sx + 5.2f, sz + 3.0f, PI, CM_FIRSTAID, 1, 0, "", g + 0.1f);
    Cont(ZONE_WASTE, sx - 5.2f, sz + 3.2f, 0, CM_FRIDGE, 1, 0, "", g + 0.1f);
    // officina
    float ox = cx + 10, oz = cz + 13;
    Wall(ox - 4, oz + 4, ox + 4, oz + 4, g, 4.0f, 0.25f, Col(170, 165, 150));
    Wall(ox + 4, oz - 4, ox + 4, oz + 4, g, 4.0f, 0.25f, Col(170, 165, 150));
    Vis(V3(ox, g + 4.1f, oz), V3(8.5f, 0.25f, 8.5f), C_CONCRETE_D);
    Cont(ZONE_WASTE, ox + 3, oz + 3, 0, CM_TOOLBOX, 1);
    Car(ox, oz, 0.1f, Col(160, 70, 60));
    // insegna con l'atomo
    float ax = cx - 14, az = cz - 9;
    Cyl(V3(ax, g, az), 0.25f, 9, C_METAL, 8);
    Vector3 ac = V3(ax, g + 10.5f, az);
    Z->B(ac).Sphere(MatTRS(ac, V3(0, 0, 0), V3(1.2f, 1.2f, 1.2f)), 6, 8, Glow(Col(255, 80, 60), 0.4f));
    Z->B(ac).Torus(MatTRS(ac, V3(PI / 2, 0, 0), V3(1, 1, 1)), 2.0f, 0.12f, 20, 5, Col(210, 200, 180));
    Z->B(ac).Torus(MatTRS(ac, V3(PI / 2, PI / 3, 0.4f), V3(1, 1, 1)), 2.0f, 0.12f, 20, 5, Col(210, 200, 180));
    Z->B(ac).Torus(MatTRS(ac, V3(PI / 2, -PI / 3, -0.4f), V3(1, 1, 1)), 2.0f, 0.12f, 20, 5, Col(210, 200, 180));
    Light(ac, Col(255, 90, 60), 8, 0.6f, 0.2f, true);
    Car(cx + 4, cz - 7, 1.4f, Col(120, 150, 170));
    Car(cx - 9, cz + 4, 2.8f, Col(180, 170, 120));
    for (int i = 0; i < 5; i++) Tire(cx + gen.range(-12, 14), cz + gen.range(-10, 18));
    Barrel(cx + 15, cz + 5, C_RUST);
    Barrel(cx + 15.7f, cz + 5.6f, Col(60, 90, 60));
    W.locations.push_back({"Stazione Atomo", V3(cx, g, cz), 25, false, V3(cx - 10, g, cz - 14), 0});
    // i sacchi di cibo marcio che attirano i radscarafaggi
    ItemOnGround(ZONE_WASTE, V3(cx, g, cz + 1.5f), I_ROTFOOD, 1, "roach_sack");
    ItemOnGround(ZONE_WASTE, V3(sx - 3.5f, g + 0.2f, sz + 2.6f), I_ROTFOOD, 1, "roach_sack");
    ItemOnGround(ZONE_WASTE, V3(ox - 3.0f, g, oz + 3.0f), I_ROTFOOD, 1, "roach_sack");
    ItemOnGround(ZONE_WASTE, V3(ox + 2.6f, g, oz - 1.5f), I_PARTS, 1);
    // radscarafaggi
    for (int i = 0; i < 7; i++) {
        float px = cx + gen.range(-10, 10), pz = cz + gen.range(-6, 16);
        if (i >= 5) { px = sx + gen.range(-4, 4); pz = sz + gen.range(-2, 2.5f); }
        Spawn(EK_ROACH, ZONE_WASTE, px, pz, gen.range(0, 6.28f)).tag = "gas_roach";
    }
}

void BuildRaiderCamp() {
    float cx = L_RAIDERS.x, cz = L_RAIDERS.z;
    float g = GY(cx, cz);
    // muro di rottami
    float R0 = 30;
    int N = 46;
    for (int i = 0; i < N; i++) {
        float a = (float)i / N * 2 * PI;
        if (fabsf(AngleWrap(a - (-PI / 2))) < 0.2f) continue; // ingresso a ovest
        float px = cx + sinf(a) * R0, pz = cz + cosf(a) * R0;
        float gg = GY(px, pz);
        float h = gen.range(2.5f, 4.2f);
        Color c = ColLerp(CORR[gen.irange(0, 5)], C_RUST, 0.4f);
        Vector3 ctr = V3(px, gg + h * 0.5f - 0.3f, pz);
        Matrix m = MatTRS(V3(0, 0, 0), V3(gen.range(-0.08f, 0.08f), 0, gen.range(-0.1f, 0.1f)), V3(4.3f, h, 0.2f)) * MatrixRotateY(a + gen.range(-0.1f, 0.1f)) * MatrixTranslate(ctr.x, ctr.y, ctr.z);
        VisM(m, c, ctr);
        for (int k = -1; k <= 1; k += 2) {
            Vector3 cp = V3(px, 0, pz) + V3(cosf(a), 0, -sinf(a)) * (k * 1.2f);
            Z->AddCyl(cp.x, cp.z, 0.9f, gg - 1, gg + h);
        }
        if (gen.chance(0.3f)) Car(px + sinf(a) * 3, pz + cosf(a) * 3, a + PI / 2, ColJitter(ColLerp(C_RUST_L, Col(120, 115, 105), 0.5f), gen, 0.2f));
    }
    // totem di coprimozzi e lattine all'ingresso
    for (int s = -1; s <= 1; s += 2) {
        for (int k = 0; k < 2; k++) {
            float px = cx - R0 - 3 - k * 3, pz = cz + s * 6;
            float gg = GY(px, pz);
            Cyl(V3(px, gg, pz), 0.06f, 2.6f, C_WOOD_D, 5, false);
            Vector3 hc = V3(px, gg + 2.4f, pz);
            Z->B(hc).Cylinder(MatTRS(hc, V3(0, 0, PI / 2), V3(0.7f, 0.06f, 0.7f)), 10, Col(180, 180, 175));
            for (int c = 0; c < 3; c++) {
                Vector3 cp = V3(px, gg + 1.2f + c * 0.35f, pz + 0.12f);
                Z->B(cp).Cylinder(MatTRS(cp, V3(0, 0, 0), V3(0.14f, 0.2f, 0.14f)), 6, CORR[(k + c) % 6]);
            }
        }
    }
    AddSign(V3(cx - R0 - 1, g + 3.6f, cz), -PI / 2, 4.5f, 1.0f, "STATE ALLA LARGA", Col(90, 70, 50), Col(200, 40, 30));
    for (int s = -1; s <= 1; s += 2) Cyl(V3(cx - R0 - 1, g, cz + s * 2.4f), 0.12f, 4.2f, C_WOOD_D, 5);
    // baracca del capo (porta a ovest)
    float bx = cx + 10, bz = cz - 2;
    Shack(bx, bz, 11, 9, 3.6f, SIDE_W, 0, Col(110, 70, 50));
    {
        // la tessera di Delta-9 è custodita qui
        Interactable& safe = Cont(ZONE_WASTE, bx + 4, bz - 3, PI, CM_SAFE, 3, 4, "boss_safe");
        safe.keyItem = I_SAFEKEY;
        safe.inv.Clear();
        safe.inv.Add(I_KEYCARD);
        safe.inv.Add(I_CAPS, 90);
        safe.inv.Add(I_STEALTHBOY);
    }
    Interactable& term = AddInter(INT_TERMINAL, ZONE_WASTE, V3(bx + 4.2f, g + 1.0f, bz + 2.8f), -PI / 2, "Terminale dei Teschi", "raider_term");
    term.terminal = TERM_RAIDER;
    term.hackLevel = 1;
    term.size = V3(0.6f, 0.5f, 0.5f);
    Solid(V3(bx + 4.2f, g + 0.55f, bz + 2.8f), V3(0.9f, 0.9f, 1.4f), C_METAL_D);
    // tende
    for (int i = 0; i < 4; i++) {
        float a = gen.range(0, 6.28f);
        float tx = cx + sinf(a) * gen.range(10, 20), tz = cz + cosf(a) * gen.range(10, 20);
        float gg = GY(tx, tz);
        Vector3 tc = V3(tx, gg + 1.1f, tz);
        Color tcol = ColJitter(Col(120, 105, 80), gen, 0.15f);
        VisM(MatTRS(V3(0, 0, 0), V3(0, 0, PI / 4), V3(2.2f, 2.2f, 3.5f)) * MatrixRotateY(a) * MatrixTranslate(tc.x, tc.y - 0.5f, tc.z), tcol, tc);
        Z->AddCyl(tx, tz, 1.4f, gg, gg + 1.5f);
        Interactable& fl = Cont(ZONE_WASTE, tx + 1.8f, tz, a, CM_FOOTLOCKER, 2, gen.chance(0.4f) ? 2 : 0);
        if (i == 2) {
            // qualcuno ha nascosto la chiave di riserva della cassaforte
            fl.lock = 2;
            fl.locked = true;
            fl.inv.Add(I_SAFEKEY);
        }
    }
    // torretta di guardia con scale
    float wx = cx - 14, wz = cz + 12;
    float wg = GY(wx, wz);
    for (int i = 0; i < 4; i++) Cyl(V3(wx + ((i & 1) ? 1.8f : -1.8f), wg - 0.3f, wz + ((i & 2) ? 1.8f : -1.8f)), 0.15f, 4.0f, C_WOOD_D, 6);
    Slab(wx - 2.2f, wz - 2.2f, wx + 2.2f, wz + 2.2f, wg + 3.6f, 0.25f, C_WOOD);
    Wall(wx - 2.2f, wz - 2.2f, wx + 2.2f, wz - 2.2f, wg + 3.6f, 1.0f, 0.1f, C_RUST, {}, WS_RIBBED);
    Wall(wx - 2.2f, wz + 2.2f, wx + 2.2f, wz + 2.2f, wg + 3.6f, 1.0f, 0.1f, C_RUST, {}, WS_RIBBED);
    Wall(wx - 2.2f, wz - 2.2f, wx - 2.2f, wz + 2.2f, wg + 3.6f, 1.0f, 0.1f, C_RUST, {}, WS_RIBBED);
    for (int s = 0; s < 10; s++) {
        float sy = wg + (s + 1) * 0.36f;
        Solid(V3(wx + 2.9f + (9 - s) * 0.5f, sy - 0.18f, wz), V3(0.5f, 0.36f, 1.2f), C_WOOD);
    }
    // fuochi, gabbie e materassi
    FireBarrel(cx - 3, cz + 3);
    FireBarrel(cx + 2, cz - 12);
    FireBarrel(cx - 18, cz - 8);
    for (int i = 0; i < 2; i++) {
        float kx = cx + 3 + i * 3.5f, kz = cz + 15;
        float gg = GY(kx, kz);
        for (int b = 0; b < 8; b++) {
            float a = b / 8.0f * 2 * PI;
            Cyl(V3(kx + sinf(a) * 1.2f, gg, kz + cosf(a) * 1.2f), 0.04f, 2.4f, C_METAL_D, 4, false);
        }
        Z->AddCyl(kx, kz, 1.25f, gg, gg + 2.4f, true);
        Vis(V3(kx, gg + 2.45f, kz), V3(2.6f, 0.1f, 2.6f), C_RUST);
    }
    for (int i = 0; i < 3; i++) Vis(V3(cx - 6 + i * 2.5f, g + 0.12f, cz - 8), V3(1.0f, 0.2f, 2.0f), Col(150, 140, 110));
    for (int i = 0; i < 6; i++) Crate(cx + gen.range(-20, 20), cz + gen.range(-20, 20), gen.range(0.7f, 1.1f));
    Cont(ZONE_WASTE, cx - 8, cz - 15, 0.3f, CM_AMMOBOX, 2);
    Cont(ZONE_WASTE, cx + 15, cz + 12, 0, CM_CRATE, 2);
    W.locations.push_back({"Covo dei Teschi", V3(cx, g, cz), 35, false, V3(cx - 48, g, cz + 2), 0});
    // la banda dei Teschi: territoriali, ma con un lasciapassare ti lasciano in pace
    {
        Entity& e = Raider(bx - 1, bz, "boss");
        e.name = "Teschio";
        e.dlg = "boss";
        e.look.hairStyle = 3;
        e.look.hat = 0;
        e.look.hair = Col(220, 220, 210);
        e.scale = 1.08f;
        e.wanderRadius = 1.5f;
        e.territory = 16;
        e.essential = true;
    }
    {
        Entity& e = Raider(cx - R0 - 2, cz + 3, "bruto");
        e.name = "Bruto, il guardiano";
        e.dlg = "raider_guard";
        e.hostile = false;
        e.look.hat = 3;
        e.scale = 1.1f;
        e.wanderRadius = 1;
        e.essential = true;
    }
    for (int i = 0; i < 7; i++) {
        float a = gen.range(0, 6.28f), r = gen.range(6, 22);
        if (i == 3) {
            Entity& e = Raider(wx, wz);
            e.pos.y = e.home.y = wg + 3.6f;
            e.name = "Vedetta dei Teschi";
            e.detect = 40;
            e.wanderRadius = 0.5f;
        } else Raider(cx + sinf(a) * r, cz + cosf(a) * r).dlg = "raider";
    }
}

void BuildBaseExterior() {
    float x0 = 200, z0 = 170, x1 = 300, z1 = 270;
    float g = GY(250, 220);
    float h = 5.5f;
    Wall(x0, z0, x1, z0, g - 0.5f, h, 0.8f, C_CONCRETE, {}, WS_PLAIN);
    Wall(x0, z1, x1, z1, g - 0.5f, h, 0.8f, C_CONCRETE, {}, WS_PLAIN);
    Wall(x1, z0, x1, z1, g - 0.5f, h, 0.8f, C_CONCRETE, {}, WS_PLAIN);
    Wall(x0, z0, x0, z1, g - 0.5f, h, 0.8f, C_CONCRETE, {{50, 8.0f, 0, 6}}, WS_PLAIN);
    // filo spinato sui muri
    for (float t = 0; t < 100; t += 2) {
        for (int side = 0; side < 4; side++) {
            Vector3 p = side == 0 ? V3(x0 + t, 0, z0) : side == 1 ? V3(x0 + t, 0, z1) : side == 2 ? V3(x1, 0, z0 + t) : V3(x0, 0, z0 + t);
            if (side == 3 && fabsf(z0 + t - 220) < 5) continue;
            Vis(V3(p.x, g + h - 0.5f + 0.15f, p.z), V3(0.08f, 0.3f, 0.08f), C_METAL_D);
        }
    }
    for (int side = 0; side < 4; side++) {
        Vector3 a = side == 0 ? V3(x0, 0, z0) : side == 1 ? V3(x0, 0, z1) : side == 2 ? V3(x1, 0, z0) : V3(x0, 0, z0);
        Vector3 b = side == 0 ? V3(x1, 0, z0) : side == 1 ? V3(x1, 0, z1) : side == 2 ? V3(x1, 0, z1) : V3(x0, 0, z1);
        a.y = b.y = g + h - 0.2f;
        Z->B(a).Cylinder(MatAlignY(a, b, 0.05f), 4, C_METAL_D, false);
        Z->B(a).Cylinder(MatAlignY(a + V3(0, 0.25f, 0), b + V3(0, 0.25f, 0), 0.05f), 4, C_METAL_D, false);
    }
    // cancello
    Interactable& gate = AddInter(INT_GATE, ZONE_WASTE, V3(x0, g, 220), PI / 2, "Cancello della Base Delta-9", "base_gate");
    gate.locked = true;
    gate.lock = 5;
    gate.keyItem = I_KEYCARD;
    gate.size = V3(8, 5, 0.5f);
    gate.collider = Z->AddCollider(V3(x0 - 0.4f, g - 1, 216), V3(x0 + 0.4f, g + 6, 224), true);
    AddSign(V3(x0 - 0.45f, g + 4.6f, 228.5f), -PI / 2, 4.6f, 1.2f, "BASE DELTA-9", Col(70, 80, 60), Col(230, 230, 210));
    AddSign(V3(x0 - 0.45f, g + 3.0f, 210.5f), -PI / 2, 4.2f, 1.0f, "ACCESSO VIETATO", Col(200, 180, 40), Col(30, 30, 30));
    // terminale del cancello
    Solid(V3(x0 - 2.5f, g + 0.7f, 212.5f), V3(0.8f, 1.4f, 0.8f), C_METAL_D, true);
    Interactable& t = AddInter(INT_TERMINAL, ZONE_WASTE, V3(x0 - 2.95f, g + 1.3f, 212.5f), -PI / 2, "Terminale del cancello", "gate_term");
    t.terminal = TERM_GATE;
    t.hackLevel = 2;
    t.size = V3(0.5f, 0.5f, 0.5f);
    // torrette d'angolo
    for (int i = 0; i < 4; i++) {
        float tx = (i & 1) ? x1 - 3 : x0 + 3, tz = (i & 2) ? z1 - 3 : z0 + 3;
        for (int k = 0; k < 4; k++) Cyl(V3(tx + ((k & 1) ? 1.5f : -1.5f), g - 0.3f, tz + ((k & 2) ? 1.5f : -1.5f)), 0.15f, 7.5f, C_METAL, 6);
        Vis(V3(tx, g + 7.3f, tz), V3(4.2f, 0.3f, 4.2f), C_CONCRETE_D);
        Vis(V3(tx, g + 9.6f, tz), V3(4.6f, 0.25f, 4.6f), C_METAL_D);
        for (int k = 0; k < 4; k++) Cyl(V3(tx + ((k & 1) ? 2.0f : -2.0f), g + 7.4f, tz + ((k & 2) ? 2.0f : -2.0f)), 0.06f, 2.1f, C_METAL_D, 4, false);
        Vis(V3(tx, g + 9.2f, tz), V3(0.5f, 0.3f, 0.5f), Glow(Col(255, 60, 40)));
        Light(V3(tx, g + 8.8f, tz), Col(255, 60, 40), 10, 0.7f, 0.0f, true);
    }
    // caserme
    for (int k = 0; k < 2; k++) {
        float bx = 232, bz = k == 0 ? 190 : 250;
        Wall(bx - 9, bz - 4.5f, bx + 9, bz - 4.5f, g, 3.8f, 0.3f, Col(140, 145, 120), k == 0 ? std::vector<Gap>{{9, 1.6f, 0, 2.4f}, {4, 1.6f, 1.2f, 1.0f}, {14, 1.6f, 1.2f, 1.0f}} : std::vector<Gap>{{4, 1.6f, 1.2f, 1.0f}, {14, 1.6f, 1.2f, 1.0f}});
        Wall(bx - 9, bz + 4.5f, bx + 9, bz + 4.5f, g, 3.8f, 0.3f, Col(140, 145, 120), k == 1 ? std::vector<Gap>{{9, 1.6f, 0, 2.4f}} : std::vector<Gap>{});
        Wall(bx - 9, bz - 4.5f, bx - 9, bz + 4.5f, g, 3.8f, 0.3f, Col(140, 145, 120));
        Wall(bx + 9, bz - 4.5f, bx + 9, bz + 4.5f, g, 3.8f, 0.3f, Col(140, 145, 120));
        Vis(V3(bx, g + 4.0f, bz), V3(18.8f, 0.4f, 9.4f), Col(100, 110, 90));
        Slab(bx - 9, bz - 4.5f, bx + 9, bz + 4.5f, g + 0.1f, 0.3f, C_CONCRETE_D);
        for (int b = 0; b < 4; b++) {
            Solid(V3(bx - 6 + b * 4, g + 0.45f, bz + (k == 0 ? 3 : -3)), V3(1.0f, 0.5f, 2.2f), Col(90, 100, 80));
            Cont(ZONE_WASTE, bx - 7 + b * 4, bz + (k == 0 ? 3.7f : -3.7f), k == 0 ? PI : 0, CM_FOOTLOCKER, 2, b == 2 ? 2 : 0, "", g + 0.1f);
        }
        Cont(ZONE_WASTE, bx + 8.3f, bz, -PI / 2, CM_LOCKER, 3, 1, "", g + 0.1f);
        Light(V3(bx, g + 3.3f, bz), Col(230, 230, 210), 10, 0.6f, 0.4f);
        if (k == 0) {
            Solid(V3(bx - 8.3f, g + 0.55f, bz), V3(0.9f, 0.9f, 1.4f), C_METAL_D, true);
            Interactable& st = AddInter(INT_TERMINAL, ZONE_WASTE, V3(bx - 8.3f, g + 1.25f, bz), PI / 2, "Terminale di sicurezza esterno", "sec_term_out");
            st.terminal = TERM_SECURITY;
            st.hackLevel = 1;
            st.size = V3(0.5f, 0.5f, 0.5f);
        }
    }
    // ingresso del bunker
    float ex = 278, ez = 220;
    Solid(V3(ex, g + 2.5f, ez), V3(10, 5, 12), C_CONCRETE_D);
    Vis(V3(ex - 5.05f, g + 1.6f, ez), V3(0.2f, 3.2f, 3.4f), C_METAL_D);
    for (int i = 0; i < 6; i++) Vis(V3(ex - 5.15f, g + 3.5f, ez - 2.5f + i), V3(0.1f, 0.35f, 0.9f), i & 1 ? C_VAULT_YEL : C_BLACK);
    AddSign(V3(ex - 5.2f, g + 4.3f, ez), -PI / 2, 4, 0.8f, "LABORATORI", Col(60, 66, 60), Col(220, 220, 200));
    Light(V3(ex - 6, g + 4.0f, ez), Col(255, 200, 120), 9, 0.6f, 0.3f);
    {
        Interactable& d = AddInter(INT_TRANSITION, ZONE_WASTE, V3(ex - 5.3f, g + 1.2f, ez), -PI / 2, "Entra nei Laboratori Delta-9", "base_entrance");
        d.targetZone = ZONE_BASE;
        d.targetPos = V3(0, 0, -2);
        d.targetYaw = 0;
        d.size = V3(3, 3, 0.4f);
    }
    // sacchi di sabbia, camion, casse
    Sandbags(ex - 9, ez - 6, 4, PI / 2);
    Sandbags(ex - 9, ez + 6, 4, PI / 2);
    Sandbags(x0 + 10, 213, 5, PI / 2);
    Sandbags(x0 + 10, 227, 5, PI / 2);
    {
        float tx = 255, tz = 222;
        Solid(V3(tx, g + 1.2f, tz), V3(2.6f, 1.6f, 7), Col(80, 90, 65));
        Solid(V3(tx, g + 1.4f, tz - 4.5f), V3(2.5f, 2.0f, 2.2f), Col(75, 85, 60));
        Vis(V3(tx, g + 1.9f, tz - 5.65f), V3(2.0f, 0.8f, 0.05f), Col(30, 34, 36));
        for (int i = 0; i < 6; i++) {
            Vector3 wp = V3(tx + ((i & 1) ? 1.3f : -1.3f), g + 0.5f, tz - 4.5f + (i / 2) * 3.5f);
            Z->B(wp).Cylinder(MatTRS(wp, V3(0, 0, PI / 2), V3(1.0f, 0.4f, 1.0f)), 10, Col(28, 28, 28));
        }
        VisM(MatTRS(V3(0, 0, 0), V3(0, 0, 0), V3(2.7f, 0.1f, 7)) * MatrixTranslate(tx, g + 3.2f, tz + 0.2f), Col(90, 95, 70), V3(tx, 0, tz));
    }
    for (int i = 0; i < 8; i++) Crate(260 + gen.range(-8, 8), 185 + gen.range(-8, 8), gen.range(0.7f, 1.2f));
    Cont(ZONE_WASTE, 262, 196, 0, CM_AMMOBOX, 3);
    Cont(ZONE_WASTE, 288, 250, 0.4f, CM_AMMOBOX, 3);
    W.locations.push_back({"Base Delta-9", V3(250, g, 220), 55, false, V3(185, g, 222), 3});
    // sorveglianza: sentinelle di pattuglia e telecamere all'ingresso del bunker
    Entity& r1 = Spawn(EK_ROBOT, ZONE_WASTE, 235, 220, -PI / 2);
    r1.patrol = {V3(225, g, 205), V3(265, g, 205), V3(265, g, 240), V3(225, g, 240)};
    Spawn(EK_ROBOT, ZONE_WASTE, 285, 195, 0).patrol = {V3(285, g, 190), V3(285, g, 255)};
    Spawn(EK_CAMERA, ZONE_WASTE, ex - 8, ez - 9, -PI / 2 - 0.4f);
    Spawn(EK_CAMERA, ZONE_WASTE, ex - 8, ez + 9, -PI / 2 + 0.4f);
}

void BuildFarm() {
    float cx = L_FARM.x, cz = L_FARM.z;
    float g = GY(cx, cz);
    // casa colonica (porta a ovest)
    float hx = cx + 8, hz = cz - 5;
    Wall(hx - 4.5f, hz - 3.5f, hx + 4.5f, hz - 3.5f, g, 3.2f, 0.2f, C_WOOD, {{3, 1.4f, 1.1f, 1.0f}, {6.5f, 1.4f, 1.1f, 1.0f}}, WS_RIBBED);
    Wall(hx - 4.5f, hz + 3.5f, hx + 4.5f, hz + 3.5f, g, 3.2f, 0.2f, C_WOOD, {{4.5f, 1.4f, 1.1f, 1.0f}}, WS_RIBBED);
    Wall(hx - 4.5f, hz - 3.5f, hx - 4.5f, hz + 3.5f, g, 3.2f, 0.2f, C_WOOD, {{3.5f, 1.4f, 0, 2.3f}}, WS_RIBBED);
    Wall(hx + 4.5f, hz - 3.5f, hx + 4.5f, hz + 3.5f, g, 3.2f, 0.2f, C_WOOD, {}, WS_RIBBED);
    Slab(hx - 4.5f, hz - 3.5f, hx + 4.5f, hz + 3.5f, g + 0.15f, 0.3f, C_WOOD_D);
    for (int s = -1; s <= 1; s += 2) {
        Vector3 rc = V3(hx, g + 3.9f, hz + s * 1.9f);
        VisM(MatTRS(V3(0, 0, 0), V3(s * 0.55f, 0, 0), V3(9.8f, 0.15f, 4.6f)) * MatrixTranslate(rc.x, rc.y, rc.z), Col(90, 60, 50), rc);
    }
    Cont(ZONE_WASTE, hx + 3.5f, hz - 2.5f, 0, CM_FRIDGE, 1, 0, "", g + 0.15f);
    Cont(ZONE_WASTE, hx + 3.0f, hz + 2.7f, PI, CM_FOOTLOCKER, 1, 1, "", g + 0.15f);
    Light(V3(hx, g + 2.8f, hz), Col(255, 190, 120), 8, 0.5f, 0.2f);
    // fienile in rovina
    float bx = cx - 12, bz = cz + 10;
    Color red = Col(130, 52, 42);
    Wall(bx - 6, bz - 7, bx + 6, bz - 7, g, 6.0f, 0.25f, red, {{6, 3.5f, 0, 4.0f}}, WS_RUIN);
    Wall(bx - 6, bz + 7, bx + 6, bz + 7, g, 6.0f, 0.25f, red, {}, WS_RUIN);
    Wall(bx - 6, bz - 7, bx - 6, bz + 7, g, 6.0f, 0.25f, red, {{9, 3, 0, 6}}, WS_RUIN);
    Wall(bx + 6, bz - 7, bx + 6, bz + 7, g, 6.0f, 0.25f, red, {}, WS_RUIN);
    for (int i = 0; i < 5; i++) Vis(V3(bx, g + 6.2f, bz - 6 + i * 3), V3(12, 0.25f, 0.25f), C_WOOD_D);
    for (int i = 0; i < 4; i++) Crate(bx + gen.range(-4, 4), bz + gen.range(-5, 5), 1.0f);
    Cont(ZONE_WASTE, bx + 4.5f, bz + 5.5f, 0, CM_TOOLBOX, 1);
    Vis(V3(bx - 2, g + 0.5f, bz + 3), V3(3, 1.0f, 2), Col(170, 150, 80));
    // silo
    Cyl(V3(cx - 22, g - 0.5f, cz - 8), 3, 13, Col(150, 145, 135), 16);
    Vector3 dome = V3(cx - 22, g + 12.5f, cz - 8);
    Z->B(dome).Sphere(MatTRS(dome, V3(0, 0, 0), V3(6.0f, 3.0f, 6.0f)), 6, 16, C_RUST);
    // campo con colture secche e staccionata
    for (int r = 0; r < 8; r++)
        for (int k = 0; k < 14; k++) {
            float px = cx + 12 + k * 1.3f + gen.range(-0.2f, 0.2f), pz = cz + 8 + r * 2.0f;
            float gg = GY(px, pz);
            float hh = gen.range(0.5f, 1.3f);
            Vis(V3(px, gg + hh * 0.5f, pz), V3(0.06f, hh, 0.06f), Col(150, 130, 80));
        }
    for (int k = 0; k <= 10; k++) {
        float px = cx + 10 + k * 2.0f;
        for (float pz : {cz + 6.0f, cz + 24.0f}) {
            float gg = GY(px, pz);
            Vis(V3(px, gg + 0.6f, pz), V3(0.15f, 1.2f, 0.15f), C_WOOD_D);
            if (k < 10 && gen.chance(0.8f)) Vis(V3(px + 1.0f, gg + 0.9f, pz), V3(2.0f, 0.1f, 0.08f), C_WOOD);
        }
    }
    DeadTree(cx + 4, cz + 18, 7);
    Car(cx - 2, cz - 18, 0.7f, Col(110, 120, 90));
    W.locations.push_back({"Fattoria Abbandonata", V3(cx, g, cz), 30, false, V3(cx - 18, g, cz - 20), 0});
    // generatore guasto dietro la casa
    {
        float px = hx + 6.5f, pz = hz;
        Interactable& gen1 = AddInter(INT_REPAIR, ZONE_WASTE, V3(px, GY(px, pz), pz), -PI / 2, "Generatore della fattoria", "generator");
        gen1.skillReq = 35;
        gen1.partsReq = 1;
        gen1.size = V3(1.6f, 1.2f, 1.0f);
        Coll(V3(px, GY(px, pz) + 0.5f, pz), V3(1.0f, 1.0f, 1.7f), true);
    }
    ItemOnGround(ZONE_WASTE, V3(bx + 3.5f, g, bz - 4.5f), I_PARTS, 2);
    for (int i = 0; i < 4; i++) Spawn(EK_MOLERAT, ZONE_WASTE, cx + 12 + gen.range(0, 16), cz + 8 + gen.range(0, 14), gen.range(0, 6.28f));
    {
        // Ruggine, il cane di Rita, si è rifugiato nel fienile
        Entity& d = Spawn(EK_DOG, ZONE_WASTE, bx - 2, bz + 4, PI);
        d.name = "Ruggine";
        d.tag = "ruggine";
        d.dlg = "ruggine";
        d.photoKey = "ruggine";
        d.hostile = false;
        d.wanderRadius = 3;
        d.essential = true;
    }
}

void BuildRadioTower() {
    float cx = L_RADIO.x, cz = L_RADIO.z;
    float g = GY(cx, cz);
    float H = 32;
    for (int i = 0; i < 4; i++) {
        float sx = (i & 1) ? 1 : -1, sz = (i & 2) ? 1 : -1;
        Vector3 a = V3(cx + sx * 3.2f, g - 0.5f, cz + sz * 3.2f), b = V3(cx + sx * 0.5f, g + H, cz + sz * 0.5f);
        Z->B(a).Cylinder(MatAlignY(a, b, 0.14f), 5, Col(160, 70, 50), false);
        Z->AddCyl(a.x, a.z, 0.3f, g - 1, g + 6);
    }
    for (int lvl = 0; lvl < 8; lvl++) {
        float y0 = g + lvl * H / 8, y1 = g + (lvl + 1) * H / 8;
        float w0 = 3.2f - lvl * 2.7f / 8, w1 = 3.2f - (lvl + 1) * 2.7f / 8;
        for (int side = 0; side < 4; side++) {
            float sgn = (side & 1) ? 1.f : -1.f;
            bool alongX = side < 2;
            Vector3 a = alongX ? V3(cx - w0, y0, cz + sgn * w0) : V3(cx + sgn * w0, y0, cz - w0);
            Vector3 b = alongX ? V3(cx + w1, y1, cz + sgn * w1) : V3(cx + sgn * w1, y1, cz + w1);
            Z->B(a).Cylinder(MatAlignY(a, b, 0.05f), 4, lvl % 2 ? Col(200, 190, 180) : Col(160, 70, 50), false);
            Vector3 c = alongX ? V3(cx - w1, y1, cz + sgn * w1) : V3(cx + sgn * w1, y1, cz - w1);
            Vector3 d = alongX ? V3(cx + w1, y1, cz + sgn * w1) : V3(cx + sgn * w1, y1, cz + w1);
            Z->B(a).Cylinder(MatAlignY(c, d, 0.05f), 4, Col(160, 70, 50), false);
        }
    }
    Vector3 top = V3(cx, g + H + 0.6f, cz);
    Z->B(top).Sphere(MatTRS(top, V3(0, 0, 0), V3(0.6f, 0.6f, 0.6f)), 5, 6, Glow(Col(255, 40, 30)));
    Light(top, Col(255, 40, 30), 14, 1.0f, 0.9f);
    // piattaforma a metà torre con il trasmettitore e la scala per salirci
    float py = g + 16;
    Slab(cx - 2.6f, cz - 2.6f, cx + 2.6f, cz + 2.6f, py, 0.2f, C_METAL_D);
    Wall(cx - 2.6f, cz + 2.6f, cx + 2.6f, cz + 2.6f, py, 1.0f, 0.08f, C_RUST, {}, WS_RIBBED);
    Wall(cx - 2.6f, cz - 2.6f, cx - 2.6f, cz + 2.6f, py, 1.0f, 0.08f, C_RUST, {}, WS_RIBBED);
    Wall(cx + 2.6f, cz - 2.6f, cx + 2.6f, cz + 2.6f, py, 1.0f, 0.08f, C_RUST, {}, WS_RIBBED);
    Wall(cx - 2.6f, cz - 2.6f, cx - 0.9f, cz - 2.6f, py, 1.0f, 0.08f, C_RUST, {}, WS_RIBBED);
    Wall(cx + 0.9f, cz - 2.6f, cx + 2.6f, cz - 2.6f, py, 1.0f, 0.08f, C_RUST, {}, WS_RIBBED);
    {
        // scala a pioli sul lato sud
        float lz = cz - 3.0f;
        for (int s = -1; s <= 1; s += 2) {
            Vector3 a = V3(cx + s * 0.4f, g - 0.2f, lz), b = V3(cx + s * 0.4f, py + 1.0f, lz);
            Z->B(a).Cylinder(MatAlignY(a, b, 0.04f), 4, C_METAL_D, false);
        }
        for (float y = g + 0.3f; y < py + 0.8f; y += 0.4f) {
            Vector3 r = V3(cx, y, lz);
            Z->B(r).Cylinder(MatAlignY(r - V3(0.4f, 0, 0), r + V3(0.4f, 0, 0), 0.025f), 4, C_METAL, false);
        }
        Interactable& up = AddInter(INT_LADDER, ZONE_WASTE, V3(cx, g + 1.0f, lz), 0, "Sali sulla torre", "tower_up");
        up.targetPos = V3(cx, py, cz - 1.6f);
        up.targetYaw = 0;
        up.size = V3(1, 2, 1);
        Interactable& dn = AddInter(INT_LADDER, ZONE_WASTE, V3(cx, py + 0.6f, cz - 2.4f), 0, "Scendi dalla torre", "tower_down");
        dn.targetPos = V3(cx, g, lz - 1.4f);
        dn.targetYaw = PI;
        dn.size = V3(1, 1.5f, 1);
    }
    {
        Interactable& tr = AddInter(INT_REPAIR, ZONE_WASTE, V3(cx, py + 0.1f, cz + 1.7f), PI, "Trasmettitore radio", "transmitter");
        tr.skillReq = 15;
        tr.partsReq = 1;
        tr.size = V3(1.2f, 1.3f, 0.6f);
        Coll(V3(cx, py + 0.7f, cz + 1.7f), V3(1.3f, 1.2f, 0.7f), true);
    }
    // baracca dell'esploratrice (porta a est)
    float hx = cx + 8, hz = cz + 6;
    Shack(hx, hz, 5, 4, 2.8f, SIDE_E, 0, Col(120, 116, 108), true);
    Cont(ZONE_WASTE, hx - 1.2f, hz + 0.6f, 0, CM_DESK, 1, 0, "", g + 0.27f);
    ItemOnGround(ZONE_WASTE, V3(hx - 1.6f, g + 1.07f, hz + 0.6f), I_RECORD, 1, "record");
    {
        Interactable& c = Cont(ZONE_WASTE, hx + 1.4f, hz - 1.3f, 0, CM_NONE, 1, 0, "scout_pack", g + 0.27f);
        c.name = "Zaino di Vera";
        c.inv.Clear();
        c.inv.Add(I_SCOUTNOTE);
        c.inv.Add(I_CACHEKEY);
        c.inv.Add(I_WATER, 2);
        c.inv.Add(I_BAIT, 2);
        c.inv.Add(I_CAPS, 25);
        c.size = V3(0.6f, 0.7f, 0.5f);
    }
    {
        Entity& v = NPC(ZONE_WASTE, hx - 0.6f, hz - 1.0f, PI / 2, "Vera, l'esploratrice", "scout", OF_LEATHER, true, FAC_TOWN);
        v.look.hat = 3;
        v.look.hair = Col(60, 40, 30);
        v.wanderRadius = 0;
        v.essential = true;
    }
    Barrel(cx - 5, cz + 4, C_RUST);
    Pole(cx + 14, cz - 6, 7);
    W.locations.push_back({"Torre Radio", V3(cx, g, cz), 18, false, V3(cx + 20, g, cz - 2), 0});
    for (int i = 0; i < 3; i++) Spawn(EK_DOG, ZONE_WASTE, cx - 14 + gen.range(-8, 8), cz + gen.range(-10, 10), gen.range(0, 6.28f));
}

void BuildCrater() {
    float cx = L_CRATER.x, cz = L_CRATER.z;
    float g = GY(cx, cz);
    // pozza radioattiva
    for (int i = 0; i < 3; i++) {
        float px = cx + (i == 0 ? 0 : gen.range(-8, 8)), pz = cz + (i == 0 ? 0 : gen.range(-8, 8));
        float r = i == 0 ? 9 : gen.range(2.5f, 4);
        float gg = GY(px, pz);
        Vector3 c = V3(px, gg + 0.25f, pz);
        Z->B(c).Cylinder(MatTRS(c, V3(0, 0, 0), V3(r * 2, 0.5f, r * 2)), 20, Glow(Col(110, 255, 70), 0.75f));
        Light(c + V3(0, 1.5f, 0), Col(100, 255, 60), r * 2.2f, 0.9f, 0.1f);
        Emit(c, EM_RADGLOW, r * 0.8f, 1.0f);
    }
    W.zones[ZONE_WASTE].rads.push_back({V3(cx, g, cz), 48, 14});
    // rottami contorti
    for (int i = 0; i < 14; i++) {
        float a = gen.range(0, 6.28f), r = gen.range(8, 38);
        float px = cx + sinf(a) * r, pz = cz + cosf(a) * r;
        float gg = GY(px, pz);
        Vector3 c = V3(px, gg + 0.6f, pz);
        VisM(MatTRS(c, V3(gen.range(-0.8f, 0.8f), gen.range(0, 6), gen.range(-0.8f, 0.8f)), V3(gen.range(0.3f, 0.8f), gen.range(1.5f, 4), gen.range(0.2f, 0.5f))), ColJitter(C_RUST, gen, 0.2f), c);
    }
    for (int i = 0; i < 6; i++) {
        float a = gen.range(0, 6.28f), r = gen.range(12, 30);
        float px = cx + sinf(a) * r, pz = cz + cosf(a) * r;
        Barrel(px, pz, Col(200, 180, 40), true);
        W.zones[ZONE_WASTE].rads.push_back({V3(px, GY(px, pz), pz), 5, 4});
    }
    for (int i = 0; i < 10; i++) {
        float a = gen.range(0, 6.28f), r = gen.range(30, 50);
        RockProp(cx + sinf(a) * r, cz + cosf(a) * r, gen.range(1.5f, 4.0f), Col(90, 95, 80));
    }
    // deposito nascosto sul bordo
    float sx = cx + 42, sz = cz - 30;
    float sg = GY(sx, sz);
    Wall(sx - 2.5f, sz - 2.5f, sx + 2.5f, sz - 2.5f, sg - 0.3f, 3.0f, 0.4f, C_CONCRETE_D);
    Wall(sx - 2.5f, sz + 2.5f, sx + 2.5f, sz + 2.5f, sg - 0.3f, 3.0f, 0.4f, C_CONCRETE_D, {{2.5f, 1.4f, 0, 2.3f}});
    Wall(sx - 2.5f, sz - 2.5f, sx - 2.5f, sz + 2.5f, sg - 0.3f, 3.0f, 0.4f, C_CONCRETE_D);
    Wall(sx + 2.5f, sz - 2.5f, sx + 2.5f, sz + 2.5f, sg - 0.3f, 3.0f, 0.4f, C_CONCRETE_D);
    Vis(V3(sx, sg + 2.85f, sz), V3(5.6f, 0.3f, 5.6f), C_CONCRETE);
    Slab(sx - 2.5f, sz - 2.5f, sx + 2.5f, sz + 2.5f, sg + 0.1f, 0.4f, C_CONCRETE_D);
    AddSign(V3(sx + 1.6f, sg + 2.2f, sz + 2.72f), 0, 1.3f, 0.5f, "PROPRIETÀ PRIVATA", Col(180, 170, 150), Col(120, 30, 20));
    {
        Interactable& c = Cont(ZONE_WASTE, sx, sz - 1.5f, 0, CM_FOOTLOCKER, 3, 5, "cache", sg + 0.1f);
        c.keyItem = I_CACHEKEY;
        c.name = "Baule di Vera";
        c.inv.Clear();
        c.inv.Add(I_RADSUIT);
        c.inv.Add(I_STEALTHBOY, 2);
        c.inv.Add(I_RADAWAY, 3);
        c.inv.Add(I_MEDKIT, 3);
        c.inv.Add(I_PARTS, 2);
        c.inv.Add(I_CAPS, 120);
    }
    Cont(ZONE_WASTE, sx + 1.5f, sz + 0.5f, 0, CM_AMMOBOX, 2, 0, "", sg + 0.1f);
    W.locations.push_back({"Cratere Verde", V3(cx, g, cz), 45, false, V3(cx + 20, GY(cx + 20, cz + 55), cz + 55), 4});
    for (int i = 0; i < 5; i++) Spawn(EK_GHOUL, ZONE_WASTE, cx + gen.range(-25, 25), cz + gen.range(-25, 25), gen.range(0, 6.28f));
    {
        Entity& e = Spawn(EK_GHOUL, ZONE_WASTE, cx + 3, cz + 3, 0);
        e.name = "Ghoul luminoso";
        e.photoKey = "glowghoul";
        e.glow = true;
        e.radAura = 6;
    }
}

void BuildCanyon() {
    float cx = L_CANYON.x, cz = L_CANYON.z;
    float g = GY(cx, cz);
    for (int i = 0; i < 6; i++) Brush(cx + gen.range(-12, 12), cz + gen.range(-12, 12), gen.irange(3, 7));
    // nido
    for (int i = 0; i < 12; i++) {
        float a = i / 12.0f * 2 * PI;
        RockProp(cx + sinf(a) * 5, cz + cosf(a) * 5, gen.range(0.7f, 1.4f), Col(100, 90, 80));
    }
    for (int i = 0; i < 20; i++) {
        float a = gen.range(0, 6.28f), r = gen.range(2, 4.5f);
        Vector3 p = V3(cx + sinf(a) * r, GY(cx + sinf(a) * r, cz + cosf(a) * r) + 0.2f, cz + cosf(a) * r);
        Z->B(p).Cylinder(MatAlignY(p, p + V3(gen.range(-1, 1), gen.range(0, 0.4f), gen.range(-1, 1)), 0.05f), 4, C_WOOD_D, false);
    }
    for (int i = 0; i < 3; i++) {
        Vector3 egg = V3(cx + gen.range(-1, 1), g + 0.35f, cz + gen.range(-1, 1));
        Z->B(egg).Sphere(MatTRS(egg, V3(0, 0, 0), V3(0.55f, 0.7f, 0.55f)), 5, 8, Col(200, 190, 160));
    }
    for (int i = 0; i < 15; i++) {
        float a = gen.range(0, 6.28f), r = gen.range(14, 30);
        RockProp(cx + sinf(a) * r, cz + cosf(a) * r, gen.range(2.0f, 5.0f), Col(110, 95, 82));
    }
    {
        // lo zaino che Jack ha perso scappando: è proprio accanto alla bestia addormentata
        Interactable& c = Cont(ZONE_WASTE, cx + 6, cz + 5, 0.6f, CM_NONE, 2, 0, "jack_pack");
        c.name = "Zaino di Jack";
        c.inv.Clear();
        c.inv.Add(I_JOURNAL);
        c.inv.Add(I_MEAT, 2);
        c.inv.Add(I_CAPS, 40);
        c.size = V3(0.6f, 0.7f, 0.5f);
    }
    W.locations.push_back({"Gola dello Squarciatore", V3(cx, g, cz), 25, false, V3(-190, GY(-190, -200), -200), 4});
    Entity& e = Spawn(EK_DEATHCLAW, ZONE_WASTE, cx, cz + 3, 0);
    e.tag = "beast";
    e.name = "Squarciatore";
}

// edificio in rovina su più piani con scala interna
void RuinBuilding(float x0, float z0, float x1, float z1, int floors, Color col, int doorSide, bool hospital = false) {
    float g = GY((x0 + x1) / 2, (z0 + z1) / 2);
    float fh = 3.6f;
    float w = x1 - x0, d = z1 - z0;
    // scala lungo il muro ovest
    float stX = x0 + 1.3f, stZ0 = z0 + 1.5f;
    int steps = 10;
    float stepD = 0.55f;
    for (int f = 0; f < floors; f++) {
        float y = g + f * fh;
        int style = hospital ? WS_PLAIN : (f == floors - 1 ? WS_RUIN : WS_BRICK);
        auto Win = [&](float len, int side) {
            std::vector<Gap> gaps;
            int nw = (int)(len / 4.5f);
            for (int k = 0; k < nw; k++) {
                float p = (k + 0.5f) * len / nw;
                if (f == 0 && side == doorSide && k == nw / 2) gaps.push_back({p, 2.0f, 0, 2.5f});
                else if (!(f == 0 && side == doorSide && abs(k - nw / 2) == 0)) gaps.push_back({p, 1.4f, 1.0f, 1.4f});
            }
            return gaps;
        };
        Color c = ColJitter(col, gen, 0.04f);
        Wall(x0, z0, x1, z0, y - (f == 0 ? 0.4f : 0), fh + (f == 0 ? 0.4f : 0), 0.3f, c, Win(w, SIDE_N), style);
        Wall(x0, z1, x1, z1, y - (f == 0 ? 0.4f : 0), fh + (f == 0 ? 0.4f : 0), 0.3f, c, Win(w, SIDE_S), style);
        Wall(x0, z0, x0, z1, y - (f == 0 ? 0.4f : 0), fh + (f == 0 ? 0.4f : 0), 0.3f, c, Win(d, SIDE_W), style);
        Wall(x1, z0, x1, z1, y - (f == 0 ? 0.4f : 0), fh + (f == 0 ? 0.4f : 0), 0.3f, c, Win(d, SIDE_E), style);
        // pavimento: lastre a tessere con buchi
        float tile = 4.0f;
        for (float tx = x0; tx < x1 - 0.01f; tx += tile)
            for (float tz = z0; tz < z1 - 0.01f; tz += tile) {
                float ax = tx, bx = std::min(tx + tile, x1), az = tz, bz = std::min(tz + tile, z1);
                bool stairHole = (f > 0) && ax < stX + 1.0f && bz > stZ0 - 0.5f && az < stZ0 + steps * stepD + 1.0f;
                if (stairHole) continue;
                if (f > 0 && !hospital && gen.chance(0.22f)) continue;
                Slab(ax, az, bx, bz, y + (f == 0 ? 0.15f : 0.0f), f == 0 ? 0.3f : 0.3f, f == 0 ? Col(105, 100, 95) : ColShade(col, 0.8f), true);
            }
        if (f + 1 < floors) {
            for (int s = 0; s < steps; s++) {
                float sy = y + (s + 1) * fh / steps;
                Solid(V3(stX, sy - fh / steps * 0.5f, stZ0 + s * stepD + stepD * 0.5f), V3(1.6f, fh / steps, stepD), C_CONCRETE_D);
            }
        }
        // macerie e arredi
        int nr = (int)(w * d / 60);
        for (int k = 0; k < nr; k++) {
            float rx = gen.range(x0 + 3, x1 - 1.5f), rz = gen.range(z0 + 1.5f, z1 - 1.5f);
            if (f == 0) {
                if (gen.chance(0.5f)) RockProp(rx, rz, gen.range(0.6f, 1.6f), Col(120, 115, 105));
                else Crate(rx, rz, gen.range(0.6f, 1.0f), y + 0.15f);
            }
        }
    }
    if (!hospital) {
        // muro crollato: cumulo di macerie davanti
        for (int k = 0; k < 4; k++) RockProp(gen.range(x0, x1), (doorSide == SIDE_S ? z0 - 1.5f : z1 + 1.5f) + gen.range(-0.5f, 0.5f), gen.range(1.0f, 2.2f), Col(125, 115, 105));
    }
}

void BuildRuins() {
    // strade già nel terreno; edifici negli isolati
    RuinBuilding(4, -196, 32, -170, 2, C_BRICK, SIDE_S);
    Cont(ZONE_WASTE, 28, -175, 0, CM_DESK, 2);
    Cont(ZONE_WASTE, 8, -190, 0, CM_TRASH, 1);
    // ospedale
    RuinBuilding(58, -198, 100, -170, 2, Col(196, 192, 182), SIDE_S, true);
    {
        float g = GY(79, -184);
        AddSign(V3(79, g + 6.4f, -169.75f), 0, 7, 1.4f, "OSPEDALE", Col(200, 196, 186), Col(160, 30, 30));
        AddSign(V3(79, g + 3.5f, -169.75f), 0, 1.6f, 1.6f, "+", Col(200, 196, 186), Col(200, 30, 30));
        AddSign(V3(79, g + 6.4f, -198.2f), PI, 7, 1.4f, "OSPEDALE", Col(200, 196, 186), Col(160, 30, 30));
        // tramezzi
        Wall(58, -184, 74, -184, g, 3.6f, 0.2f, Col(190, 186, 176), {{8, 1.6f, 0, 2.3f}});
        Wall(84, -184, 100, -184, g, 3.6f, 0.2f, Col(190, 186, 176), {{8, 1.6f, 0, 2.3f}});
        for (int i = 0; i < 4; i++) {
            Solid(V3(62 + i * 3.5f, g + 0.6f, -194), V3(1.0f, 0.5f, 2.1f), Col(200, 200, 200));
            Solid(V3(86 + i * 3.5f, g + 0.6f, -194), V3(1.0f, 0.5f, 2.1f), Col(200, 200, 200));
        }
        Interactable& c = Cont(ZONE_WASTE, 98.8f, -196, -PI / 2, CM_LOCKER, 2, 2, "meds", g + 0.15f);
        c.name = "Armadietto medico";
        c.inv.Add(I_MEDSUPPLIES);
        c.inv.Add(I_MEDKIT, 2);
        Cont(ZONE_WASTE, 59.2f, -196, PI / 2, CM_FIRSTAID, 2, 0, "", g + 0.15f);
        Cont(ZONE_WASTE, 70, -172, PI, CM_DESK, 2, 0, "", g + 0.15f);
        Cont(ZONE_WASTE, 90, -183, 0, CM_FIRSTAID, 2, 0, "", g + 3.6f);
        Light(V3(79, g + 3.0f, -190), Col(200, 255, 200), 14, 0.5f, 0.6f);
        for (int i = 0; i < 4; i++) Spawn(EK_GHOUL, ZONE_WASTE, 62 + i * 10, -190 + (i % 2) * 10, gen.range(0, 6.28f));
    }
    RuinBuilding(118, -196, 146, -168, 2, Col(150, 140, 120), SIDE_S);
    Cont(ZONE_WASTE, 140, -173, 0, CM_SAFE, 2, 3);
    RuinBuilding(4, -254, 34, -214, 3, C_BRICK, SIDE_N);
    Cont(ZONE_WASTE, 30, -220, 0, CM_LOCKER, 2, 1);
    Cont(ZONE_WASTE, 25, -250, 0, CM_DESK, 2, 0, "", GY(25, -250) + 7.2f);
    RuinBuilding(118, -254, 146, -216, 2, Col(160, 150, 130), SIDE_W);
    Cont(ZONE_WASTE, 140, -250, 0, CM_FRIDGE, 1);
    RuinBuilding(48, -306, 102, -270, 3, Col(140, 136, 128), SIDE_N);
    Cont(ZONE_WASTE, 98, -300, 0, CM_SAFE, 3, 4);
    Cont(ZONE_WASTE, 60, -290, 0, CM_DESK, 2, 0, "", GY(60, -290) + 3.6f);
    RuinBuilding(120, -305, 145, -272, 1, Col(120, 110, 100), SIDE_N);
    for (int k = 0; k < 14; k++) RockProp(gen.range(4, 34), gen.range(-306, -272), gen.range(1.0f, 3.0f), Col(125, 118, 108));
    // scheletro del grattacielo
    {
        float sx0 = 86, sz0 = -254, sx1 = 104, sz1 = -236;
        float g = GY(95, -245);
        int fl = 11;
        for (int i = 0; i < 4; i++)
            for (int k = 0; k < 4; k++) {
                float px = sx0 + i * (sx1 - sx0) / 3, pz = sz0 + k * (sz1 - sz0) / 3;
                int hgt = fl - (gen.chance(0.4f) ? gen.irange(1, 5) : 0);
                Solid(V3(px, g + hgt * 1.8f, pz), V3(0.6f, hgt * 3.6f, 0.6f), C_CONCRETE_D);
            }
        for (int f = 1; f < fl; f++) {
            float y = g + f * 3.6f;
            for (int i = 0; i < 4; i++) {
                if (gen.chance(0.25f)) continue;
                float pz = sz0 + i * (sz1 - sz0) / 3;
                Vis(V3((sx0 + sx1) / 2, y, pz), V3(sx1 - sx0, 0.5f, 0.4f), C_RUST);
                float px = sx0 + i * (sx1 - sx0) / 3;
                if (!gen.chance(0.25f)) Vis(V3(px, y, (sz0 + sz1) / 2), V3(0.4f, 0.5f, sz1 - sz0), C_RUST);
            }
            if (f < 3) Slab(sx0, sz0, sx1, sz1, y + 0.25f, 0.25f, C_CONCRETE_D);
        }
    }
    // accampamento dei mutanti in piazza
    {
        float mx = 62, mz = -234;
        float g = GY(mx, mz);
        FireBarrel(mx, mz);
        for (int i = 0; i < 3; i++) Vis(V3(mx - 5 + i * 3, g + 0.15f, mz + 6), V3(1.2f, 0.3f, 2.2f), Col(140, 120, 100));
        for (int i = 0; i < 6; i++) Barrel(mx + gen.range(-12, 12), mz + gen.range(-10, 10), CORR[gen.irange(0, 5)]);
        Brush(mx + 4, mz - 4, 6);
        Cont(ZONE_WASTE, mx + 8, mz + 5, 0, CM_CRATE, 3);
        Cont(ZONE_WASTE, mx - 9, mz - 6, 0, CM_AMMOBOX, 3);
        for (int i = 0; i < 4; i++) Spawn(EK_MUTANT, ZONE_WASTE, mx + gen.range(-8, 8), mz + gen.range(-8, 8), gen.range(0, 6.28f));
        Entity& b = Spawn(EK_MUTANT, ZONE_WASTE, mx, mz - 3, 0);
        b.name = "Grande Mutante";
        b.photoKey = "bigmutant";
        b.scale = 1.55f;
        b.wanderRadius = 2;
    }
    // auto, pali e lampioni lungo le strade
    for (int i = 0; i < 12; i++) {
        bool ns = gen.chance(0.5f);
        float x = ns ? (gen.chance(0.5f) ? 40.f : 110.f) + gen.range(-2.5f, 2.5f) : gen.range(5, 145);
        float z = ns ? gen.range(-305, -165) : (gen.chance(0.5f) ? -205.f : -262.f) + gen.range(-2.5f, 2.5f);
        Car(x, z, (ns ? 0 : PI / 2) + gen.range(-0.4f, 0.4f), ColJitter(CORR[gen.irange(0, 5)], gen, 0.2f));
    }
    for (int i = 0; i < 10; i++) {
        float z = -165 - i * 15;
        float gx = 34.5f, gg = GY(gx, z);
        float hh = gen.chance(0.3f) ? 2.5f : 6.0f;
        Cyl(V3(gx, gg, z), 0.12f, hh, C_METAL_D, 6);
        if (hh > 3) Vis(V3(gx + 0.8f, gg + hh, z), V3(1.6f, 0.15f, 0.3f), C_METAL_D);
    }
    for (int i = 0; i < 4; i++) Spawn(EK_GHOUL, ZONE_WASTE, gen.range(10, 140), gen.range(-300, -170), gen.range(0, 6.28f));
    W.locations.push_back({"Rovine di Nuova Speranza", V3(L_RUINS.x, GY(75, -235), L_RUINS.z), 70, false, V3(40, GY(40, -150), -150), 5});
}

bool NearPOI(float x, float z, float extra) {
    const Vector3 pois[] = {L_VAULT, L_FARM, L_GAS, L_TOWN, L_RADIO, L_RAIDERS, L_CANYON};
    const float radii[] = {30, 38, 32, 58, 22, 40, 22};
    for (int i = 0; i < 7; i++)
        if (DistXZ(V3(x, 0, z), pois[i]) < radii[i] + extra) return true;
    if (DistXZ(V3(x, 0, z), L_CRATER) < 52 + extra) return true;
    if (x > 190 - extra && x < 310 + extra && z > 160 - extra && z < 280 + extra) return true;
    if (x > -6 - extra && x < 156 + extra && z > -318 - extra && z < -152 + extra) return true;
    return false;
}

float RoadDist(float x, float z) {
    float best = 1e9f;
    for (auto& r : W.roads)
        for (size_t k = 0; k + 1 < r.size(); k++) best = std::min(best, DistSeg(x, z, r[k].x, r[k].z, r[k + 1].x, r[k + 1].z, nullptr));
    return best;
}

void BuildWasteland() {
    // alberi morti e rocce sparse
    for (int i = 0; i < 260; i++) {
        float x = gen.range(-WORLD_HALF + 30, WORLD_HALF - 30), z = gen.range(-WORLD_HALF + 30, WORLD_HALF - 30);
        if (NearPOI(x, z, 4) || RoadDist(x, z) < 7) continue;
        float k = gen.f();
        if (k < 0.3f) DeadTree(x, z, gen.range(4, 9));
        else if (k < 0.85f) RockProp(x, z, gen.range(0.8f, 4.5f));
        else if (k < 0.92f) Tire(x, z);
        else Barrel(x, z, ColJitter(C_RUST, gen, 0.2f));
    }
    // cespugli secchi
    for (int i = 0; i < 400; i++) {
        float x = gen.range(-WORLD_HALF + 30, WORLD_HALF - 30), z = gen.range(-WORLD_HALF + 30, WORLD_HALF - 30);
        if (NearPOI(x, z, 0) || RoadDist(x, z) < 5) continue;
        float g = GY(x, z);
        Vector3 c = V3(x, g + 0.2f, z);
        Z->B(c).Rock(MatTRS(c, V3(0, gen.range(0, 6), 0), V3(gen.range(0.6f, 1.4f), gen.range(0.4f, 0.8f), gen.range(0.6f, 1.4f))), gen, ColJitter(Col(110, 100, 62), gen, 0.15f));
    }
    // auto abbandonate e pali lungo le strade
    for (auto& r : W.roads) {
        for (size_t k = 0; k + 1 < r.size(); k++) {
            Vector3 a = r[k], b = r[k + 1];
            float len = DistXZ(a, b);
            for (float t = 6; t < len - 6; t += 22) {
                Vector3 p = Vector3Lerp(a, b, t / len);
                if (NearPOI(p.x, p.z, -6)) continue;
                Vector3 dir = Vector3Normalize(V3(b.x - a.x, 0, b.z - a.z));
                Vector3 side = V3(dir.z, 0, -dir.x);
                if (gen.chance(0.25f)) {
                    float off = gen.range(-3.5f, 3.5f);
                    Car(p.x + side.x * off, p.z + side.z * off, atan2f(dir.x, dir.z) + gen.range(-0.6f, 0.6f), ColJitter(CORR[gen.irange(0, 5)], gen, 0.25f));
                }
                if (gen.chance(0.7f)) {
                    Vector3 pp = p + side * 6.5f;
                    if (!NearPOI(pp.x, pp.z, 0)) {
                        if (gen.chance(0.15f)) {
                            float g = GY(pp.x, pp.z);
                            Vector3 c = V3(pp.x, g + 0.2f, pp.z);
                            Z->B(c).Cylinder(MatAlignY(c, c + V3(dir.x * 7.5f, 0.3f, dir.z * 7.5f), 0.13f), 6, C_WOOD_D);
                        } else Pole(pp.x, pp.z);
                    }
                }
            }
        }
    }
    // cartelli stradali
    {
        float g = GY(-16, 74);
        Cyl(V3(-16, g, 74), 0.08f, 3, C_METAL, 6);
        AddSign(V3(-16, g + 3.1f, 74.05f), PI * 0.2f, 2.4f, 0.6f, "FERROCITTÀ >", Col(40, 90, 50), Col(230, 230, 220));
    }
    {
        float g = GY(180, 214);
        Cyl(V3(180, g, 214), 0.08f, 3, C_METAL, 6);
        AddSign(V3(180, g + 3.1f, 214.05f), -PI / 2, 2.6f, 0.6f, "DELTA-9 >", Col(40, 90, 50), Col(230, 230, 220));
    }
    // creature e predoni vaganti
    auto Group = [&](EntKind k, float x, float z, int n) {
        for (int i = 0; i < n; i++) Spawn(k, ZONE_WASTE, x + gen.range(-6, 6), z + gen.range(-6, 6), gen.range(0, 6.28f)).wanderRadius = 14;
    };
    Group(EK_ROACH, -60, 125, 3);
    Group(EK_ROACH, 150, 80, 4);
    Group(EK_ROACH, -200, 60, 3);
    Group(EK_ROACH, 120, -120, 3);
    Group(EK_MOLERAT, 100, -110, 2);
    Group(EK_MOLERAT, -30, -150, 3);
    Group(EK_MOLERAT, -230, 140, 2);
    Group(EK_DOG, 180, 120, 3);
    Group(EK_DOG, -80, -60, 2);
    Group(EK_DOG, 120, 290, 3);
    Group(EK_GHOUL, 40, -140, 2);
    // piccoli accampamenti di Teschi lungo le strade
    for (Vector3 p : {V3(130, 0, -30), V3(134, 0, -26), V3(128, 0, -24), V3(-118, 0, 60), V3(-122, 0, 64), V3(170, 0, 140), V3(174, 0, 142)}) {
        Entity& e = Raider(p.x, p.z);
        e.dlg = "raider";
        e.territory = 24;
    }
}

// ---------------------------------------------------------------------------
//  Interni: Rifugio 33
// ---------------------------------------------------------------------------
void BuildVault() {
    Z = &W.zones[ZONE_VAULT];
    Z->name = "Rifugio 33";
    Z->outdoor = false;
    Z->floorY = 0;
    Z->bmin = V3(-32, -2, -38);
    Z->bmax = V3(26, 12, 44);
    Z->ambSky = V3(0.36f, 0.39f, 0.44f);
    Z->ambGround = V3(0.22f, 0.23f, 0.26f);
    Z->fog = V3(0.03f, 0.04f, 0.05f);
    Z->fogDensity = 0.022f;
    Color wall = C_VAULT_WALL, floor = C_VAULT_FLOOR, ceil = Col(80, 86, 92);
    // galleria d'uscita
    Room(-4, -36, 4, -24, 7, WS_PLAIN, Col(90, 84, 76), Col(80, 74, 66), Col(70, 66, 60), {}, 1 << SIDE_S, Col(255, 230, 190), 6);
    for (int i = 0; i < 8; i++) {
        float rx = gen.chance(0.5f) ? gen.range(-3.5f, -2.5f) : gen.range(2.5f, 3.5f);
        Vector3 c = V3(rx, gen.range(0, 5), gen.range(-35, -25));
        Z->B(c).Rock(MatTRS(c, V3(0, gen.range(0, 6), 0), V3(2, 2, 2)), gen, Col(95, 88, 80));
    }
    Vis(V3(0, 3.5f, -35.8f), V3(7.6f, 7, 0.2f), Glow(Col(255, 240, 210), 0.85f));
    {
        Interactable& t = AddInter(INT_TRANSITION, ZONE_VAULT, V3(0, 1, -34.5f), PI, "Esci nelle Terre Bruciate", "vault_exit");
        t.targetZone = ZONE_WASTE;
        t.targetPos = V3(L_VAULT.x, 0, L_VAULT.z - 7);
        t.targetYaw = PI;
        t.size = V3(6, 6, 1);
    }
    // sala della porta
    Room(-8, -24, 8, -12, 7, WS_VAULT, wall, floor, ceil, {{SIDE_N, 8, 6.6f, 6.6f}, {SIDE_S, 8, 4, 3.5f}});
    {
        Interactable& d = AddInter(INT_VAULTDOOR, ZONE_VAULT, V3(0, 3.3f, -24), 0, "Porta del Rifugio", "vault_door_in");
        d.size = V3(6.6f, 6.6f, 1);
        d.collider = Z->AddCollider(V3(-3.4f, 0, -24.6f), V3(3.4f, 6.6f, -23.4f), true);
    }
    Solid(V3(5.5f, 0.6f, -21.5f), V3(2.0f, 1.2f, 1.0f), C_METAL_D, true);
    Vis(V3(5.5f, 1.25f, -21.2f), V3(1.6f, 0.4f, 0.5f), Glow(Col(80, 255, 140), 0.3f));
    Vis(V3(-5.5f, 3.5f, -23.75f), V3(1.6f, 0.4f, 0.2f), Glow(Col(255, 60, 40), 0.5f));
    AddSign(V3(0, 5.8f, -12.4f), PI, 5, 1.0f, "USCITA PRINCIPALE", C_VAULT_PANEL, C_VAULT_YEL);
    // corridoio nord
    Room(-2, -12, 2, 0, 3.5f, WS_VAULT, wall, floor, ceil, {}, (1 << SIDE_N) | (1 << SIDE_S), Col(205, 220, 255), 6);
    // atrio
    Room(-14, 0, 14, 16, 7, WS_VAULT, wall, floor, ceil, {{SIDE_N, 14, 4, 3.5f}, {SIDE_S, 14, 4, 3.5f}, {SIDE_W, 8, 3, 3}, {SIDE_E, 8, 3, 3}}, 0, Col(205, 220, 255), 9);
    for (int s = -1; s <= 1; s += 2) {
        Vis(V3(s * 12.5f, 3.6f, 8), V3(3, 0.25f, 16), C_METAL_D);
        Vis(V3(s * 11.0f, 4.2f, 8), V3(0.1f, 1.0f, 16), C_METAL);
    }
    AddSign(V3(0, 5.2f, 0.25f), 0, 7, 1.6f, "RIFUGIO 33", C_VAULT_PANEL, C_VAULT_YEL);
    AddSign(V3(-13.75f, 4.0f, 8), PI / 2, 4.5f, 0.8f, "< REATTORE", C_VAULT_PANEL, C_VAULT_YEL);
    AddSign(V3(13.75f, 4.0f, 8), -PI / 2, 5.2f, 0.8f, "SOVRINTENDENTE >", C_VAULT_PANEL, C_VAULT_YEL);
    for (int i = 0; i < 3; i++) {
        float tx = -6 + i * 6;
        Cyl(V3(tx, 0, 10), 0.9f, 0.85f, Col(190, 190, 185), 12);
        for (int k = 0; k < 4; k++) {
            float a = k * PI / 2 + 0.4f;
            Cyl(V3(tx + sinf(a) * 1.5f, 0, 10 + cosf(a) * 1.5f), 0.25f, 0.5f, C_VAULT_PANEL, 8);
        }
    }
    for (int s = -1; s <= 1; s += 2) {
        Solid(V3(s * 9, 0.5f, 3), V3(2.5f, 1.0f, 1.2f), Col(110, 100, 90));
        Vector3 pc = V3(s * 9, 1.4f, 3);
        Z->B(pc).Sphere(MatTRS(pc, V3(0, 0, 0), V3(1.6f, 1.0f, 0.9f)), 5, 8, Col(70, 100, 60));
    }
    // ufficio del Sovrintendente
    Room(14, 2, 24, 12, 3.5f, WS_VAULT, Col(130, 120, 110), Col(90, 70, 55), ceil, {}, 1 << SIDE_W, Col(255, 230, 200), 6);
    Solid(V3(20, 0.45f, 7), V3(1.2f, 0.9f, 3.2f), C_WOOD);
    Solid(V3(23.5f, 1.2f, 7), V3(0.6f, 2.4f, 4.0f), C_WOOD_D);
    {
        Interactable& t = AddInter(INT_TERMINAL, ZONE_VAULT, V3(20, 1.15f, 5.9f), -PI / 2, "Terminale del Sovrintendente", "overseer_term");
        t.terminal = TERM_OVERSEER;
        t.hackLevel = 0;
        t.size = V3(0.5f, 0.5f, 0.5f);
    }
    Cont(ZONE_VAULT, 23.2f, 3.0f, -PI / 2, CM_SAFE, 2, 3);
    // reattore
    Room(-30, 0, -14, 16, 8, WS_VAULT, Col(110, 112, 115), Col(70, 72, 76), Col(60, 62, 66), {}, 1 << SIDE_E, Col(255, 200, 150), 7, 0.6f);
    {
        Interactable& r = AddInter(INT_REACTOR, ZONE_VAULT, V3(-22, 0, 8), 0, "Reattore a fusione", "reactor");
        r.size = V3(4, 6, 4);
        Z->AddCyl(-22, 8, 2.8f, 0, 8);
    }
    for (int i = 0; i < 4; i++) {
        float a = i * PI / 2 + PI / 4;
        Vector3 p0 = V3(-22 + sinf(a) * 2.5f, 6.5f, 8 + cosf(a) * 2.5f), p1 = V3(-22 + sinf(a) * 7.5f, 7.5f, 8 + cosf(a) * 7.5f);
        Z->B(p0).Cylinder(MatAlignY(p0, p1, 0.25f), 8, C_METAL);
    }
    Solid(V3(-28.5f, 0.6f, 3), V3(1.0f, 1.2f, 3.0f), C_METAL_D, true);
    Vis(V3(-28.0f, 1.25f, 3), V3(0.1f, 0.5f, 2.6f), Glow(Col(255, 120, 60), 0.4f));
    AddSign(V3(-22, 6.2f, 0.25f), 0, 5, 0.9f, "PERICOLO: RADIAZIONI", Col(220, 180, 40), Col(30, 30, 30));
    // corridoio sud
    Room(-2, 16, 2, 30, 3.5f, WS_VAULT, wall, floor, ceil, {{SIDE_W, 7, 2.5f, 2.8f}, {SIDE_E, 7, 2.5f, 2.8f}}, (1 << SIDE_N) | (1 << SIDE_S), Col(205, 220, 255), 6);
    // infermeria
    Room(2, 18, 12, 28, 3.5f, WS_VAULT, Col(200, 205, 210), Col(150, 155, 160), ceil, {}, 1 << SIDE_W, Col(220, 240, 255), 6);
    for (int i = 0; i < 2; i++) Solid(V3(9.5f, 0.45f, 20.5f + i * 4), V3(2.2f, 0.6f, 1.0f), Col(220, 220, 220));
    Cont(ZONE_VAULT, 11.8f, 26, -PI / 2, CM_FIRSTAID, 1);
    // mensa
    Room(-12, 18, -2, 28, 3.5f, WS_VAULT, wall, floor, ceil, {}, 1 << SIDE_E, Col(255, 235, 200), 6);
    for (int i = 0; i < 2; i++) {
        Solid(V3(-7, 0.4f, 21 + i * 4), V3(4, 0.8f, 1.2f), Col(180, 180, 175));
        for (int s = -1; s <= 1; s += 2)
            for (int k = 0; k < 3; k++) Cyl(V3(-8.5f + k * 1.5f, 0, 21 + i * 4 + s * 1.0f), 0.22f, 0.45f, C_VAULT_PANEL, 8, false);
    }
    Solid(V3(-11.4f, 0.55f, 23), V3(0.8f, 1.1f, 6), C_METAL);
    Cont(ZONE_VAULT, -11.4f, 19.2f, PI / 2, CM_FRIDGE, 1);
    // alloggi
    Room(-12, 30, 12, 42, 3.5f, WS_VAULT, wall, floor, ceil, {{SIDE_N, 12, 4, 3.0f}}, 0, Col(205, 220, 255), 7);
    for (int i = 0; i < 4; i++)
        for (int s = -1; s <= 1; s += 2) {
            float bx = s * (4 + i * 2.2f);
            Solid(V3(bx, 0.35f, 40.4f), V3(1.1f, 0.5f, 2.2f), C_VAULT_PANEL);
            Vis(V3(bx, 0.65f, 41.1f), V3(0.8f, 0.15f, 0.5f), Col(220, 220, 215));
        }
    {
        Interactable& b = AddInter(INT_BED, ZONE_VAULT, V3(4, 0.6f, 40.4f), 0, "Il tuo letto", "bed_vault");
        b.size = V3(1.1f, 0.5f, 2.2f);
    }
    for (int i = 0; i < 5; i++) Cont(ZONE_VAULT, -10 + i * 1.2f, 30.6f, PI, CM_LOCKER, 1, i == 3 ? 1 : 0);
    Cont(ZONE_VAULT, 4, 38.4f, 0, CM_FOOTLOCKER, 1);
    W.zones[ZONE_VAULT].Finalize();

    // abitanti del Rifugio
    {
        Entity& e = NPC(ZONE_VAULT, 21.5f, 7, -PI / 2, "Sovrintendente Ferri", "overseer", OF_VAULT, false, FAC_VAULT);
        e.look.hair = Col(170, 170, 165);
        e.look.hairStyle = 1;
        e.wanderRadius = 0.6f;
        e.essential = true;
    }
    {
        Entity& e = NPC(ZONE_VAULT, 4, -18, PI, "Agente Russo", "vault_guard", OF_GUARD, false, FAC_VAULT);
        e.look.hat = 2;
        e.wanderRadius = 1.5f;
        e.essential = true;
    }
    {
        Entity& e = NPC(ZONE_VAULT, 7, 23, -PI / 2, "Dottor Bianchi", "vault_doc", OF_DOCTOR, false, FAC_VAULT);
        e.wanderRadius = 2;
        e.essential = true;
    }
    const char* names[] = {"Anna", "Marco", "Lucia", "Paolo"};
    Vector3 pos[] = {V3(-7, 0, 23), V3(-4, 0, 6), V3(5, 0, 12), V3(-24, 0, 14)};
    for (int i = 0; i < 4; i++) {
        Entity& e = NPC(ZONE_VAULT, pos[i].x, pos[i].z, gen.range(0, 6), names[i], "vault_dweller", OF_VAULT, i % 2 == 0, FAC_VAULT);
        e.wanderRadius = 4;
        e.essential = true;
    }
}

// ---------------------------------------------------------------------------
//  Interni: Laboratori Delta-9
// ---------------------------------------------------------------------------
void BuildBaseInterior() {
    Z = &W.zones[ZONE_BASE];
    Z->name = "Laboratori Delta-9";
    Z->outdoor = false;
    Z->floorY = 0;
    Z->bmin = V3(-34, -2, -8);
    Z->bmax = V3(28, 12, 82);
    Z->ambSky = V3(0.26f, 0.28f, 0.28f);
    Z->ambGround = V3(0.15f, 0.15f, 0.15f);
    Z->fog = V3(0.02f, 0.025f, 0.02f);
    Z->fogDensity = 0.028f;
    Color wall = Col(128, 130, 120), floor = Col(80, 82, 78), ceil = Col(70, 72, 68);
    Color lamp = Col(235, 235, 215);
    // ingresso
    Room(-6, -6, 6, 6, 4, WS_BUNKER, wall, floor, ceil, {{SIDE_S, 6, 3, 3}}, 0, lamp, 6, 0.4f);
    for (int s = 0; s < 6; s++) Vis(V3(0, 0.2f + s * 0.35f, -5.5f + s * 0.0f - 0.1f * s), V3(3, 0.3f, 0.4f), C_CONCRETE_D);
    {
        Interactable& t = AddInter(INT_TRANSITION, ZONE_BASE, V3(0, 1.2f, -5.5f), PI, "Esci all'aperto", "base_exit");
        t.targetZone = ZONE_WASTE;
        t.targetPos = V3(270, 0, 220);
        t.targetYaw = -PI / 2;
        t.size = V3(3, 3, 0.5f);
    }
    // corridoio con posto di blocco
    Room(-2, 6, 2, 30, 3.5f, WS_BUNKER, wall, floor, ceil, {}, (1 << SIDE_N) | (1 << SIDE_S), Col(255, 60, 40), 6, 0.0f);
    Solid(V3(-1.2f, 0.6f, 20), V3(1.6f, 1.2f, 0.5f), C_METAL_D, true);
    Vis(V3(-1.2f, 1.25f, 20), V3(1.6f, 0.1f, 0.6f), C_VAULT_YEL);
    // snodo centrale
    Room(-12, 30, 12, 50, 5, WS_BUNKER, wall, floor, ceil, {{SIDE_N, 12, 4, 3.5f}, {SIDE_S, 12, 4, 3.5f}, {SIDE_W, 10, 3, 3}, {SIDE_E, 10, 3, 3}}, 0, lamp, 8, 0.5f);
    AddSign(V3(-11.75f, 3.8f, 40), PI / 2, 3.5f, 0.7f, "CASERMA", Col(60, 66, 60), Col(220, 220, 200));
    AddSign(V3(11.75f, 3.8f, 40), -PI / 2, 3.5f, 0.7f, "MAGAZZINO", Col(60, 66, 60), Col(220, 180, 60));
    AddSign(V3(0, 4.0f, 49.75f), PI, 4.5f, 0.8f, "LABORATORIO", Col(60, 66, 60), Col(120, 220, 255));
    Solid(V3(6, 0.45f, 34), V3(3, 0.9f, 1.2f), C_METAL_D, true);
    {
        Interactable& t = AddInter(INT_TERMINAL, ZONE_BASE, V3(6, 1.15f, 34.7f), 0, "Terminale di sicurezza interno", "sec_term_in");
        t.terminal = TERM_SECURITY;
        t.hackLevel = 2;
        t.size = V3(0.5f, 0.5f, 0.5f);
    }
    Cont(ZONE_BASE, -6, 33, 0, CM_DESK, 2);
    for (int i = 0; i < 4; i++) Crate(-9 + i * 1.1f, 47.5f, 0.9f, 0);
    // caserma
    Room(-32, 32, -12, 48, 3.5f, WS_BUNKER, wall, floor, ceil, {}, 1 << SIDE_E, lamp, 7, 0.5f);
    for (int i = 0; i < 4; i++) {
        Solid(V3(-29 + i * 4.5f, 0.4f, 46.5f), V3(1.1f, 0.5f, 2.2f), Col(90, 100, 80));
        Interactable& l = Cont(ZONE_BASE, -29 + i * 4.5f, 33.0f, 0, CM_LOCKER, 3, i == 1 ? 2 : 0);
        if (i == 1) {
            l.tag = "maint_locker";
            l.inv.Add(I_MAINTCODE);
        }
    }
    Cont(ZONE_BASE, -31, 40, PI / 2, CM_FOOTLOCKER, 3, 3);
    // magazzino
    Room(12, 34, 26, 46, 3.5f, WS_BUNKER, wall, floor, ceil, {}, 1 << SIDE_W, Col(255, 220, 170), 6);
    {
        Interactable& d = AddInter(INT_DOOR, ZONE_BASE, V3(12, 1.5f, 40), PI / 2, "Porta del magazzino", "storage_door");
        d.lock = 4;
        d.locked = true;
        d.size = V3(3, 3, 0.3f);
        d.collider = Z->AddCollider(V3(11.6f, 0, 38.4f), V3(12.4f, 3.1f, 41.6f), true);
    }
    {
        Interactable& c = Cont(ZONE_BASE, 24.5f, 40, -PI / 2, CM_LOCKER, 3, 0, "exosuit");
        c.name = "Rastrelliera degli esoscheletri";
        c.inv.Clear();
        c.inv.Add(I_EXOSUIT);
    }
    Cont(ZONE_BASE, 20, 45, PI, CM_LOCKER, 3);
    {
        Interactable& c = Cont(ZONE_BASE, 16, 45, PI, CM_LOCKER, 3);
        c.inv.Add(I_STEALTHBOY, 2);
        c.inv.Add(I_REPAIRKIT);
    }
    Cont(ZONE_BASE, 20, 35, 0, CM_AMMOBOX, 3);
    Cont(ZONE_BASE, 23, 35, 0, CM_AMMOBOX, 3);
    // laboratorio
    Room(-12, 50, 12, 80, 6, WS_BUNKER, Col(150, 158, 160), Col(96, 100, 102), ceil, {}, 1 << SIDE_N, Col(200, 235, 255), 8, 0.3f);
    for (int s = -1; s <= 1; s += 2)
        for (int i = 0; i < 3; i++) {
            Vector3 tp = V3(s * 9.5f, 0, 56 + i * 7);
            Cyl(tp, 0.9f, 0.4f, C_METAL_D, 12);
            Cyl(tp + V3(0, 0.4f, 0), 0.75f, 2.4f, Glow(Col(90, 230, 160), 0.55f), 12);
            Cyl(tp + V3(0, 2.8f, 0), 0.9f, 0.4f, C_METAL_D, 12);
            Light(tp + V3(0, 1.6f, 0), Col(90, 230, 160), 5, 0.6f);
        }
    for (int i = 0; i < 3; i++) Solid(V3(0, 0.45f, 56 + i * 5), V3(4, 0.9f, 1.4f), Col(170, 175, 175));
    for (int i = 0; i < 5; i++) {
        Solid(V3(-11.4f, 1.3f, 60 + i * 2.6f), V3(1.0f, 2.6f, 2.4f), C_METAL_D, true);
        Vis(V3(-10.85f, 1.6f, 60 + i * 2.6f), V3(0.1f, 1.0f, 1.8f), Glow(Col(80, 200, 255), 0.4f));
    }
    {
        Interactable& t = AddInter(INT_TERMINAL, ZONE_BASE, V3(-10.8f, 1.2f, 75.5f), PI / 2, "Terminale del laboratorio", "lab_term");
        t.terminal = TERM_LAB;
        t.hackLevel = 3;
        t.keyItem = I_MAINTCODE;
        t.size = V3(0.5f, 0.5f, 0.5f);
        Solid(V3(-11.3f, 0.5f, 75.5f), V3(1.0f, 1.0f, 1.4f), C_METAL_D, true);
    }
    // piedistallo con la cella
    Cyl(V3(0, 0, 74), 1.0f, 1.0f, C_METAL, 12);
    Light(V3(0, 2.0f, 74), Col(120, 200, 255), 7, 1.0f, 0.2f);
    ItemOnGround(ZONE_BASE, V3(0, 1.0f, 74), I_FUSIONCELL, 1, "fusion_cell");
    Cont(ZONE_BASE, 11.4f, 78, -PI / 2, CM_FIRSTAID, 2);
    W.zones[ZONE_BASE].Finalize();

    // sorveglianza automatica: telecamere e sentinelle (scortano fuori gli intrusi)
    Spawn(EK_CAMERA, ZONE_BASE, 0.8f, 24, PI);
    Spawn(EK_ROBOT, ZONE_BASE, -6, 40, 0).patrol = {V3(-8, 0, 36), V3(8, 0, 36), V3(8, 0, 46), V3(-8, 0, 46)};
    Spawn(EK_ROBOT, ZONE_BASE, -24, 40, PI / 2);
    Spawn(EK_CAMERA, ZONE_BASE, -30, 45, PI / 2);
    Spawn(EK_ROBOT, ZONE_BASE, 6, 62, PI);
    {
        Entity& e = Spawn(EK_ROBOT, ZONE_BASE, 0, 70, PI);
        e.name = "Sentinella Mk.V";
        e.tag = "mk5";
        e.photoKey = "mk5";
        e.scale = 1.6f;
        e.speed = 3.2f;
        e.detect = 24;
        e.territory = 14;
        e.patrol = {V3(-4.5f, 0, 70.5f), V3(4.5f, 0, 70.5f), V3(4.5f, 0, 77.5f), V3(-4.5f, 0, 77.5f)};
    }
}

void BuildVendors() {
    Vendor gus{"Emporio di Gus", {}, 650};
    int gi[][2] = {{I_BAIT, 6}, {I_NOISE, 8}, {I_JACKET, 1}, {I_DARKCLOAK, 1}, {I_COAT, 1}, {I_REPAIRKIT, 3}, {I_PARTS, 6}, {I_BOBBYPIN, 12}, {I_STEALTHBOY, 1}, {I_MEDKIT, 2}, {I_SCRAP, 3}, {I_CLOCK, 1}, {I_COMIC, 2}};
    for (auto& p : gi) gus.inv.Add(p[0], p[1]);
    Vendor rosi{"Clinica di Ferrocittà", {}, 400};
    int ri[][2] = {{I_MEDKIT, 8}, {I_RADAWAY, 5}, {I_RADX, 4}, {I_PUREWATER, 5}, {I_HERBTEA, 4}};
    for (auto& p : ri) rosi.inv.Add(p[0], p[1]);
    Vendor tano{"Bar di Tano", {}, 300};
    int ti[][2] = {{I_COLA, 12}, {I_BEANS, 6}, {I_CRAM, 4}, {I_MEAT, 6}, {I_WATER, 8}, {I_PUREWATER, 4}, {I_COFFEE, 4}};
    for (auto& p : ti) tano.inv.Add(p[0], p[1]);
    gVendorTemplates = {gus, rosi, tano};
}

} // namespace

void WorldBuild() {
    Zone& wz = W.zones[ZONE_WASTE];
    wz.id = ZONE_WASTE;
    wz.name = "Terre Bruciate";
    wz.outdoor = true;
    wz.bmin = V3(-WORLD_HALF, -50, -WORLD_HALF);
    wz.bmax = V3(WORLD_HALF, 200, WORLD_HALF);
    W.zones[ZONE_VAULT].id = ZONE_VAULT;
    W.zones[ZONE_BASE].id = ZONE_BASE;
    gEntTemplates.clear();
    gInterTemplates.clear();
    GenTerrain();
    Z = &wz;
    BuildVaultExterior();
    BuildTown();
    BuildGasStation();
    BuildRaiderCamp();
    BuildBaseExterior();
    BuildFarm();
    BuildRadioTower();
    BuildCrater();
    BuildCanyon();
    BuildRuins();
    BuildWasteland();
    wz.Finalize();
    BuildVault();
    BuildBaseInterior();
    GenMapTexture();
    BuildVendors();
    InitTerminals();
    InitPhotoSubjects();
    for (int i = 0; i < (int)gEntTemplates.size(); i++) gEntTemplates[i].id = i;
}

void ResetDynamicWorld() {
    G.ents = gEntTemplates;
    G.inters = gInterTemplates;
    G.vendors = gVendorTemplates;
    for (auto& it : G.inters)
        if (it.collider >= 0) W.zones[it.zone].boxes[it.collider].enabled = !it.open;
    for (auto& l : W.locations) l.discovered = false;
}
