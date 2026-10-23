// Rendering: shader d'illuminazione, cielo, costruttore di mesh, primitive.
#include "game.h"

Renderer R;

// ---------------------------------------------------------------------------
//  Shader
// ---------------------------------------------------------------------------
static const char* LIT_VS = R"(#version 330
in vec3 vertexPosition;
in vec3 vertexNormal;
in vec4 vertexColor;
uniform mat4 mvp;
uniform mat4 matModel;
uniform mat4 matNormal;
out vec3 fragPos;
out vec3 fragNormal;
out vec4 fragColor;
void main() {
    vec4 wp = matModel * vec4(vertexPosition, 1.0);
    fragPos = wp.xyz;
    fragNormal = normalize((matNormal * vec4(vertexNormal, 0.0)).xyz);
    fragColor = vertexColor;
    gl_Position = mvp * vec4(vertexPosition, 1.0);
}
)";

static const char* LIT_FS = R"(#version 330
in vec3 fragPos;
in vec3 fragNormal;
in vec4 fragColor;
uniform sampler2D texture0;
uniform vec4 colDiffuse;
uniform vec3 viewPos;
uniform vec3 sunDir;
uniform vec3 sunColor;
uniform vec3 skyAmb;
uniform vec3 groundAmb;
uniform vec3 fogColor;
uniform float fogDensity;
uniform float emissive;
uniform float time;
uniform int numLights;
uniform vec3 lightPos[8];
uniform vec3 lightColor[8];
uniform float lightRange[8];
out vec4 finalColor;
void main() {
    vec3 n = normalize(fragNormal);
    if (!gl_FrontFacing) n = -n;
    vec3 bw = abs(n);
    bw /= (bw.x + bw.y + bw.z + 0.0001);
    float dx = texture(texture0, fragPos.zy * 0.37).r;
    float dy = texture(texture0, fragPos.xz * 0.37).r;
    float dz = texture(texture0, fragPos.xy * 0.37).r;
    float detail = dx * bw.x + dy * bw.y + dz * bw.z;
    float big = texture(texture0, fragPos.xz * 0.023 + fragPos.y * 0.01).g;
    detail = mix(0.78, 1.16, detail) * mix(0.86, 1.08, big);
    vec4 base = fragColor * colDiffuse;
    float emi = clamp(emissive + (1.0 - fragColor.a), 0.0, 1.0);
    vec3 albedo = base.rgb * detail;
    float ndl = max(dot(n, sunDir), 0.0);
    vec3 amb = mix(groundAmb, skyAmb, n.y * 0.5 + 0.5);
    vec3 light = amb + sunColor * ndl;
    for (int i = 0; i < 8; i++) {
        if (i >= numLights) break;
        vec3 L = lightPos[i] - fragPos;
        float d = length(L);
        float att = clamp(1.0 - d / lightRange[i], 0.0, 1.0);
        att *= att;
        float nl = max(dot(n, L / max(d, 0.001)), 0.0) * 0.85 + 0.15;
        light += lightColor[i] * att * nl;
    }
    vec3 col = albedo * light;
    col = mix(col, base.rgb * 1.25, emi);
    float dist = length(fragPos - viewPos);
    float f = 1.0 - exp(-pow(dist * fogDensity, 1.5));
    col = mix(col, fogColor, clamp(f * (1.0 - emi * 0.5), 0.0, 1.0));
    finalColor = vec4(col, 1.0);
}
)";

static const char* SKY_VS = R"(#version 330
in vec3 vertexPosition;
uniform mat4 mvp;
out vec3 dir;
void main() {
    dir = vertexPosition;
    gl_Position = mvp * vec4(vertexPosition, 1.0);
}
)";

static const char* SKY_FS = R"(#version 330
in vec3 dir;
uniform vec3 sunDir;
uniform vec3 zenith;
uniform vec3 horizon;
uniform vec3 sunCol;
uniform float night;
uniform float time;
out vec4 finalColor;
float hash(vec3 p) { return fract(sin(dot(p, vec3(12.9898, 78.233, 37.719))) * 43758.5453); }
float noise(vec2 p) {
    vec2 i = floor(p); vec2 f = fract(p); f = f * f * (3.0 - 2.0 * f);
    float a = hash(vec3(i, 1.0)), b = hash(vec3(i + vec2(1, 0), 1.0));
    float c = hash(vec3(i + vec2(0, 1), 1.0)), d = hash(vec3(i + vec2(1, 1), 1.0));
    return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}
void main() {
    vec3 d = normalize(dir);
    float h = d.y;
    vec3 col = mix(horizon, zenith, pow(clamp(h, 0.0, 1.0), 0.55));
    if (h < 0.0) col = horizon * (1.0 + h * 0.3);
    float sd = max(dot(d, sunDir), 0.0);
    col += sunCol * (pow(sd, 900.0) * 6.0 + pow(sd, 18.0) * 0.30 + pow(sd, 4.0) * 0.10) * (1.0 - night);
    // luna
    float md = max(dot(d, -sunDir), 0.0);
    col += vec3(0.75, 0.8, 0.9) * (smoothstep(0.9993, 0.9996, md) * 0.9 + pow(md, 60.0) * 0.08) * night;
    // stelle
    if (night > 0.01 && h > 0.0) {
        vec3 p = floor(d * 260.0);
        float r = hash(p);
        if (r > 0.9965) col += vec3(0.9, 0.9, 1.0) * (r - 0.9965) * 280.0 * night * smoothstep(0.0, 0.25, h) * (0.7 + 0.3 * sin(time * 3.0 + r * 50.0));
    }
    // nubi di polvere
    if (h > 0.0) {
        vec2 uv = d.xz / (h + 0.15) * 1.6 + vec2(time * 0.004, time * 0.002);
        float n = noise(uv) * 0.6 + noise(uv * 2.3) * 0.3 + noise(uv * 5.1) * 0.1;
        float cl = smoothstep(0.45, 0.85, n) * smoothstep(0.0, 0.3, h);
        vec3 cc = mix(horizon * 1.1, sunCol * 0.6 + horizon * 0.5, 0.4) * (1.0 - night * 0.8);
        col = mix(col, cc, cl * 0.55);
    }
    finalColor = vec4(col, 1.0);
}
)";

// ---------------------------------------------------------------------------
//  MeshBuilder
// ---------------------------------------------------------------------------
static inline void PushV(std::vector<float>& v, Vector3 p) { v.push_back(p.x); v.push_back(p.y); v.push_back(p.z); }
static inline void PushC(std::vector<unsigned char>& c, Color col) { c.push_back(col.r); c.push_back(col.g); c.push_back(col.b); c.push_back(col.a); }

void MeshBuilder::Tri(Vector3 a, Vector3 b, Vector3 cc, Color col) {
    Vector3 nn = Vector3Normalize(Vector3CrossProduct(b - a, cc - a));
    PushV(v, a); PushV(v, b); PushV(v, cc);
    PushV(n, nn); PushV(n, nn); PushV(n, nn);
    PushC(c, col); PushC(c, col); PushC(c, col);
}

void MeshBuilder::TriN(Vector3 a, Vector3 b, Vector3 cc, Vector3 na, Vector3 nb, Vector3 nc, Color ca, Color cb, Color c3) {
    PushV(v, a); PushV(v, b); PushV(v, cc);
    PushV(n, na); PushV(n, nb); PushV(n, nc);
    PushC(c, ca); PushC(c, cb); PushC(c, c3);
}

void MeshBuilder::Quad(Vector3 a, Vector3 b, Vector3 cc, Vector3 d, Color col) {
    Tri(a, b, cc, col);
    Tri(a, cc, d, col);
}

void MeshBuilder::Box(const Matrix& m, Color col) {
    Vector3 p[8];
    for (int i = 0; i < 8; i++) {
        Vector3 l = V3((i & 1) ? 0.5f : -0.5f, (i & 2) ? 0.5f : -0.5f, (i & 4) ? 0.5f : -0.5f);
        p[i] = Vector3Transform(l, m);
    }
    // facce in senso antiorario viste dall'esterno
    Color top = col, side = col, bot = ColShade(col, 0.9f);
    Quad(p[4], p[5], p[7], p[6], side);  // +z
    Quad(p[1], p[0], p[2], p[3], side);  // -z
    Quad(p[5], p[1], p[3], p[7], side);  // +x
    Quad(p[0], p[4], p[6], p[2], side);  // -x
    Quad(p[2], p[6], p[7], p[3], top);   // +y
    Quad(p[0], p[1], p[5], p[4], bot);   // -y
}

void MeshBuilder::BoxAt(Vector3 center, Vector3 size, Color col, float yaw) {
    Box(MatTRS(center, V3(0, yaw, 0), size), col);
}

void MeshBuilder::Cylinder(const Matrix& m, int seg, Color col, bool caps, float topScale) {
    Matrix nm = MatrixTranspose(MatrixInvert(m));
    auto TN = [&](Vector3 d) { return Vector3Normalize(Vector3Transform(d, nm) - Vector3Transform(V3(0, 0, 0), nm)); };
    for (int i = 0; i < seg; i++) {
        float a0 = (float)i / seg * 2 * PI, a1 = (float)(i + 1) / seg * 2 * PI;
        Vector3 d0 = V3(sinf(a0), 0, cosf(a0)), d1 = V3(sinf(a1), 0, cosf(a1));
        Vector3 b0 = Vector3Transform(d0 * 0.5f + V3(0, -0.5f, 0), m);
        Vector3 b1 = Vector3Transform(d1 * 0.5f + V3(0, -0.5f, 0), m);
        Vector3 t0 = Vector3Transform(d0 * (0.5f * topScale) + V3(0, 0.5f, 0), m);
        Vector3 t1 = Vector3Transform(d1 * (0.5f * topScale) + V3(0, 0.5f, 0), m);
        Vector3 n0 = TN(d0), n1 = TN(d1);
        TriN(b0, b1, t1, n0, n1, n1, col, col, col);
        TriN(b0, t1, t0, n0, n1, n0, col, col, col);
        if (caps) {
            Vector3 tc = Vector3Transform(V3(0, 0.5f, 0), m), bc = Vector3Transform(V3(0, -0.5f, 0), m);
            if (topScale > 0.001f) Tri(tc, t0, t1, col);
            Tri(bc, b1, b0, ColShade(col, 0.85f));
        }
    }
}

void MeshBuilder::Sphere(const Matrix& m, int rings, int segs, Color col) {
    Matrix nm = MatrixTranspose(MatrixInvert(m));
    Vector3 o = Vector3Transform(V3(0, 0, 0), nm);
    auto P = [&](int r, int s) {
        float th = (float)r / rings * PI, ph = (float)s / segs * 2 * PI;
        return V3(sinf(th) * sinf(ph), cosf(th), sinf(th) * cosf(ph));
    };
    for (int r = 0; r < rings; r++)
        for (int s = 0; s < segs; s++) {
            Vector3 a = P(r, s), b = P(r + 1, s), cc = P(r + 1, s + 1), d = P(r, s + 1);
            Vector3 A = Vector3Transform(a * 0.5f, m), B = Vector3Transform(b * 0.5f, m);
            Vector3 C = Vector3Transform(cc * 0.5f, m), D = Vector3Transform(d * 0.5f, m);
            Vector3 na = Vector3Normalize(Vector3Transform(a, nm) - o), nb = Vector3Normalize(Vector3Transform(b, nm) - o);
            Vector3 nc = Vector3Normalize(Vector3Transform(cc, nm) - o), nd = Vector3Normalize(Vector3Transform(d, nm) - o);
            if (r > 0) TriN(A, B, C, na, nb, nc, col, col, col);
            if (r < rings - 1) TriN(A, C, D, na, nc, nd, col, col, col);
        }
}

void MeshBuilder::Rock(const Matrix& m, Rng& r, Color col) {
    // sfera irregolare a facce piatte
    const int rings = 5, segs = 7;
    Vector3 pts[rings + 1][segs + 1];
    float seed = r.range(0, 100);
    for (int i = 0; i <= rings; i++)
        for (int s = 0; s <= segs; s++) {
            float th = (float)i / rings * PI, ph = (float)(s % segs) / segs * 2 * PI;
            Vector3 d = V3(sinf(th) * sinf(ph), cosf(th), sinf(th) * cosf(ph));
            float k = 0.75f + 0.5f * ValueNoise(d.x * 2.1f + seed, d.z * 2.1f + d.y * 1.7f, 99);
            pts[i][s] = Vector3Transform(d * (0.5f * k), m);
        }
    for (int i = 0; i < rings; i++)
        for (int s = 0; s < segs; s++) {
            Color cc = ColJitter(col, r, 0.08f);
            if (i > 0) Tri(pts[i][s], pts[i + 1][s], pts[i + 1][s + 1], cc);
            if (i < rings - 1) Tri(pts[i][s], pts[i + 1][s + 1], pts[i][s + 1], cc);
        }
}

void MeshBuilder::Torus(const Matrix& m, float Rr, float rr, int seg, int side, Color col) {
    for (int i = 0; i < seg; i++)
        for (int j = 0; j < side; j++) {
            auto P = [&](int a, int b) {
                float u = (float)a / seg * 2 * PI, w = (float)b / side * 2 * PI;
                return Vector3Transform(V3((Rr + rr * cosf(w)) * cosf(u), rr * sinf(w), (Rr + rr * cosf(w)) * sinf(u)), m);
            };
            Vector3 a = P(i, j), b = P(i + 1, j), cc = P(i + 1, j + 1), d = P(i, j + 1);
            Tri(a, cc, b, col);
            Tri(a, d, cc, col);
        }
}

void MeshBuilder::Append(const MeshBuilder& o) {
    v.insert(v.end(), o.v.begin(), o.v.end());
    n.insert(n.end(), o.n.begin(), o.n.end());
    c.insert(c.end(), o.c.begin(), o.c.end());
}

Mesh MeshBuilder::Build() {
    Mesh mesh = {};
    mesh.vertexCount = (int)(v.size() / 3);
    mesh.triangleCount = mesh.vertexCount / 3;
    mesh.vertices = (float*)MemAlloc((unsigned int)(v.size() * sizeof(float)));
    mesh.normals = (float*)MemAlloc((unsigned int)(n.size() * sizeof(float)));
    mesh.colors = (unsigned char*)MemAlloc((unsigned int)c.size());
    memcpy(mesh.vertices, v.data(), v.size() * sizeof(float));
    memcpy(mesh.normals, n.data(), n.size() * sizeof(float));
    memcpy(mesh.colors, c.data(), c.size());
    UploadMesh(&mesh, false);
    return mesh;
}

// ---------------------------------------------------------------------------
//  Texture procedurali
// ---------------------------------------------------------------------------
static Texture2D MakeDetailTexture() {
    const int S = 256;
    Image img = GenImageColor(S, S, WHITE);
    Color* px = (Color*)img.data;
    for (int y = 0; y < S; y++)
        for (int x = 0; x < S; x++) {
            // rumore periodico (tileable) su più ottave
            float v = 0, amp = 0.5f, norm = 0;
            int period = 8;
            for (int o = 0; o < 5; o++) {
                float fx = (float)x / S * period, fy = (float)y / S * period;
                int ix = (int)fx, iy = (int)fy;
                float tx = fx - ix, ty = fy - iy;
                tx = tx * tx * (3 - 2 * tx);
                ty = ty * ty * (3 - 2 * ty);
                float a = Hash2(ix % period, iy % period, o), b = Hash2((ix + 1) % period, iy % period, o);
                float c = Hash2(ix % period, (iy + 1) % period, o), d = Hash2((ix + 1) % period, (iy + 1) % period, o);
                v += LerpF(LerpF(a, b, tx), LerpF(c, d, tx), ty) * amp;
                norm += amp;
                amp *= 0.55f;
                period *= 2;
            }
            v /= norm;
            float grain = Hash2(x, y, 77);
            float r = Clamp01(v * 0.8f + grain * 0.2f);
            // canale G: macchie larghe
            float fx = (float)x / S * 4, fy = (float)y / S * 4;
            int ix = (int)fx, iy = (int)fy;
            float tx = fx - ix, ty = fy - iy;
            tx = tx * tx * (3 - 2 * tx);
            ty = ty * ty * (3 - 2 * ty);
            float g = LerpF(LerpF(Hash2(ix % 4, iy % 4, 5), Hash2((ix + 1) % 4, iy % 4, 5), tx),
                            LerpF(Hash2(ix % 4, (iy + 1) % 4, 5), Hash2((ix + 1) % 4, (iy + 1) % 4, 5), tx), ty);
            px[y * S + x] = Color{(unsigned char)(r * 255), (unsigned char)(g * 255), (unsigned char)(grain * 255), 255};
        }
    Texture2D t = LoadTextureFromImage(img);
    UnloadImage(img);
    GenTextureMipmaps(&t);
    SetTextureFilter(t, TEXTURE_FILTER_TRILINEAR);
    SetTextureWrap(t, TEXTURE_WRAP_REPEAT);
    return t;
}

static Texture2D MakeSoftDot() {
    const int S = 64;
    Image img = GenImageColor(S, S, BLANK);
    Color* px = (Color*)img.data;
    for (int y = 0; y < S; y++)
        for (int x = 0; x < S; x++) {
            float dx = (x + 0.5f) / S * 2 - 1, dy = (y + 0.5f) / S * 2 - 1;
            float d = sqrtf(dx * dx + dy * dy);
            float a = Clamp01(1 - d);
            a = a * a * (3 - 2 * a);
            px[y * S + x] = Color{255, 255, 255, (unsigned char)(a * 255)};
        }
    Texture2D t = LoadTextureFromImage(img);
    UnloadImage(img);
    SetTextureFilter(t, TEXTURE_FILTER_BILINEAR);
    return t;
}

static Texture2D MakeVignette() {
    const int S = 256;
    Image img = GenImageColor(S, S, BLANK);
    Color* px = (Color*)img.data;
    for (int y = 0; y < S; y++)
        for (int x = 0; x < S; x++) {
            float dx = (x + 0.5f) / S * 2 - 1, dy = (y + 0.5f) / S * 2 - 1;
            float d = sqrtf(dx * dx + dy * dy) / 1.414f;
            float a = SmoothStepF(0.35f, 1.0f, d);
            px[y * S + x] = Color{255, 255, 255, (unsigned char)(a * 255)};
        }
    Texture2D t = LoadTextureFromImage(img);
    UnloadImage(img);
    SetTextureFilter(t, TEXTURE_FILTER_BILINEAR);
    return t;
}

static Texture2D MakeNoise() {
    const int S = 256;
    Image img = GenImageColor(S, S, BLANK);
    Color* px = (Color*)img.data;
    for (int i = 0; i < S * S; i++) {
        unsigned char v = (unsigned char)(Hash2(i, i * 7, 3) * 255);
        px[i] = Color{v, v, v, 255};
    }
    Texture2D t = LoadTextureFromImage(img);
    UnloadImage(img);
    SetTextureWrap(t, TEXTURE_WRAP_REPEAT);
    return t;
}

// ---------------------------------------------------------------------------
//  Inizializzazione
// ---------------------------------------------------------------------------
void RenderInit() {
    R.lit = LoadShaderFromMemory(LIT_VS, LIT_FS);
    R.lit.locs[SHADER_LOC_MATRIX_MODEL] = GetShaderLocation(R.lit, "matModel");
    R.lit.locs[SHADER_LOC_MATRIX_NORMAL] = GetShaderLocation(R.lit, "matNormal");
    R.locViewPos = GetShaderLocation(R.lit, "viewPos");
    R.locSunDir = GetShaderLocation(R.lit, "sunDir");
    R.locSunColor = GetShaderLocation(R.lit, "sunColor");
    R.locSkyAmb = GetShaderLocation(R.lit, "skyAmb");
    R.locGroundAmb = GetShaderLocation(R.lit, "groundAmb");
    R.locFogColor = GetShaderLocation(R.lit, "fogColor");
    R.locFogDensity = GetShaderLocation(R.lit, "fogDensity");
    R.locEmissive = GetShaderLocation(R.lit, "emissive");
    R.locNumLights = GetShaderLocation(R.lit, "numLights");
    R.locLightPos = GetShaderLocation(R.lit, "lightPos[0]");
    R.locLightColor = GetShaderLocation(R.lit, "lightColor[0]");
    R.locLightRange = GetShaderLocation(R.lit, "lightRange[0]");
    R.locTime = GetShaderLocation(R.lit, "time");

    R.sky = LoadShaderFromMemory(SKY_VS, SKY_FS);
    R.skLocSunDir = GetShaderLocation(R.sky, "sunDir");
    R.skLocZenith = GetShaderLocation(R.sky, "zenith");
    R.skLocHorizon = GetShaderLocation(R.sky, "horizon");
    R.skLocSunCol = GetShaderLocation(R.sky, "sunCol");
    R.skLocNight = GetShaderLocation(R.sky, "night");
    R.skLocTime = GetShaderLocation(R.sky, "time");

    R.detail = MakeDetailTexture();
    R.softDot = MakeSoftDot();
    R.vignette = MakeVignette();
    R.noiseTex = MakeNoise();

    R.litMat = LoadMaterialDefault();
    R.litMat.shader = R.lit;
    R.litMat.maps[MATERIAL_MAP_DIFFUSE].texture = R.detail;

    // primitive unitarie centrate
    MeshBuilder b;
    b.Box(MatrixIdentity(), WHITE);
    R.prim[PR_CUBE] = LoadModelFromMesh(b.Build());
    b.Clear();
    b.Sphere(MatrixIdentity(), 8, 12, WHITE);
    R.prim[PR_SPHERE] = LoadModelFromMesh(b.Build());
    b.Clear();
    b.Cylinder(MatrixIdentity(), 10, WHITE, true);
    R.prim[PR_CYL] = LoadModelFromMesh(b.Build());
    b.Clear();
    b.Cylinder(MatrixIdentity(), 10, WHITE, true, 0.0f);
    R.prim[PR_CONE] = LoadModelFromMesh(b.Build());
    for (int i = 0; i < PR_COUNT; i++) {
        R.prim[i].materials[0].shader = R.lit;
        R.prim[i].materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = R.detail;
    }
    // cupola del cielo
    b.Clear();
    b.Sphere(MatrixIdentity(), 16, 24, WHITE);
    R.skyDome = LoadModelFromMesh(b.Build());
    R.skyDome.materials[0].shader = R.sky;
}

void RenderShutdown() {
    for (int i = 0; i < PR_COUNT; i++) UnloadModel(R.prim[i]);
    UnloadModel(R.skyDome);
    UnloadTexture(R.detail);
    UnloadTexture(R.softDot);
    UnloadTexture(R.vignette);
    UnloadTexture(R.noiseTex);
    UnloadShader(R.lit);
    UnloadShader(R.sky);
}

void RenderApplyFrameUniforms(Vector3 camPos, float time) {
    SetShaderValue(R.lit, R.locViewPos, &camPos, SHADER_UNIFORM_VEC3);
    SetShaderValue(R.lit, R.locSunDir, &R.sunDir, SHADER_UNIFORM_VEC3);
    SetShaderValue(R.lit, R.locSunColor, &R.sunColor, SHADER_UNIFORM_VEC3);
    SetShaderValue(R.lit, R.locSkyAmb, &R.skyAmb, SHADER_UNIFORM_VEC3);
    SetShaderValue(R.lit, R.locGroundAmb, &R.groundAmb, SHADER_UNIFORM_VEC3);
    SetShaderValue(R.lit, R.locFogColor, &R.fogColor, SHADER_UNIFORM_VEC3);
    SetShaderValue(R.lit, R.locFogDensity, &R.fogDensity, SHADER_UNIFORM_FLOAT);
    SetShaderValue(R.lit, R.locTime, &time, SHADER_UNIFORM_FLOAT);
    float e = 0;
    SetShaderValue(R.lit, R.locEmissive, &e, SHADER_UNIFORM_FLOAT);
    R.curEmissive = 0;
}

void RenderSetLights(const std::vector<LightSrc>& lights, Vector3 camPos, float time) {
    // sceglie le luci più rilevanti per la camera
    struct Cand { float score; int idx; };
    std::vector<Cand> cands;
    for (int i = 0; i < (int)lights.size(); i++) {
        const LightSrc& l = lights[i];
        if (l.nightOnly && R.night < 0.25f) continue;
        float d = Vector3Distance(l.pos, camPos) - l.range;
        if (d > 60) continue;
        cands.push_back({d, i});
    }
    std::sort(cands.begin(), cands.end(), [](const Cand& a, const Cand& b) { return a.score < b.score; });
    Vector3 pos[MAX_LIGHTS], col[MAX_LIGHTS];
    float range[MAX_LIGHTS];
    int n = 0;
    for (auto& c : cands) {
        if (n >= MAX_LIGHTS) break;
        const LightSrc& l = lights[c.idx];
        float k = l.intensity;
        if (l.flicker > 0) {
            float f = sinf(time * 13.0f + c.idx * 7.1f) * 0.5f + sinf(time * 31.0f + c.idx) * 0.3f + ValueNoise(time * 8, (float)c.idx, 4) * 0.6f;
            k *= 1.0f - l.flicker * Clamp01(f * 0.5f + 0.3f);
        }
        if (l.nightOnly) k *= Clamp01((R.night - 0.25f) * 3);
        pos[n] = l.pos;
        col[n] = ColToV3(l.color) * k;
        range[n] = l.range;
        n++;
    }
    SetShaderValue(R.lit, R.locNumLights, &n, SHADER_UNIFORM_INT);
    if (n > 0) {
        SetShaderValueV(R.lit, R.locLightPos, pos, SHADER_UNIFORM_VEC3, n);
        SetShaderValueV(R.lit, R.locLightColor, col, SHADER_UNIFORM_VEC3, n);
        SetShaderValueV(R.lit, R.locLightRange, range, SHADER_UNIFORM_FLOAT, n);
    }
}

void DrawPrim(int prim, const Matrix& m, Color c, float emissive) {
    if (emissive != R.curEmissive) {
        SetShaderValue(R.lit, R.locEmissive, &emissive, SHADER_UNIFORM_FLOAT);
        R.curEmissive = emissive;
    }
    Model& md = R.prim[prim];
    md.materials[0].maps[MATERIAL_MAP_DIFFUSE].color = c;
    DrawMesh(md.meshes[0], md.materials[0], m);
}

void DrawPrimBox(Vector3 center, Vector3 size, float yaw, Color c, float emissive) {
    DrawPrim(PR_CUBE, MatTRS(center, V3(0, yaw, 0), size), c, emissive);
}

void DrawLimb(const Matrix& parent, Vector3 a, Vector3 b, float radius, Color c, float emissive) {
    DrawPrim(PR_CYL, MatAlignY(a, b, radius) * parent, c, emissive);
}

void DrawSky(Camera3D cam) {
    Vector3 hor = R.fogColor;
    SetShaderValue(R.sky, R.skLocSunDir, &R.skySun, SHADER_UNIFORM_VEC3);
    SetShaderValue(R.sky, R.skLocZenith, &R.zenith, SHADER_UNIFORM_VEC3);
    SetShaderValue(R.sky, R.skLocHorizon, &hor, SHADER_UNIFORM_VEC3);
    Vector3 sc = R.sunColor;
    SetShaderValue(R.sky, R.skLocSunCol, &sc, SHADER_UNIFORM_VEC3);
    SetShaderValue(R.sky, R.skLocNight, &R.night, SHADER_UNIFORM_FLOAT);
    float t = (float)GetTime();
    SetShaderValue(R.sky, R.skLocTime, &t, SHADER_UNIFORM_FLOAT);
    rlDisableDepthMask();
    rlDisableBackfaceCulling();
    Matrix m = MatrixScale(500, 500, 500) * MatrixTranslate(cam.position.x, cam.position.y, cam.position.z);
    DrawMesh(R.skyDome.meshes[0], R.skyDome.materials[0], m);
    rlDrawRenderBatchActive();
    rlEnableBackfaceCulling();
    rlEnableDepthMask();
}

void DrawGlowBillboard(Camera3D cam, Vector3 pos, float size, Color c) {
    DrawBillboard(cam, R.softDot, pos, size, c);
}

void DrawBeam(Vector3 a, Vector3 b, float radius, Color c) {
    DrawCylinderEx(a, b, radius, radius, 5, c);
}
