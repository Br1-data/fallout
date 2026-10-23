// Funzioni di utilità: rumore, colori, matematica.
#include "game.h"
#include <cstdarg>

Rng gRng(12345);

float Hash2(int x, int z, int seed) {
    uint32_t h = (uint32_t)x * 374761393u + (uint32_t)z * 668265263u + (uint32_t)seed * 2246822519u;
    h = (h ^ (h >> 13)) * 1274126177u;
    h ^= h >> 16;
    return (h & 0xFFFFFF) / 16777215.0f;
}

float ValueNoise(float x, float z, int seed) {
    int ix = (int)floorf(x), iz = (int)floorf(z);
    float fx = x - ix, fz = z - iz;
    float ux = fx * fx * (3 - 2 * fx), uz = fz * fz * (3 - 2 * fz);
    float a = Hash2(ix, iz, seed), b = Hash2(ix + 1, iz, seed);
    float c = Hash2(ix, iz + 1, seed), d = Hash2(ix + 1, iz + 1, seed);
    return LerpF(LerpF(a, b, ux), LerpF(c, d, ux), uz);
}

float Fbm(float x, float z, int octaves, int seed) {
    float sum = 0, amp = 0.5f, norm = 0;
    for (int i = 0; i < octaves; i++) {
        sum += ValueNoise(x, z, seed + i * 17) * amp;
        norm += amp;
        amp *= 0.5f;
        x *= 2.03f;
        z *= 2.03f;
    }
    return sum / norm;
}

Color ColShade(Color c, float f) {
    return Color{(unsigned char)Clamp(c.r * f, 0.f, 255.f), (unsigned char)Clamp(c.g * f, 0.f, 255.f),
                 (unsigned char)Clamp(c.b * f, 0.f, 255.f), c.a};
}

Color ColLerp(Color a, Color b, float t) {
    t = Clamp01(t);
    return Color{(unsigned char)(a.r + (b.r - a.r) * t), (unsigned char)(a.g + (b.g - a.g) * t),
                 (unsigned char)(a.b + (b.b - a.b) * t), (unsigned char)(a.a + (b.a - a.a) * t)};
}

Color ColJitter(Color c, Rng& r, float amount) {
    float f = 1.0f + r.range(-amount, amount);
    return ColShade(c, f);
}

Color ColAlpha(Color c, float a) {
    c.a = (unsigned char)Clamp(a * 255.f, 0.f, 255.f);
    return c;
}

float AngleWrap(float a) {
    while (a > PI) a -= 2 * PI;
    while (a < -PI) a += 2 * PI;
    return a;
}

float AngleTo(Vector3 from, Vector3 to) {
    return atan2f(to.x - from.x, to.z - from.z);
}

float DistXZ(Vector3 a, Vector3 b) {
    float dx = a.x - b.x, dz = a.z - b.z;
    return sqrtf(dx * dx + dz * dz);
}

Vector3 YawDir(float yaw) { return V3(sinf(yaw), 0, cosf(yaw)); }

std::string Fmt(const char* fmt, ...) {
    char buf[1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    return std::string(buf);
}

Matrix MatTRS(Vector3 t, Vector3 e, Vector3 s) {
    Matrix m = MatrixScale(s.x, s.y, s.z);
    if (e.x != 0 || e.y != 0 || e.z != 0) m = m * MatrixRotateXYZ(e);
    return m * MatrixTranslate(t.x, t.y, t.z);
}

Matrix MatAlignY(Vector3 a, Vector3 b, float radius) {
    Vector3 d = b - a;
    float len = Vector3Length(d);
    if (len < 1e-5f) return MatTRS(a, V3(0, 0, 0), V3(radius * 2, 0.001f, radius * 2));
    Vector3 dir = d / len;
    Vector3 up = V3(0, 1, 0);
    Matrix rot = MatrixIdentity();
    float dt = Vector3DotProduct(up, dir);
    if (dt < 0.9999f) {
        if (dt < -0.9999f) rot = MatrixRotateX(PI);
        else {
            Vector3 axis = Vector3Normalize(Vector3CrossProduct(up, dir));
            rot = MatrixRotate(axis, acosf(dt));
        }
    }
    Vector3 mid = (a + b) * 0.5f;
    return MatrixScale(radius * 2, len, radius * 2) * rot * MatrixTranslate(mid.x, mid.y, mid.z);
}
