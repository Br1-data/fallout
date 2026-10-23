// DUSTFALL - gioco di ruolo post-apocalittico in 3D
// Header condiviso: tipi, costanti e stato globale del gioco.
#pragma once

#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <functional>
#include <map>
#include <string>
#include <vector>

// ============================================================================
//  Utilità
// ============================================================================
struct Rng {
    uint64_t s;
    explicit Rng(uint64_t seed = 1) { s = seed * 0x9E3779B97F4A7C15ull + 0x632BE59BD9B4E019ull; if (!s) s = 1; next(); }
    uint32_t next() { s ^= s << 13; s ^= s >> 7; s ^= s << 17; return (uint32_t)(s >> 32); }
    float f() { return (next() >> 8) * (1.0f / 16777216.0f); }
    float range(float a, float b) { return a + (b - a) * f(); }
    int irange(int a, int b) { return a + (int)(next() % (uint32_t)(b - a + 1)); }
    bool chance(float p) { return f() < p; }
};
extern Rng gRng; // casualità di gioco (non deterministica)

float Hash2(int x, int z, int seed);
float ValueNoise(float x, float z, int seed);
float Fbm(float x, float z, int octaves, int seed);

inline float Clamp01(float v) { return v < 0 ? 0 : (v > 1 ? 1 : v); }
inline float SmoothStepF(float a, float b, float x) { float t = Clamp01((x - a) / (b - a)); return t * t * (3 - 2 * t); }
inline float LerpF(float a, float b, float t) { return a + (b - a) * t; }
inline Vector3 V3(float x, float y, float z) { return Vector3{x, y, z}; }
inline Vector2 V2(float x, float y) { return Vector2{x, y}; }
inline Color Col(int r, int g, int b, int a = 255) { return Color{(unsigned char)r, (unsigned char)g, (unsigned char)b, (unsigned char)a}; }
Color ColShade(Color c, float f);
Color ColLerp(Color a, Color b, float t);
Color ColJitter(Color c, Rng& r, float amount);
Color ColAlpha(Color c, float a);
inline Vector3 ColToV3(Color c) { return V3(c.r / 255.f, c.g / 255.f, c.b / 255.f); }
float AngleWrap(float a);
float AngleTo(Vector3 from, Vector3 to); // yaw (radianti) da 'from' verso 'to'
float DistXZ(Vector3 a, Vector3 b);
Vector3 YawDir(float yaw); // direzione orizzontale per un angolo yaw
std::string Fmt(const char* fmt, ...);
Matrix MatTRS(Vector3 t, Vector3 eulerRad, Vector3 s);
Matrix MatAlignY(Vector3 a, Vector3 b, float radius); // cilindro unitario da a a b

// ============================================================================
//  Costanti
// ============================================================================
constexpr float WORLD_HALF = 384.0f;
constexpr int TER_N = 256;
constexpr float TER_CELL = WORLD_HALF * 2.0f / TER_N;
constexpr int MAX_LIGHTS = 8;

enum ZoneId { ZONE_WASTE = 0, ZONE_VAULT = 1, ZONE_BASE = 2, ZONE_COUNT };

// ============================================================================
//  Rendering
// ============================================================================
struct MeshBuilder {
    std::vector<float> v, n;
    std::vector<unsigned char> c;
    void Tri(Vector3 a, Vector3 b, Vector3 cc, Color col);
    void TriN(Vector3 a, Vector3 b, Vector3 cc, Vector3 na, Vector3 nb, Vector3 nc, Color ca, Color cb, Color c3);
    void Quad(Vector3 a, Vector3 b, Vector3 cc, Vector3 d, Color col);
    void Box(const Matrix& m, Color col);                  // cubo unitario centrato
    void BoxAt(Vector3 center, Vector3 size, Color col, float yaw = 0);
    void Cylinder(const Matrix& m, int seg, Color col, bool caps = true, float topScale = 1.0f); // raggio 0.5, y da -0.5 a 0.5
    void Sphere(const Matrix& m, int rings, int segs, Color col);
    void Rock(const Matrix& m, Rng& r, Color col);
    void Torus(const Matrix& m, float R, float rr, int seg, int side, Color col);
    void Append(const MeshBuilder& o);
    size_t VertexCount() const { return v.size() / 3; }
    Mesh Build();
    void Clear() { v.clear(); n.clear(); c.clear(); }
};

struct LightSrc {
    Vector3 pos;
    Color color;
    float range;
    float intensity = 1.0f;
    float flicker = 0.0f; // 0 = stabile
    bool nightOnly = false;
};

enum Prim { PR_CUBE, PR_SPHERE, PR_CYL, PR_CONE, PR_COUNT };

struct Renderer {
    Shader lit{}, sky{};
    int locViewPos, locSunDir, locSunColor, locSkyAmb, locGroundAmb, locFogColor, locFogDensity, locEmissive;
    int locNumLights, locLightPos, locLightColor, locLightRange, locTime;
    int skLocSunDir, skLocZenith, skLocHorizon, skLocSunCol, skLocNight, skLocTime;
    Texture2D detail{}, softDot{}, vignette{}, noiseTex{};
    Model prim[PR_COUNT]{};
    Model skyDome{};
    Material litMat{};
    float curEmissive = -1.f;
    // parametri d'illuminazione correnti
    Vector3 sunDir{0, 1, 0}, sunColor{1, 1, 1}, skyAmb{0.3f, 0.3f, 0.3f}, groundAmb{0.2f, 0.2f, 0.2f}, fogColor{0.5f, 0.5f, 0.5f};
    Vector3 zenith{0.3f, 0.4f, 0.6f};
    float fogDensity = 0.004f, night = 0.f;
    float lightLevel = 1.f; // luminosità media (per cartelli e particelle)
};
extern Renderer R;

void RenderInit();
void RenderShutdown();
void RenderSetLights(const std::vector<LightSrc>& lights, Vector3 camPos, float time);
void RenderApplyFrameUniforms(Vector3 camPos, float time);
void DrawPrim(int prim, const Matrix& m, Color c, float emissive = 0.f);
void DrawPrimBox(Vector3 center, Vector3 size, float yaw, Color c, float emissive = 0.f);
void DrawLimb(const Matrix& parent, Vector3 a, Vector3 b, float radius, Color c, float emissive = 0.f);
void DrawSky(Camera3D cam);
void DrawGlowBillboard(Camera3D cam, Vector3 pos, float size, Color c);
void DrawBeam(Vector3 a, Vector3 b, float radius, Color c);

// ============================================================================
//  Oggetti e inventario
// ============================================================================
enum Special { ST_STR, ST_PER, ST_END, ST_CHA, ST_INT, ST_AGI, ST_LCK, ST_COUNT };
enum Skill { SK_SMALLGUNS, SK_ENERGY, SK_MELEE, SK_EXPLOSIVES, SK_MEDICINE, SK_LOCKPICK, SK_SCIENCE, SK_BARTER, SK_SPEECH, SK_SNEAK, SK_COUNT };
extern const char* SPECIAL_NAMES[ST_COUNT];
extern const char* SPECIAL_DESC[ST_COUNT];
extern const char* SKILL_NAMES[SK_COUNT];
extern const char* SKILL_DESC[SK_COUNT];
extern const int SKILL_GOV[SK_COUNT];

enum ItemType { IT_WEAPON, IT_ARMOR, IT_AID, IT_AMMO, IT_MISC, IT_KEY };

enum WeaponModel { WM_FIST, WM_KNIFE, WM_PIPE, WM_BAT, WM_SLEDGE, WM_PISTOL, WM_RIFLE, WM_SHOTGUN, WM_ASSAULT,
                   WM_LASERPISTOL, WM_LASERRIFLE, WM_PLASMA, WM_MINIGUN, WM_GRENADE };

enum SfxId {
    SFX_PISTOL, SFX_RIFLE, SFX_SHOTGUN, SFX_AUTO, SFX_LASER, SFX_PLASMA, SFX_MINIGUN, SFX_SWING, SFX_PUNCH,
    SFX_HIT, SFX_IMPACT, SFX_METALHIT, SFX_STEP1, SFX_STEP2, SFX_STEP3, SFX_STEP4, SFX_GEIGER, SFX_PICKUP,
    SFX_UI_MOVE, SFX_UI_OK, SFX_UI_BACK, SFX_LEVELUP, SFX_QUEST, SFX_EXPLOSION, SFX_RELOAD, SFX_EMPTY,
    SFX_ROACH, SFX_GROWL, SFX_GHOUL, SFX_ROBOT, SFX_ROAR, SFX_MUTANT, SFX_DOG, SFX_DOOR, SFX_VAULTDOOR,
    SFX_PIN_TURN, SFX_PIN_BREAK, SFX_UNLOCK, SFX_TYPE, SFX_TERM_OK, SFX_TERM_FAIL, SFX_VATS_IN, SFX_VATS_OUT,
    SFX_HURT, SFX_DEATH, SFX_STIM, SFX_EAT, SFX_JUMP, SFX_LAND, SFX_THROW, SFX_BOUNCE, SFX_SPARK,
    SFX_COUNT
};

struct WeaponDef {
    WeaponModel model;
    Skill skill;
    int ammo;          // id oggetto munizioni, -1 = nessuna
    int mag;
    float damage;
    float fireDelay;   // secondi tra un colpo e l'altro
    float spread;      // gradi
    int pellets;
    float range;
    float reload;
    int apCost;
    bool automatic;
    bool melee;
    bool energy;
    bool twoHanded;
    SfxId sfx;
    Color beam;        // colore raggio (armi a energia)
    float zoom;        // fattore zoom mirando
    float recoil;      // gradi
};

struct ItemDef {
    const char* name;
    ItemType type;
    float weight;
    int value;
    const char* desc;
    int weapon = -1;   // indice in WEAPONS
    float dr = 0, radRes = 0;
    int look = 0;      // aspetto armatura
    float heal = 0, rads = 0, radX = 0, ap = 0;
};

enum ItemId {
    I_CAPS,
    // armi
    I_KNIFE, I_PIPE, I_BAT, I_SLEDGE, I_PISTOL, I_HUNTRIFLE, I_SHOTGUN, I_ASSAULT, I_LASERPISTOL, I_LASERRIFLE, I_PLASMA, I_MINIGUN, I_GRENADE,
    // munizioni
    I_AMMO_10, I_AMMO_308, I_AMMO_SHELL, I_AMMO_556, I_AMMO_CELL, I_AMMO_MFC, I_AMMO_5MM,
    // armature
    I_VAULTSUIT, I_LEATHER, I_RAIDER, I_METAL, I_COMBAT, I_RADSUIT, I_POWER, I_COAT,
    // aiuti
    I_STIMPAK, I_RADAWAY, I_RADX, I_COLA, I_WATER, I_PUREWATER, I_MEAT, I_BEANS, I_CRAM, I_JET, I_MENTATS,
    // varie
    I_BOBBYPIN, I_SCRAP, I_TINCAN, I_TOASTER, I_TEDDY, I_CLOCK, I_BOTTLE, I_FUSE, I_CIRCUIT,
    // missione / chiavi
    I_MEDSUPPLIES, I_KEYCARD, I_FUSIONCELL, I_CLAW, I_CACHEKEY, I_SCOUTNOTE, I_SAFEKEY,
    I_COUNT
};

extern WeaponDef WEAPONS[];
extern ItemDef ITEMS[I_COUNT];
void ItemsInit();

struct ItemStack { int id; int count; };
struct Inventory {
    std::vector<ItemStack> items;
    int Count(int id) const;
    void Add(int id, int n = 1);
    bool Remove(int id, int n = 1);
    float Weight() const;
    bool Empty() const { return items.empty(); }
    void Clear() { items.clear(); }
};

// ============================================================================
//  Mondo
// ============================================================================
struct Box {
    Vector3 mn, mx;
    bool enabled = true;
    bool metal = false;
};
struct Cyl {
    float x, z, r, y0, y1;
    bool metal = false;
};

struct StaticChunk { Model model{}; BoundingBox bb{}; };

enum EmitterType { EM_FIRE, EM_SMOKE, EM_STEAM, EM_SPARKS, EM_RADGLOW, EM_DRIP };
struct Emitter { Vector3 pos; EmitterType type; float rate; float acc = 0; float size = 1; };

struct Sign {
    Vector3 pos;
    float yaw;
    float w, h;
    Texture2D tex{};
    bool emissive = false;
    Color tint = WHITE;
};

struct RadSource { Vector3 pos; float radius; float strength; };

struct Zone {
    int id = 0;
    std::string name;
    bool outdoor = true;
    std::vector<StaticChunk> chunks;
    std::vector<Box> boxes;
    std::vector<Cyl> cyls;
    std::vector<LightSrc> lights;
    std::vector<Emitter> emitters;
    std::vector<Sign> signs;
    std::vector<RadSource> rads;
    std::map<long long, MeshBuilder> builders;
    // griglia spaziale per i collider
    float gridMinX = 0, gridMinZ = 0, gridCell = 8;
    int gridW = 0, gridH = 0;
    std::vector<std::vector<int>> gridBoxes, gridCyls;
    // illuminazione per gli interni
    Vector3 ambSky{0.3f, 0.3f, 0.32f}, ambGround{0.2f, 0.2f, 0.22f}, fog{0.02f, 0.02f, 0.03f};
    float fogDensity = 0.02f;
    float floorY = 0;
    Vector3 bmin{-100, -10, -100}, bmax{100, 30, 100};

    MeshBuilder& B(Vector3 p);
    void AddBox(Vector3 center, Vector3 size, Color col, bool collide = true, float yaw = 0, bool metal = false);
    int AddCollider(Vector3 mn, Vector3 mx, bool metal = false);
    void AddCyl(float x, float z, float r, float y0, float y1, bool metal = false);
    void Finalize();
};

struct Terrain {
    std::vector<float> h;
    std::vector<Color> col;
    float H(int i, int j) const { return h[j * (TER_N + 1) + i]; }
    float Height(float x, float z) const;
    Vector3 Normal(float x, float z) const;
};

struct Location {
    std::string name;
    Vector3 pos;
    float radius;
    bool discovered = false;
    Vector3 travelPos;
    int icon = 0; // 0 generico, 1 città, 2 rifugio, 3 militare, 4 grotta, 5 rovine
};

struct RayHit {
    bool hit = false;
    float t = 1e9f;
    Vector3 point{}, normal{};
    int entity = -1;
    int part = 0;     // 0 torso, 1 testa, 2 gambe
    bool metal = false;
    bool terrain = false;
};

struct World {
    Zone zones[ZONE_COUNT];
    Terrain terrain;
    std::vector<Location> locations;
    Texture2D mapTex{};
    std::vector<std::vector<Vector3>> roads;
};
extern World W;

void WorldBuild();               // genera la geometria statica (una volta)
float GroundHeight(int zone, Vector3 p, float stepUp = 0.45f);
float TerrainHeight(float x, float z);
void ResolveCollision(int zone, Vector3& pos, float radius, float height, float stepUp = 0.45f);
bool CircleBlocked(int zone, Vector3 pos, float radius, float height, float stepUp = 0.45f);
bool RaycastWorld(int zone, Vector3 o, Vector3 dir, float maxT, RayHit& hit);
bool LineOfSight(int zone, Vector3 a, Vector3 b);
float RadiationAt(int zone, Vector3 p);
void DrawZoneStatic(int zone, Camera3D cam, float viewDist);
void DrawSigns(int zone, Camera3D cam);
int NearestLocation(Vector3 p, float* dist);

// ============================================================================
//  Oggetti interattivi (contenitori, porte, terminali...)
// ============================================================================
enum InterType { INT_CONTAINER, INT_DOOR, INT_TRANSITION, INT_TERMINAL, INT_BED, INT_ITEM, INT_REACTOR, INT_VAULTDOOR, INT_GATE };
enum ContModel { CM_CRATE, CM_AMMOBOX, CM_FIRSTAID, CM_FRIDGE, CM_DESK, CM_LOCKER, CM_SAFE, CM_FOOTLOCKER, CM_TOOLBOX, CM_TRASH, CM_NONE };

struct Interactable {
    InterType type;
    int zone;
    Vector3 pos;
    float yaw = 0;
    std::string name;
    std::string tag;
    int model = CM_NONE;
    int lock = 0;         // 0 nessuna, 1 facile(0), 2 media(25), 3 difficile(50), 4 molto diff.(75), 5 solo chiave
    int keyItem = -1;
    bool locked = false;
    Inventory inv;
    int targetZone = 0;
    Vector3 targetPos{};
    float targetYaw = 0;
    bool open = false;
    float anim = 0;
    int collider = -1;    // indice Box nella zona (porte)
    Vector3 size{1, 1, 1};
    int terminal = -1;    // contenuto terminale
    int hackLevel = 0;    // 0 nessuna protezione
    bool hacked = false;
    bool lockedOut = false;
    bool used = false;    // oggetti a terra raccolti, ecc.
    bool discovered = true;
};

// ============================================================================
//  Entità (PNG e creature)
// ============================================================================
enum EntKind { EK_HUMAN, EK_ROACH, EK_MOLERAT, EK_DOG, EK_GHOUL, EK_MUTANT, EK_ROBOT, EK_TURRET, EK_DEATHCLAW, EK_COUNT };
enum Faction { FAC_TOWN, FAC_VAULT, FAC_RAIDER, FAC_CREATURE, FAC_MUTANT, FAC_ROBOT };
enum Outfit { OF_SETTLER, OF_VAULT, OF_RAIDER, OF_SHERIFF, OF_DOCTOR, OF_GUARD, OF_TRADER, OF_GHOUL, OF_MUTANT, OF_LEATHER, OF_COMBAT, OF_POWER, OF_RADSUIT, OF_BARMAN };
enum AiState { AI_IDLE, AI_WANDER, AI_COMBAT, AI_FLEE, AI_SEARCH, AI_RETURN };

struct Look {
    Color skin = Col(205, 160, 130);
    Color shirt = Col(120, 100, 80);
    Color pants = Col(80, 70, 60);
    Color hair = Col(60, 45, 35);
    int hairStyle = 1;  // 0 calvo, 1 corti, 2 lunghi, 3 cresta
    int hat = 0;        // 0 nessuno, 1 cappello, 2 elmetto, 3 bandana, 4 casco atomico
    int outfit = OF_SETTLER;
    bool female = false;
};

struct Entity {
    int id = 0;
    EntKind kind = EK_HUMAN;
    int zone = ZONE_WASTE;
    std::string name;
    std::string tag;
    std::string dlg;
    Faction fac = FAC_TOWN;
    bool hostile = false;
    bool essential = false;
    Vector3 pos{}, home{}, vel{};
    float yaw = 0;
    float hp = 50, maxHp = 50, dr = 0;
    float speed = 3.5f;
    float scale = 1.0f;
    int weapon = -1;          // oggetto arma (umanoidi)
    float meleeDmg = 5;
    float accuracy = 0.6f;
    float detect = 25;
    float wanderRadius = 8;
    int xp = 10;
    int vendor = -1;          // indice negozio
    bool glow = false;
    float radAura = 0;
    // stato IA
    AiState ai = AI_IDLE;
    float aiTimer = 0;
    float alert = 0;
    bool aware = false;
    Vector3 moveTarget{};
    Vector3 lastSeen{};
    float attackCd = 0;
    float attackAnim = 0;
    bool attackHit = false;
    float losTimer = 0;
    bool los = false;
    float strafe = 1;
    float strafeTimer = 0;
    int mag = 0;
    float reloadT = 0;
    float burst = 0;
    float voiceCd = 0;
    // animazione
    float anim = 0, moveAmt = 0, hitFlash = 0, deathT = 0, aimBlend = 0;
    bool dead = false;
    float cripple = 0;
    float stateFlee = 0;
    float talkT = 0;
    Inventory inv;
    Look look;
    std::vector<Vector3> patrol;
    int patrolIdx = 0;
};

// ============================================================================
//  Giocatore
// ============================================================================
enum Perk {
    PK_TOUGHNESS, PK_GUNSLINGER, PK_COMMANDO, PK_IRONFIST, PK_MEDIC, PK_LEADBELLY, PK_RADRES, PK_EDUCATED,
    PK_STRONGBACK, PK_ACTIONBOY, PK_SNIPER, PK_FINESSE, PK_BLOODYMESS, PK_FORTUNE, PK_LASER, PK_FASTRELOAD,
    PK_LIFEGIVER, PK_NINJA, PK_REGEN, PK_INFILTRATOR, PK_HACKER, PK_DEMOLITION, PK_COUNT
};
struct PerkDef { const char* name; const char* desc; int level; int req; int reqVal; };
extern PerkDef PERKS[PK_COUNT];

struct Player {
    std::string name = "Viandante";
    bool female = false;
    int special[ST_COUNT] = {5, 5, 5, 5, 5, 5, 5};
    int skillPts[SK_COUNT] = {};
    bool tagged[SK_COUNT] = {};
    bool perks[PK_COUNT] = {};
    int level = 1, xp = 0;
    int pendingLevels = 0;
    float hp = 100, ap = 80, rads = 0;
    float radXTimer = 0, jetTimer = 0, mentatsTimer = 0;
    float healOverTime = 0;
    Inventory inv;
    int weapon = -1;     // oggetto arma impugnata (-1 = pugni)
    int armor = I_VAULTSUIT;
    int mag = 0;
    int karma = 0;
    // fisica
    int zone = ZONE_VAULT;
    Vector3 pos{}, vel{};
    float yaw = 0, pitch = 0;
    bool onGround = true;
    bool crouch = false;
    bool sprint = false;
    float eyeH = 1.65f;
    float fallStartY = 0;
    // combattimento
    float fireCd = 0, reloadT = 0, equipT = 0, swingT = 0, recoil = 0, flash = 0;
    bool swingDone = true;
    float bob = 0, stepDist = 0, aimT = 0;
    float hurtFlash = 0, hurtDir = 0;
    bool pipLight = false;
    bool thirdPerson = false;
    float lastCombat = 100;
    // statistiche
    int kills = 0, locksPicked = 0, termsHacked = 0, questsDone = 0, capsEarned = 0, stimpaksUsed = 0, shots = 0;
    float playTime = 0;
};

int SkillValue(Skill s);
float MaxHP();
float MaxAP();
float CarryMax();
float PlayerDR();
float PlayerRadRes();
int XPForLevel(int lvl);
void GiveXP(int amount, const char* reason = nullptr);
void GiveItem(int id, int count = 1, bool notify = true);
void TakeItem(int id, int count = 1);

// ============================================================================
//  Missioni
// ============================================================================
enum QuestId { Q_MAIN, Q_ROACH, Q_MEDS, Q_BEAST, Q_SIGNAL, Q_COUNT };
struct Quest { const char* title; int stage = 0; bool done = false; };
struct QuestObjective { std::string text; int zone = -1; Vector3 pos{}; bool hasPos = false; };
extern Quest QUESTS[Q_COUNT];
void SetQuestStage(int q, int stage);
void CompleteQuest(int q);
std::vector<QuestObjective> QuestObjectives(int q);
std::string QuestDescription(int q);
void QuestsUpdate(float dt);
void QuestsOnKill(Entity& e);
void QuestsOnPickup(int item);
void QuestsOnEnterZone(int zone);

// ============================================================================
//  Dialoghi
// ============================================================================
struct DlgOption { std::string text; std::function<void()> act; bool enabled = true; };
struct Dialogue {
    int npc = -1;
    int node = 0;
    int next = 0;
    bool end = false;
    std::string speaker;
    std::string text;
    std::vector<DlgOption> opts;
    int sel = 0;
    float typeT = 0;
};
void DialogueStart(int npc);
void DialogueBuild();

// ============================================================================
//  Effetti
// ============================================================================
struct Particle {
    Vector3 pos, vel;
    float life, maxLife, size, grow;
    Color col;
    float gravity;
    bool additive;
    int zone;
};
struct Tracer { Vector3 a, b; float t, maxT; Color col; float width; int zone; };
struct Grenade { Vector3 pos, vel; float fuse; int zone; bool player; float dmg; };
struct DynLight { Vector3 pos; Color col; float range; float t; int zone; };

void FxUpdate(float dt);
void FxDraw(Camera3D cam);
void SpawnParticle(int zone, Vector3 p, Vector3 v, float life, float size, Color c, float gravity, bool additive, float grow = 0);
void SpawnBlood(int zone, Vector3 p, Vector3 dir, Color c, int n);
void SpawnImpact(int zone, Vector3 p, Vector3 n, bool metal);
void SpawnExplosion(int zone, Vector3 p, float radius, float dmg, bool byPlayer);
void SpawnTracer(int zone, Vector3 a, Vector3 b, Color c, float width, float life);
void SpawnDynLight(int zone, Vector3 p, Color c, float range, float t);
void ThrowGrenade(int zone, Vector3 p, Vector3 v, bool player);
void EmittersUpdate(int zone, Vector3 camPos, float dt);

// ============================================================================
//  Audio
// ============================================================================
void AudioInit();
void AudioShutdown();
void AudioUpdate(float dt);
void PlaySfx(SfxId id, float vol = 1.0f, float pitch = 1.0f);
void PlaySfx3D(SfxId id, Vector3 pos, float vol = 1.0f, float pitchVar = 0.08f);
void SetRadioStation(int st); // 0 spenta, 1 musica, 2 segnale
extern int gRadioStation;

// ============================================================================
//  Interfaccia
// ============================================================================
void UIInit();
void UIShutdown();
float UIS();      // scala UI (altezza/720)
float UIW();      // larghezza virtuale
void UIText(const std::string& s, float x, float y, float size, Color c, int align = 0, bool term = false);
Vector2 UIMeasure(const std::string& s, float size, bool term = false);
void UIRect(float x, float y, float w, float h, Color c);
void UIRectLines(float x, float y, float w, float h, float thick, Color c);
bool UIHover(float x, float y, float w, float h);
bool UIClicked(float x, float y, float w, float h);
std::vector<std::string> UIWrap(const std::string& s, float size, float width, bool term = false);
void Notify(const std::string& msg, Color c = Color{120, 255, 150, 255});
void BigNotify(const std::string& msg, const std::string& sub = "");

extern const Color PIP;
extern const Color PIP_DIM;
extern const Color PIP_DARK;
extern const Color PIP_BG;

// ============================================================================
//  Stato del gioco
// ============================================================================
enum GameState {
    GS_MENU, GS_INTRO, GS_CREATE, GS_PLAY, GS_PIPBOY, GS_DIALOGUE, GS_BARTER, GS_LOOT, GS_LOCKPICK,
    GS_TERMINAL, GS_VATS, GS_PAUSE, GS_LEVELUP, GS_DEAD, GS_ENDING, GS_SLEEP, GS_SAVELOAD, GS_OPTIONS
};

struct Settings {
    float sensitivity = 1.0f;
    float volume = 0.8f;
    float radioVolume = 0.6f;
    float fov = 75.0f;
    bool invertY = false;
    bool showFps = false;
    bool fullscreen = false;
};

struct Message { std::string text; Color col; float t; };

struct VatsTarget { int ent; float chance[3]; };
struct VatsShot { int ent; int part; };

struct Vendor { std::string name; Inventory inv; int caps; };

struct Game {
    GameState state = GS_MENU;
    GameState prevState = GS_PLAY;
    Player player;
    std::vector<Entity> ents;
    std::vector<Interactable> inters;
    std::vector<Vendor> vendors;
    std::map<std::string, int> flags;
    float timeOfDay = 9.0f; // ore
    int day = 1;
    float time = 0;         // secondi reali
    float timeScale = 1.0f;
    Camera3D cam{};
    Settings settings;
    std::vector<Message> messages;
    std::string bigMsg, bigSub;
    float bigMsgT = 0;
    Dialogue dlg;
    // interazione
    int lookEnt = -1, lookInter = -1;
    int lootInter = -1, lootEnt = -1;
    int barterVendor = -1, barterNpc = -1;
    int activeTerminal = -1;
    // VATS
    std::vector<VatsTarget> vatsTargets;
    std::vector<VatsShot> vatsQueue;
    int vatsSel = 0, vatsPart = 0;
    bool vatsExec = false;
    float vatsT = 0;
    // transizioni
    float fade = 0;
    int pendingZone = -1;
    Vector3 pendingPos{};
    float pendingYaw = 0;
    bool inCombat = false;
    float geigerAcc = 0;
    float radRate = 0;
    float sleepHours = 0;
    int endingSlide = 0;
    float endingT = 0;
    bool gameStarted = false;
    bool quit = false;
    float autosaveT = 0;
    int pipTab = 0, pipSub = 0, pipSel = 0;
    float pipScroll = 0;
};
extern Game G;

// --- funzioni di gioco (definite nei vari moduli) ---
void NewGame();
void ResetDynamicWorld();
void ChangeZone(int zone, Vector3 pos, float yaw);
void PlayerUpdate(float dt);
void PlayerFire();
void PlayerReload();
void EquipWeapon(int item);
void EquipArmor(int item);
void UseItem(int item);
void PlayerTakeDamage(float dmg, Vector3 from, bool radiation = false);
void PlayerKilledEntity(Entity& e);
void UpdateCamera3D();
void DrawViewModel();
void DrawPlayerBody();
int CurrentWeaponDefIndex();
const WeaponDef& CurrentWeapon();
bool HostilesNear(float range);
void AlertNearby(int zone, Vector3 p, float radius);

void EntitiesUpdate(float dt);
void EntitiesDraw(Camera3D cam);
void DrawEntity(const Entity& e);
void DamageEntity(Entity& e, float dmg, int part, Vector3 dir, bool byPlayer, bool crit = false);
bool RaycastEntities(int zone, Vector3 o, Vector3 dir, float maxT, RayHit& hit, int ignore = -1);
Vector3 EntityPartPos(const Entity& e, int part);
const char* EntityPartName(const Entity& e, int part);
void MakeEntityHostile(Entity& e);
Entity* FindEntityByTag(const std::string& tag);
Interactable* FindInterByTag(const std::string& tag);
void DrawHumanoid(Vector3 pos, float yaw, const Look& look, float anim, float moveAmt, float aim, float attack, float deathT, int weaponModel, float flash, float scale, bool glow, float crouch = 0);

void InteractablesDraw(Camera3D cam);
void InteractablesUpdate(float dt);
void Interact(int idx);
bool TryUnlock(Interactable& it);

void VatsEnter();
void VatsUpdate(float dt);
void VatsDraw();
float VatsChance(const Entity& e, int part);

void HudDraw();
void PipBoyUpdate();
void PipBoyDraw();
void DialogueUpdate();
void DialogueDraw();
void BarterOpen(int vendor, int npc);
void BarterUpdate();
void BarterDraw();
void LootOpen(int inter, int ent);
void LootUpdate();
void LootDraw();
void LockpickStart(int inter);
void LockpickUpdate(float dt);
void LockpickDraw();
void TerminalStart(int inter);
void TerminalUpdate(float dt);
void TerminalDraw();
void LevelUpOpen();
void LevelUpUpdate();
void LevelUpDraw();
void MenuUpdate();
void MenuDraw();
void IntroUpdate(float dt);
void IntroDraw();
void CreateUpdate();
void CreateDraw();
void PauseUpdate();
void PauseDraw();
void DeadUpdate();
void DeadDraw();
void EndingStart();
void EndingUpdate(float dt);
void EndingDraw();
void SleepOpen();
void SleepUpdate(float dt);
void SleepDraw();
void SaveLoadOpen(bool saving, GameState back);
void SaveLoadUpdate();
void SaveLoadDraw();
void OptionsOpen(GameState back);
void OptionsUpdate();
void OptionsDraw();
void SetGameState(GameState s);

bool SaveGame(int slot);
bool LoadGame(int slot);
bool SaveExists(int slot);
std::string SaveInfo(int slot);
void SaveSettings();
void LoadSettings();
void ApplySettings();

// --- costruzione del mondo / modelli dinamici ---
Entity CreateEntity(EntKind k, int zone, Vector3 pos, float yaw = 0);
void GenContainerLoot(Inventory& inv, int model, Rng& r, int tier);
Font& UIFontFor(float px, bool term);
extern std::vector<Entity> gEntTemplates;
extern std::vector<Interactable> gInterTemplates;
extern std::vector<Vendor> gVendorTemplates;

struct TermEntry { std::string title, text, action; };
struct TerminalDef { std::string header; std::vector<TermEntry> entries; };
extern std::vector<TerminalDef> TERMINALS;
void InitTerminals();
enum TerminalIds { TERM_OVERSEER, TERM_GATE, TERM_ARMORY, TERM_LAB, TERM_RAIDER, TERM_CLINIC, TERM_COUNT };
extern const int WEAPON_FIST;
void DrawWeaponModel(const Matrix& m, int wm, float flash, float spin = 0);
Color OutfitColor(int outfit);
float PlayerSpeedMul();
int ArmorOutfit();
Vector3 PlayerEye();
Vector3 PlayerChest();
float EntityReach(const Entity& e);
float EntityRadius(const Entity& e);
void EntitiesDrawShadows(Camera3D cam);
int Spec(int st);
Vector3 PlayerForward();
float HPCap();
void PlayerMeleeHit();
void FireRay(Vector3 o, Vector3 dir, const WeaponDef& w, float dmg, Vector3 muzzle, int vatsTarget = -1, int vatsPart = -1);
Vector3 MuzzleWorld();
float WeaponDamage(const WeaponDef& w);
void FxCollectLights(int zone, std::vector<LightSrc>& out);
void FxClear();
void LocationsUpdate();
