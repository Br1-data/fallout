// Audio interamente sintetizzato: effetti, ambiente e la radio del Pip-Boy.
#include "game.h"

static const int SR = 22050;
static const int POOL = 4;
static Sound sBase[SFX_COUNT];
static Sound sPool[SFX_COUNT][POOL];
static int sNext[SFX_COUNT];
static bool sReady = false;
int gRadioStation = 0;

struct LoopStream { Music music{}; std::vector<unsigned char> wav; bool ok = false; float vol = 0, target = 0; };
static LoopStream sWind, sHum, sRadio, sMorse;

static Rng aRng(777);
static float Noise() { return aRng.f() * 2 - 1; }
static float MidiF(float m) { return 440.0f * powf(2.0f, (m - 69) / 12.0f); }

// ---------------------------------------------------------------------------
//  Costruzione dei suoni
// ---------------------------------------------------------------------------
static Sound ToSound(const std::vector<float>& b, float gain = 1.0f) {
    Wave w{};
    w.frameCount = (unsigned int)b.size();
    w.sampleRate = SR;
    w.sampleSize = 16;
    w.channels = 1;
    short* d = (short*)MemAlloc((unsigned int)(b.size() * sizeof(short)));
    float peak = 0.001f;
    for (float v : b) peak = std::max(peak, fabsf(v));
    float norm = std::min(1.0f, 0.95f / peak) * gain;
    for (size_t i = 0; i < b.size(); i++) d[i] = (short)(Clamp(b[i] * norm, -1.f, 1.f) * 32000);
    w.data = d;
    Sound s = LoadSoundFromWave(w);
    UnloadWave(w);
    return s;
}

static std::vector<float> Buf(float sec) { return std::vector<float>((size_t)(sec * SR), 0.0f); }

struct LP { float y = 0, a; explicit LP(float a_) : a(a_) {} float operator()(float x) { y += a * (x - y); return y; } };

static std::vector<float> Sweep(float len, float f0, float f1, float decay, float square, float noiseAmt) {
    auto b = Buf(len);
    float ph = 0;
    for (size_t i = 0; i < b.size(); i++) {
        float t = (float)i / SR;
        float k = t / len;
        float f = f0 * powf(f1 / f0, k);
        ph += 2 * PI * f / SR;
        float s = sinf(ph);
        float v = s * (1 - square) + (s > 0 ? 1.f : -1.f) * square;
        b[i] = (v + Noise() * noiseAmt) * expf(-t * decay);
    }
    return b;
}

static std::vector<float> Tones(std::initializer_list<float> freqs, float each, float decay, float square = 0.2f) {
    auto b = Buf(each * freqs.size() + 0.3f);
    int k = 0;
    for (float f : freqs) {
        size_t start = (size_t)(k * each * SR);
        float ph = 0;
        for (size_t i = start; i < b.size(); i++) {
            float t = (float)(i - start) / SR;
            ph += 2 * PI * f / SR;
            float s = sinf(ph);
            b[i] += (s * (1 - square) + (s > 0 ? square : -square)) * expf(-t * decay) * std::min(1.0f, t * 400);
        }
        k++;
    }
    return b;
}

static std::vector<float> NoiseEnv(float len, float lpA, float attack, float decay, float hp = 0) {
    auto b = Buf(len);
    LP lp(lpA), lpl(hp > 0 ? hp : 0.0001f);
    for (size_t i = 0; i < b.size(); i++) {
        float t = (float)i / SR;
        float v = lp(Noise());
        if (hp > 0) v -= lpl(v);
        float env = std::min(1.0f, t / std::max(attack, 1e-4f)) * expf(-t * decay);
        b[i] = v * env;
    }
    return b;
}

static void Mix(std::vector<float>& a, const std::vector<float>& b, float g = 1.0f, float at = 0) {
    size_t off = (size_t)(at * SR);
    if (a.size() < off + b.size()) a.resize(off + b.size(), 0);
    for (size_t i = 0; i < b.size(); i++) a[off + i] += b[i] * g;
}

static std::vector<float> Voice(float len, float f0, float f1, float lpA, float noise, float am = 0) {
    auto b = Buf(len);
    LP lp(lpA);
    float ph = 0;
    for (size_t i = 0; i < b.size(); i++) {
        float t = (float)i / SR;
        float k = t / len;
        float f = LerpF(f0, f1, k) * (1 + 0.03f * sinf(t * 35));
        ph += f / SR;
        float saw = 2 * (ph - floorf(ph)) - 1;
        float v = lp(saw + Noise() * noise);
        if (am > 0) v *= 0.6f + 0.4f * sinf(t * am * 2 * PI);
        float env = std::min(1.0f, t * 20) * (1 - k) * (1 - k * 0.3f);
        b[i] = v * env;
    }
    return b;
}

// piccolo "ping" metallico (lattine, pioli, attrezzi)
static std::vector<float> Ping(float len, float f, float decay) {
    auto b = Buf(len);
    for (size_t i = 0; i < b.size(); i++) {
        float t = (float)i / SR;
        b[i] = (sinf(2 * PI * f * t) + 0.6f * sinf(2 * PI * f * 1.52f * t) + 0.4f * sinf(2 * PI * f * 2.19f * t)) * expf(-t * decay) * 0.5f;
    }
    Mix(b, NoiseEnv(0.01f, 0.8f, 0.0003f, 300), 0.6f);
    return b;
}

static void BuildSfx() {
    {
        // otturatore: due scatti e il trascinamento della pellicola
        auto b = Buf(0.42f);
        Mix(b, NoiseEnv(0.02f, 0.9f, 0.0003f, 260, 0.3f), 1.0f, 0.0f);
        Mix(b, NoiseEnv(0.02f, 0.9f, 0.0003f, 260, 0.3f), 0.8f, 0.07f);
        Mix(b, Sweep(0.22f, 900, 1300, 6, 0.6f, 0.2f), 0.12f, 0.16f);
        sBase[SFX_SHUTTER] = ToSound(b, 0.6f);
    }
    sBase[SFX_ALARM] = ToSound(Tones({659, 880, 659}, 0.15f, 9, 0.2f), 0.35f);
    sBase[SFX_CAUGHT] = ToSound(Tones({523, 440, 349}, 0.17f, 6, 0.35f), 0.5f);
    {
        // barattolo con i sassolini dentro
        auto b = Buf(0.8f);
        for (int c = 0; c < 16; c++) Mix(b, Ping(0.08f, aRng.range(1800, 3200), 60), aRng.range(0.3f, 0.9f), aRng.range(0, 0.6f));
        Mix(b, Ping(0.3f, 900, 14), 0.8f);
        sBase[SFX_RATTLE] = ToSound(b, 0.7f);
    }
    {
        auto b = Sweep(0.7f, 300, 2600, 3, 0.0f, 0.03f);
        Mix(b, Sweep(0.7f, 450, 3900, 3.5f, 0.0f, 0.0f), 0.4f);
        sBase[SFX_STEALTH] = ToSound(b, 0.5f);
    }
    {
        // chiave inglese e cricchetto
        auto b = Buf(1.2f);
        for (int c = 0; c < 10; c++) Mix(b, NoiseEnv(0.015f, 0.9f, 0.0003f, 250, 0.4f), 0.5f, 0.05f + c * 0.045f);
        Mix(b, Ping(0.4f, 1400, 12), 0.8f, 0.6f);
        Mix(b, Ping(0.4f, 1100, 12), 0.6f, 0.82f);
        Mix(b, Tones({880, 1175}, 0.08f, 18, 0.1f), 0.35f, 1.0f);
        sBase[SFX_REPAIR] = ToSound(b, 0.7f);
    }
    {
        auto b = Buf(0.9f);
        for (int c = 0; c < 4; c++) Mix(b, Ping(0.12f, 700 + c * 30, 40), 0.6f, c * 0.2f);
        sBase[SFX_CLIMB] = ToSound(b, 0.6f);
    }
    {
        auto b = Buf(0.32f);
        LP a(0.25f), c(0.04f);
        for (size_t i = 0; i < b.size(); i++) {
            float t = (float)i / SR;
            float n = Noise();
            b[i] = (a(n) - c(n)) * sinf(PI * t / 0.32f);
        }
        sBase[SFX_SWING] = ToSound(b, 0.6f);
    }
    {
        auto b = Buf(0.5f);
        for (int c = 0; c < 3; c++) Mix(b, NoiseEnv(0.07f, 0.35f, 0.02f, 30, 0.2f), 0.8f, c * 0.14f);
        sBase[SFX_SNIFF] = ToSound(b, 0.5f);
    }
    sBase[SFX_IMPACT] = ToSound(NoiseEnv(0.14f, 0.7f, 0.001f, 45, 0.15f), 0.6f);
    {
        auto b = Buf(0.5f);
        for (size_t i = 0; i < b.size(); i++) {
            float t = (float)i / SR;
            b[i] = (sinf(2 * PI * 1230 * t) + 0.7f * sinf(2 * PI * 1870 * t) + 0.5f * sinf(2 * PI * 2690 * t)) * expf(-t * 11) * 0.5f;
        }
        Mix(b, NoiseEnv(0.03f, 0.8f, 0.001f, 100), 0.8f);
        sBase[SFX_METALHIT] = ToSound(b, 0.7f);
        sBase[SFX_BOUNCE] = ToSound(b, 0.4f);
    }
    for (int k = 0; k < 4; k++) {
        auto b = NoiseEnv(0.14f, 0.12f + k * 0.03f, 0.003f, 32 + k * 4);
        for (int c = 0; c < 6; c++) Mix(b, NoiseEnv(0.01f, 0.6f, 0.0005f, 300), 0.3f, aRng.range(0, 0.06f));
        sBase[SFX_STEP1 + k] = ToSound(b, 0.6f);
    }
    sBase[SFX_GEIGER] = ToSound(NoiseEnv(0.012f, 0.9f, 0.0002f, 700), 0.8f);
    sBase[SFX_PICKUP] = ToSound(Tones({660, 990}, 0.07f, 25, 0.1f), 0.5f);
    sBase[SFX_UI_MOVE] = ToSound(Tones({1800}, 0.03f, 90, 0.5f), 0.25f);
    sBase[SFX_UI_OK] = ToSound(Tones({880, 1320}, 0.05f, 40, 0.3f), 0.35f);
    sBase[SFX_UI_BACK] = ToSound(Tones({440, 330}, 0.05f, 40, 0.3f), 0.35f);
    sBase[SFX_LEVELUP] = ToSound(Tones({523, 659, 784, 1046, 1318}, 0.13f, 5, 0.25f), 0.6f);
    {
        auto b = Tones({784, 1175}, 0.18f, 4, 0.0f);
        Mix(b, Tones({1568}, 0.0f, 6, 0.0f), 0.3f, 0.18f);
        sBase[SFX_QUEST] = ToSound(b, 0.55f);
    }
    sBase[SFX_DISCOVER] = ToSound(Tones({659, 880, 1318}, 0.09f, 9, 0.0f), 0.5f);
    {
        auto b = NoiseEnv(0.45f, 0.5f, 0.01f, 5, 0.2f);
        for (size_t i = 0; i < b.size(); i++) b[i] *= (sinf((float)i / SR * 2 * PI * 38) > 0 ? 1.0f : 0.2f);
        sBase[SFX_ROACH] = ToSound(b, 0.5f);
    }
    sBase[SFX_GROWL] = ToSound(Voice(0.6f, 95, 70, 0.15f, 0.6f, 18), 0.7f);
    sBase[SFX_GHOUL] = ToSound(Voice(0.7f, 420, 650, 0.5f, 0.8f, 11), 0.6f);
    sBase[SFX_ROBOT] = ToSound(Tones({1200, 900, 1500}, 0.08f, 25, 0.8f), 0.35f);
    sBase[SFX_ROAR] = ToSound(Voice(1.4f, 75, 55, 0.12f, 1.2f, 24));
    sBase[SFX_MUTANT] = ToSound(Voice(0.8f, 110, 85, 0.1f, 0.5f, 6), 0.8f);
    {
        auto b = Voice(0.18f, 520, 300, 0.4f, 0.5f);
        Mix(b, Voice(0.18f, 500, 280, 0.4f, 0.5f), 1.0f, 0.22f);
        sBase[SFX_DOG] = ToSound(b, 0.7f);
    }
    sBase[SFX_BARK] = ToSound(Voice(0.16f, 700, 460, 0.45f, 0.35f), 0.6f);
    {
        auto b = NoiseEnv(0.9f, 0.05f, 0.05f, 3);
        Mix(b, Sweep(0.6f, 280, 420, 4, 0.1f, 0), 0.15f, 0.1f);
        sBase[SFX_DOOR] = ToSound(b, 0.7f);
    }
    {
        auto b = Buf(4.0f);
        LP lp(0.03f);
        for (size_t i = 0; i < b.size(); i++) {
            float t = (float)i / SR;
            float env = std::min(1.0f, t * 2) * std::min(1.0f, (4.0f - t) * 1.5f);
            b[i] = (lp(Noise()) * 3 + sinf(2 * PI * 48 * t) * 0.4f + sinf(2 * PI * 97 * t) * 0.2f) * env;
        }
        for (int c = 0; c < 8; c++) Mix(b, NoiseEnv(0.06f, 0.5f, 0.001f, 40), 0.5f, 0.3f + c * 0.45f);
        Mix(b, NoiseEnv(1.2f, 0.6f, 0.2f, 2, 0.3f), 0.25f, 2.6f);
        sBase[SFX_VAULTDOOR] = ToSound(b);
    }
    sBase[SFX_PIN_TURN] = ToSound(NoiseEnv(0.08f, 0.9f, 0.001f, 40, 0.4f), 0.3f);
    {
        auto b = NoiseEnv(0.03f, 0.9f, 0.0005f, 120);
        Mix(b, Tones({3100}, 0.0f, 25, 0), 0.4f);
        sBase[SFX_PIN_BREAK] = ToSound(b, 0.6f);
    }
    {
        auto b = Sweep(0.12f, 180, 90, 30, 0.2f, 0.3f);
        Mix(b, Tones({820}, 0.0f, 20), 0.4f, 0.04f);
        sBase[SFX_UNLOCK] = ToSound(b, 0.7f);
    }
    sBase[SFX_TYPE] = ToSound(NoiseEnv(0.03f, 0.8f, 0.0005f, 160, 0.2f), 0.4f);
    sBase[SFX_TERM_OK] = ToSound(Tones({1000, 1500}, 0.09f, 20, 0.5f), 0.35f);
    sBase[SFX_TERM_FAIL] = ToSound(Tones({180, 150}, 0.12f, 8, 0.9f), 0.35f);
    sBase[SFX_SCAN_IN] = ToSound(Sweep(0.5f, 300, 1300, 3, 0.1f, 0.02f), 0.5f);
    sBase[SFX_SCAN_OUT] = ToSound(Sweep(0.4f, 1300, 280, 4, 0.1f, 0.02f), 0.5f);
    sBase[SFX_HURT] = ToSound(Tones({440, 349}, 0.10f, 8, 0.1f), 0.3f); // avviso ambientale, nessun grido
    sBase[SFX_FAINT] = ToSound(Sweep(1.3f, 600, 110, 1.6f, 0.0f, 0.02f), 0.5f);
    {
        auto b = NoiseEnv(0.45f, 0.9f, 0.02f, 6, 0.5f);
        Mix(b, Tones({1600}, 0.0f, 30), 0.2f, 0.35f);
        sBase[SFX_STIM] = ToSound(b, 0.5f);
    }
    {
        auto b = Buf(0.45f);
        for (int c = 0; c < 4; c++) Mix(b, NoiseEnv(0.07f, 0.25f, 0.005f, 40), 0.7f, c * 0.1f);
        sBase[SFX_EAT] = ToSound(b, 0.5f);
    }
    sBase[SFX_JUMP] = ToSound(NoiseEnv(0.12f, 0.3f, 0.02f, 25), 0.4f);
    sBase[SFX_LAND] = ToSound(Sweep(0.18f, 100, 50, 25, 0, 0.3f), 0.6f);
    {
        auto b = Buf(0.35f);
        for (int c = 0; c < 9; c++) Mix(b, NoiseEnv(0.01f, 0.9f, 0.0003f, 400), aRng.range(0.3f, 1.0f), aRng.range(0, 0.3f));
        sBase[SFX_SPARK] = ToSound(b, 0.5f);
    }
    {
        // russare: inspirazione rauca e lento sbuffo
        auto b = Buf(1.6f);
        LP lp(0.05f);
        for (size_t i = 0; i < b.size(); i++) {
            float t = (float)i / SR;
            float in = t < 0.9f ? sinf(PI * t / 0.9f) : 0.0f;
            float out = t >= 0.9f ? sinf(PI * (t - 0.9f) / 0.7f) * 0.5f : 0.0f;
            float rasp = 0.6f + 0.4f * sinf(2 * PI * 32 * t);
            b[i] = lp(Noise()) * 3 * (in * rasp + out) + sinf(2 * PI * 55 * t) * in * 0.3f;
        }
        sBase[SFX_SNORE] = ToSound(b, 0.6f);
    }
}

// ---------------------------------------------------------------------------
//  Flussi in loop (vento, ronzio, radio)
// ---------------------------------------------------------------------------
static void MakeLoop(LoopStream& L, std::vector<float> b, int crossfade) {
    // dissolvenza circolare tra fine e inizio per un loop senza scatti
    size_t n = b.size() - crossfade;
    for (int i = 0; i < crossfade; i++) {
        float k = (float)i / crossfade;
        b[i] = b[i] * k + b[n + i] * (1 - k);
    }
    b.resize(n);
    float peak = 0.001f;
    for (float v : b) peak = std::max(peak, fabsf(v));
    std::vector<unsigned char>& w = L.wav;
    unsigned int dataBytes = (unsigned int)(n * 2);
    w.resize(44 + dataBytes);
    auto W32 = [&](size_t off, unsigned int v) { memcpy(&w[off], &v, 4); };
    auto W16 = [&](size_t off, unsigned short v) { memcpy(&w[off], &v, 2); };
    memcpy(&w[0], "RIFF", 4);
    W32(4, 36 + dataBytes);
    memcpy(&w[8], "WAVEfmt ", 8);
    W32(16, 16);
    W16(20, 1);
    W16(22, 1);
    W32(24, SR);
    W32(28, SR * 2);
    W16(32, 2);
    W16(34, 16);
    memcpy(&w[36], "data", 4);
    W32(40, dataBytes);
    for (size_t i = 0; i < n; i++) {
        short s = (short)(Clamp(b[i] / peak * 0.9f, -1.f, 1.f) * 32000);
        memcpy(&w[44 + i * 2], &s, 2);
    }
    L.music = LoadMusicStreamFromMemory(".wav", w.data(), (int)w.size());
    L.music.looping = true;
    L.ok = L.music.stream.buffer != nullptr;
    if (L.ok) {
        SetMusicVolume(L.music, 0);
        PlayMusicStream(L.music);
    }
}

static std::vector<float> GenWind(float sec) {
    std::vector<float> b((size_t)(sec * SR));
    float brown = 0;
    LP lp(0.02f), lp2(0.004f);
    for (size_t i = 0; i < b.size(); i++) {
        float t = (float)i / SR;
        brown = brown * 0.995f + Noise() * 0.05f;
        float gust = 0.55f + 0.3f * sinf(t * 0.37f) + 0.15f * sinf(t * 1.13f + 1.0f);
        lp.a = 0.01f + 0.03f * gust;
        float v = lp(Noise()) * 6 + lp2(brown) * 0.5f;
        b[i] = v * gust;
    }
    return b;
}

static std::vector<float> GenHum(float sec) {
    std::vector<float> b((size_t)(sec * SR));
    LP lp(0.01f);
    for (size_t i = 0; i < b.size(); i++) {
        float t = (float)i / SR;
        b[i] = sinf(2 * PI * 60 * t) * 0.5f + sinf(2 * PI * 120 * t) * 0.25f + sinf(2 * PI * 180 * t) * 0.1f + lp(Noise()) * 0.8f;
    }
    return b;
}

// tono "pianoforte elettrico"
static void Note(std::vector<float>& b, float start, float dur, float freq, float amp, int timbre) {
    size_t s0 = (size_t)(start * SR), len = (size_t)((dur + 0.3f) * SR);
    float ph = 0;
    for (size_t i = 0; i < len && s0 + i < b.size(); i++) {
        float t = (float)i / SR;
        float env;
        float v;
        if (timbre == 0) { // piano
            env = expf(-t * 3.5f) * std::min(1.0f, t * 300);
            ph += freq / SR;
            v = sinf(2 * PI * ph) + 0.35f * sinf(4 * PI * ph) * expf(-t * 6) + 0.1f * sinf(6 * PI * ph);
        } else if (timbre == 1) { // basso
            env = expf(-t * 4) * std::min(1.0f, t * 200);
            ph += freq / SR;
            v = sinf(2 * PI * ph) + 0.25f * sinf(4 * PI * ph);
        } else { // clarinetto con vibrato
            float rel = t > dur ? expf(-(t - dur) * 20) : 1.0f;
            env = std::min(1.0f, t * 25) * rel * (0.85f + 0.15f * expf(-t * 3));
            ph += freq * (1 + 0.006f * sinf(t * 2 * PI * 5.5f) * std::min(1.0f, t * 3)) / SR;
            v = sinf(2 * PI * ph) + 0.3f * sinf(6 * PI * ph) + 0.12f * sinf(10 * PI * ph);
        }
        b[s0 + i] += v * env * amp;
    }
}

static std::vector<float> GenRadioMusic() {
    const float bpm = 116, beat = 60.0f / bpm;
    struct Ch { int root; int iv[4]; };
    const Ch C6{48, {0, 4, 7, 9}}, A7{45, {0, 4, 7, 10}}, Dm7{50, {0, 3, 7, 10}}, G7{43, {0, 4, 7, 10}}, E7{40, {0, 4, 7, 10}}, D7{50, {0, 4, 7, 10}}, F6{41, {0, 4, 7, 9}};
    std::vector<Ch> A = {C6, A7, Dm7, G7, C6, A7, Dm7, G7};
    std::vector<Ch> Bsec = {E7, E7, A7, A7, D7, D7, G7, G7};
    std::vector<Ch> Aend = {C6, A7, Dm7, G7, F6, G7, C6, C6};
    std::vector<Ch> song;
    for (auto& c : A) song.push_back(c);
    for (auto& c : A) song.push_back(c);
    for (auto& c : Bsec) song.push_back(c);
    for (auto& c : Aend) song.push_back(c);
    float total = song.size() * 4 * beat;
    std::vector<float> b((size_t)((total + 1.0f) * SR), 0.0f);
    Rng r(4242);
    // melodia della sezione A (riutilizzata)
    struct MN { float t, d; int m; };
    auto MakeMelody = [&](const std::vector<Ch>& sec, uint64_t seed) {
        Rng mr(seed);
        std::vector<MN> mel;
        int prev = 72;
        for (size_t bar = 0; bar < sec.size(); bar++) {
            const Ch& c = sec[bar];
            int pattern = mr.irange(0, 3);
            std::vector<std::pair<float, float>> rh;
            if (pattern == 0) rh = {{0, 1.5f}, {1.5f, 0.5f}, {2, 2}};
            else if (pattern == 1) rh = {{0, 0.66f}, {0.66f, 0.34f}, {1, 0.66f}, {1.66f, 0.34f}, {2, 1}, {3, 1}};
            else if (pattern == 2) rh = {{0, 1}, {1, 1}, {2, 0.66f}, {2.66f, 1.34f}};
            else rh = {{0.66f, 0.34f}, {1, 2}, {3, 1}};
            for (auto& p : rh) {
                int best = prev, bd = 99;
                for (int o = 60; o <= 84; o++) {
                    int pc = (o - c.root % 12 + 120) % 12;
                    bool tone = pc == c.iv[0] % 12 || pc == c.iv[1] % 12 || pc == c.iv[2] % 12 || pc == c.iv[3] % 12;
                    if (!tone) continue;
                    int d = abs(o - prev) + (mr.chance(0.5f) ? mr.irange(0, 4) : 0);
                    if (o == prev) d += 3;
                    if (d < bd) { bd = d; best = o; }
                }
                prev = Clamp(best, 64, 81);
                mel.push_back({bar * 4 + p.first, p.second, prev});
            }
        }
        return mel;
    };
    auto melA = MakeMelody(A, 11), melB = MakeMelody(Bsec, 23), melE = MakeMelody(Aend, 11);
    for (size_t bar = 0; bar < song.size(); bar++) {
        const Ch& c = song[bar];
        const Ch& nx = song[(bar + 1) % song.size()];
        float t0 = bar * 4 * beat;
        // basso camminante
        int walk[4] = {c.root, c.root + c.iv[1], c.root + c.iv[2], nx.root + (r.chance(0.5f) ? 1 : -1)};
        for (int k = 0; k < 4; k++) Note(b, t0 + k * beat, beat * 0.9f, MidiF((float)walk[k]), 0.32f, 1);
        // accordi sul 2 e sul 4
        for (int k : {1, 3})
            for (int v = 1; v < 4; v++) Note(b, t0 + k * beat + r.range(0, 0.01f), beat * 0.6f, MidiF((float)(c.root + 12 + c.iv[v])), 0.11f, 0);
        // spazzole: piatto ride swing
        for (int k = 0; k < 4; k++) {
            float ts[2] = {t0 + k * beat, t0 + (k + 0.66f) * beat};
            for (int h = 0; h < (k % 2 ? 2 : 1); h++) {
                size_t s0 = (size_t)(ts[h] * SR);
                LP hp(0.6f);
                for (size_t i = 0; i < (size_t)(0.18f * SR) && s0 + i < b.size(); i++) {
                    float n = Noise();
                    float v = n - hp(n);
                    b[s0 + i] += v * expf(-(float)i / SR * 22) * (h ? 0.05f : 0.08f);
                }
            }
            if (k % 2 == 0) Note(b, t0 + k * beat, 0.1f, 55, 0.18f, 1);
        }
    }
    auto PlaceMel = [&](const std::vector<MN>& mel, float barOff) {
        for (auto& m : mel) Note(b, (barOff * 4 + m.t) * beat, m.d * beat * 0.95f, MidiF((float)m.m), 0.16f, 2);
    };
    PlaceMel(melA, 0);
    PlaceMel(melA, 8);
    PlaceMel(melB, 16);
    PlaceMel(melE, 24);
    // filtro "radio d'epoca": passa-banda, saturazione, fruscio
    LP lpA(0.55f), lpB(0.08f);
    for (size_t i = 0; i < b.size(); i++) {
        float v = b[i];
        float lo = lpA(v);
        float hp = lo - lpB(lo);
        float crackle = aRng.chance(0.0006f) ? Noise() * 0.6f : 0;
        b[i] = tanhf(hp * 1.8f) * 0.8f + Noise() * 0.012f + crackle;
    }
    return b;
}

static std::vector<float> GenMorse() {
    const char* pattern = "... --- ...   ... --- ...       ";
    float unit = 0.09f;
    std::vector<float> b((size_t)(14.0f * SR), 0.0f);
    float t = 0.5f;
    for (const char* p = pattern; *p; p++) {
        float len = *p == '.' ? unit : (*p == '-' ? unit * 3 : 0);
        if (len > 0) {
            size_t s0 = (size_t)(t * SR);
            for (size_t i = 0; i < (size_t)(len * SR) && s0 + i < b.size(); i++) b[s0 + i] += sinf(2 * PI * 760 * (float)i / SR) * 0.5f;
            t += len + unit;
        } else t += unit * 2;
    }
    LP lp(0.3f);
    for (size_t i = 0; i < b.size(); i++) {
        float tt = (float)i / SR;
        b[i] += lp(Noise()) * (0.25f + 0.1f * sinf(tt * 0.7f));
    }
    return b;
}

// ---------------------------------------------------------------------------
//  API
// ---------------------------------------------------------------------------
void AudioInit() {
    if (!IsAudioDeviceReady()) return;
    BuildSfx();
    for (int i = 0; i < SFX_COUNT; i++) {
        for (int k = 0; k < POOL; k++) sPool[i][k] = LoadSoundAlias(sBase[i]);
        sNext[i] = 0;
    }
    MakeLoop(sWind, GenWind(18.0f), SR);
    MakeLoop(sHum, GenHum(6.0f), SR / 4);
    MakeLoop(sRadio, GenRadioMusic(), SR / 2);
    MakeLoop(sMorse, GenMorse(), SR / 4);
    sReady = true;
}

void AudioShutdown() {
    if (!sReady) return;
    for (int i = 0; i < SFX_COUNT; i++) {
        for (int k = 0; k < POOL; k++) UnloadSoundAlias(sPool[i][k]);
        UnloadSound(sBase[i]);
    }
    for (LoopStream* L : {&sWind, &sHum, &sRadio, &sMorse})
        if (L->ok) UnloadMusicStream(L->music);
    sReady = false;
}

void PlaySfx(SfxId id, float vol, float pitch) {
    if (!sReady || id < 0 || id >= SFX_COUNT) return;
    Sound& s = sPool[id][sNext[id]];
    sNext[id] = (sNext[id] + 1) % POOL;
    SetSoundVolume(s, Clamp(vol, 0.f, 1.f));
    SetSoundPitch(s, pitch);
    SetSoundPan(s, 0.5f);
    PlaySound(s);
}

void PlaySfx3D(SfxId id, Vector3 pos, float vol, float pitchVar) {
    if (!sReady) return;
    Vector3 to = pos - G.cam.position;
    float d = Vector3Length(to);
    if (d > 90) return;
    float att = 1.0f / (1.0f + d / 9.0f);
    if (d > 60) att *= (90 - d) / 30;
    Vector3 f = Vector3Normalize(G.cam.target - G.cam.position);
    Vector3 r = Vector3Normalize(Vector3CrossProduct(f, V3(0, 1, 0)));
    float side = d > 0.01f ? Vector3DotProduct(to / d, r) : 0;
    Sound& s = sPool[id][sNext[id]];
    sNext[id] = (sNext[id] + 1) % POOL;
    SetSoundVolume(s, Clamp(vol * att, 0.f, 1.f));
    SetSoundPitch(s, 1.0f + gRng.range(-pitchVar, pitchVar));
    SetSoundPan(s, 0.5f - side * 0.4f);
    PlaySound(s);
}

void SetRadioStation(int st) {
    gRadioStation = st;
    if (st == 1 && sRadio.ok) SeekMusicStream(sRadio.music, 0);
}

void AudioUpdate(float dt) {
    if (!sReady) return;
    bool playing = G.state != GS_MENU && G.state != GS_INTRO && G.state != GS_CREATE;
    bool outdoor = W.zones[G.player.zone].outdoor;
    sWind.target = (G.state == GS_MENU) ? 0.35f : (playing && outdoor ? 0.32f : 0.0f);
    sHum.target = playing && !outdoor ? 0.22f : 0.0f;
    float rv = G.settings.radioVolume;
    sRadio.target = (gRadioStation == 1 || G.state == GS_ENDING) ? rv * 0.8f : (G.state == GS_MENU ? rv * 0.5f : 0.0f);
    sMorse.target = gRadioStation == 2 && playing ? rv * 0.6f : 0.0f;
    for (LoopStream* L : {&sWind, &sHum, &sRadio, &sMorse}) {
        if (!L->ok) continue;
        L->vol += (L->target - L->vol) * Clamp01(dt * 2);
        SetMusicVolume(L->music, L->vol);
        if (L->vol > 0.001f) UpdateMusicStream(L->music);
    }
}
