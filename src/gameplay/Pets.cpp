// Pets and their wild kin (M26.1; wiki: Wolf, Cat, Ocelot, Parrot, Taming). Part of
// Mobs: taming by feeding, sitting, following the player (teleporting when left far
// behind), tamed wolves fighting the player's fights, wild wolves hunting sheep,
// parrots dancing to jukeboxes; the natural spawning of animals (wolves, ocelots,
// parrots by biome) and of village cats.
#include "gameplay/Mobs.h"

#include "gameplay/Raids.h"

#include "world/Blocks.h"
#include "world/Items.h"
#include "world/Weather.h"

#include <algorithm>
#include <cmath>

namespace mc {

using namespace world;

namespace {

const ItemId& itemNamed(const char* name, ItemId& slot) {
    if (slot == 0) slot = itemRegistry().find(name).value_or(kNoItem);
    return slot;
}
bool solid(const World& w, int x, int y, int z) {
    return blockRegistry().collides(w.getBlock({x, y, z}));
}

} // namespace

float Mobs::petMaxHealth(const MobData& m) { return maxHealthOf(m); }

// Right-clicking a pet or a wild one with its taming food (wiki: Taming): wolves take
// bones, cats raw cod or salmon, parrots seeds, ocelots fish (trust) - 1 in 3 tames it,
// sitting. Tamed: food heals (wolves: meat) or breeds, dye recolours the collar, anything
// else makes it sit or stand.
Mobs::Use Mobs::petInteract(MobData& m, ItemId held, Xoroshiro& rng) {
    static ItemId bone, cod, salmon, wheatSeeds, beetSeeds;
    const auto& items = itemRegistry();
    const bool fish =
        held != kNoItem && (held == itemNamed("cod", cod) || held == itemNamed("salmon", salmon));
    const bool seeds = held != kNoItem && (held == itemNamed("wheat_seeds", wheatSeeds) ||
                                           held == itemNamed("beetroot_seeds", beetSeeds));
    const bool tameFood = (m.type == MobType::Wolf && held == itemNamed("bone", bone)) ||
                          ((m.type == MobType::Cat || m.type == MobType::Ocelot) && fish) ||
                          (m.type == MobType::Parrot && seeds);
    if (!m.tamed) {
        if (!tameFood || m.angry) return Use::None;
        // (wiki: wolves, cats and ocelots 1 in 3; parrots 1 in 10 per seed)
        if (rng.nextInt(m.type == MobType::Parrot ? 10 : 3) == 0) {
            m.tamed = true;
            m.sitting = m.type != MobType::Ocelot; // (ocelots only trust)
            m.health = petMaxHealth(m);
            m.persistent = true;
            m.targetUuid = 0;
            m.goal = m.pos;
        }
        return Use::Fed; // (the food is used either way)
    }
    if (m.type == MobType::Ocelot) return Use::None;
    // Wolf armor (M26.3; wiki: Wolf Armor) on a tamed wolf; shears take it off again.
    static ItemId wolfArmor;
    if (m.type == MobType::Wolf && held != kNoItem && held == itemNamed("wolf_armor", wolfArmor) &&
        m.horseArmor == 0 && !m.isBaby()) {
        m.horseArmor = 1;
        m.armorWear = 0;
        return Use::Fed;
    }
    // A dye on a tamed wolf or cat: its collar's colour.
    if (held != kNoItem && m.type != MobType::Parrot) {
        const std::string_view id = items.item(held).id;
        for (int c = 0; c < 16; ++c)
            if (id.size() > 10 && id.substr(10) == std::string(kDyeColours[c]) + "_dye") {
                if (m.color2 == c) return Use::None;
                m.color2 = uint8_t(c);
                return Use::Fed;
            }
    }
    // Food: heals a hurt pet (wolves: any meat), else puts it in love (Animals.cpp).
    const ItemDef& def = items.item(held);
    const bool meat =
        held != kNoItem && def.food > 0 &&
        (def.id.find("beef") != std::string::npos || def.id.find("porkchop") != std::string::npos ||
         def.id.find("chicken") != std::string::npos ||
         def.id.find("mutton") != std::string::npos || def.id.find("rabbit") != std::string::npos ||
         def.id == "minecraft:rotten_flesh");
    if ((m.type == MobType::Wolf && meat) || (m.type == MobType::Cat && fish)) {
        if (m.health < petMaxHealth(m)) {
            m.health = std::min(petMaxHealth(m), m.health + float(std::max(2, def.food)));
            return Use::Fed;
        }
        if (m.age == 0 && m.loveTicks == 0) {
            m.loveTicks = 600;
            return Use::Fed;
        }
        return Use::None;
    }
    m.sitting = !m.sitting; // (sit / stand)
    m.goal = m.pos;
    m.targetUuid = 0;
    return Use::Sat;
}

bool Mobs::petGoal(Context& ctx, MobData& m, double& speed) {
    const glm::dvec3 p = ctx.player.position();
    const double d = glm::length(p - m.pos);
    // Parrots dance within 3 blocks of a playing jukebox (wiki: Parrot › Dancing).
    if (m.type == MobType::Parrot &&
        (m.peek > 0 || ctx.rng.nextInt(20) == 0)) { // (looked for about once a second)
        const ChunkPos c{blockToChunk(int(std::floor(m.pos.x))),
                         blockToChunk(int(std::floor(m.pos.z)))};
        bool music = false;
        for (int dz = -1; dz <= 1 && !music; ++dz)
            for (int dx = -1; dx <= 1 && !music; ++dx)
                if (const Chunk* ch = ctx.world.chunk({c.x + dx, c.z + dz}))
                    for (const auto& j : ch->jukeboxes())
                        if (j.data.playing &&
                            glm::length(glm::dvec3((c.x + dx) * 16 + j.x + 0.5, j.y + 0.5,
                                                   (c.z + dz) * 16 + j.z + 0.5) -
                                        m.pos) < 3.5)
                            music = true;
        m.peek = music ? 1 : 0;
        if (music) {
            m.goal = m.pos;
            m.yaw += 18.0f; // (the dance: turning on the spot)
            m.limbSwingAmount = 1.0f;
            return true;
        }
    }
    // Ocelots that don't trust the player keep away from them (wiki: Ocelot).
    if (m.type == MobType::Ocelot && !m.tamed && d < 6.0) {
        const glm::dvec3 away = d > 1e-6 ? (m.pos - p) / d : glm::dvec3(1, 0, 0);
        m.goal = m.pos + away * 6.0;
        speed *= 1.5;
        return true;
    }
    if (m.type == MobType::Ocelot) return false;
    // Wild wolves now and then hunt a sheep within 16 blocks (wiki: Wolf › Behavior).
    if (m.type == MobType::Wolf && !m.tamed && !m.angry && !m.isBaby() && m.targetUuid == 0 &&
        ctx.rng.nextInt(400) == 0) {
        double best = 16.0 * 16.0;
        const ChunkPos c{blockToChunk(int(std::floor(m.pos.x))),
                         blockToChunk(int(std::floor(m.pos.z)))};
        for (int dz = -1; dz <= 1; ++dz)
            for (int dx = -1; dx <= 1; ++dx)
                if (Chunk* ch = ctx.world.chunk({c.x + dx, c.z + dz}))
                    for (MobData& o : ch->mobs())
                        if (o.type == MobType::Sheep && o.health > 0.0f &&
                            glm::dot(o.pos - m.pos, o.pos - m.pos) < best) {
                            best = glm::dot(o.pos - m.pos, o.pos - m.pos);
                            m.targetUuid = o.uuidHi;
                        }
    }
    if (m.tamed && m.sitting) {
        m.goal = m.pos;
        m.vel.x = m.vel.z = 0.0;
        return true;
    }
    // A tamed wolf takes up the player's fights: the mob the player last hit, or the one
    // that last hurt the player (never creepers or other pets - wiki: Wolf).
    if (m.type == MobType::Wolf && m.tamed && m.targetUuid == 0) {
        for (const uint64_t want : {ctx.playerAttackerUuid, ctx.playerTargetUuid})
            if (want != 0 && want != m.uuidHi)
                if (MobData* t = mobByUuid(ctx.world, m.pos, want);
                    t && t->health > 0.0f && t->type != MobType::Creeper &&
                    !(isPet(t->type) && t->tamed) && glm::length(t->pos - m.pos) < 24.0) {
                    m.targetUuid = want;
                    break;
                }
    }
    if (m.type == MobType::Wolf && m.targetUuid != 0) {
        MobData* t = mobByUuid(ctx.world, m.pos, m.targetUuid);
        if (!t || t->health <= 0.0f || glm::length(t->pos - m.pos) > 24.0) {
            m.targetUuid = 0;
        } else {
            m.goal = t->pos;
            speed *= 1.3;
            if (m.attackCooldown > 0) --m.attackCooldown;
            if (m.attackCooldown == 0 &&
                glm::length(glm::dvec2(t->pos.x - m.pos.x, t->pos.z - m.pos.z)) <
                    mobInfo(t->type).width * 0.5 + 0.9) {
                t->health -= mobInfo(MobType::Wolf).attackDamage;
                t->hurtTime = 10;
                if (m.tamed)
                    t->lastHurtByPlayer =
                        true; // (a pet's kill counts as the player's: experience, loot)
                if (!mobInfo(t->type).hostile) t->panicTicks = 100;
                m.attackCooldown = 20;
            }
            return true;
        }
    }
    if (!m.tamed) return false;
    // Following: teleport to the player when 12 or more blocks away (onto solid ground
    // beside them), walk after them past 6, stop within 2 (wiki: Taming › Following).
    if (d >= 12.0) {
        for (int k = 0; k < 10; ++k) {
            const int x = int(std::floor(p.x)) + int(ctx.rng.nextInt(5)) - 2,
                      z = int(std::floor(p.z)) + int(ctx.rng.nextInt(5)) - 2;
            const int y = int(std::floor(p.y));
            if (solid(ctx.world, x, y - 1, z) && !solid(ctx.world, x, y, z) &&
                !solid(ctx.world, x, y + 1, z)) {
                m.pos = m.prevPos = m.goal = {x + 0.5, double(y), z + 0.5};
                m.vel = glm::dvec3(0.0);
                break;
            }
        }
        return true;
    }
    if (d > 6.0) {
        m.goal = p;
        speed *= 1.3;
        return true;
    }
    if (d < 2.0) m.goal = m.pos;
    return false; // (near the player: love, tempting and strolls as any animal)
}

void Mobs::spawnCreatures(Context& ctx) {
    // Animals keep appearing now and then (wiki: Spawn › creature category: every 400
    // ticks, on grass in light 9+, at most 10 around the player): wolves in forests and
    // taigas (packs of 4, the variant by biome), ocelots and parrots in jungles. Village
    // cats (wiki: Cat › Spawning): every 1200 ticks, up to 5 near a bell.
    if (++m_creatureTicks % 400 != 0) return;
    const glm::dvec3 p = ctx.player.position();
    if (m_creatureTicks % 1200 == 0 && m_cats < 5) {
        if (const auto bell = findBell(ctx.world, p, 48)) {
            const int x = bell->x + int(ctx.rng.nextInt(17)) - 8,
                      z = bell->z + int(ctx.rng.nextInt(17)) - 8;
            for (int y = bell->y + 6; y >= bell->y - 6; --y)
                if (solid(ctx.world, x, y - 1, z) && !solid(ctx.world, x, y, z) &&
                    !solid(ctx.world, x, y + 1, z)) {
                    MobData cat = make(MobType::Cat, {x + 0.5, double(y), z + 0.5}, ctx.rng);
                    if (add(ctx.world, cat)) ++m_cats;
                    break;
                }
        }
    }
    // (M32.1; vanilla's creature category: every 400 ticks each chunk within 8 tries once,
    // the cap 10 scaled by those chunks as for monsters; animals stand on the surface)
    const ChunkPos pc{blockToChunk(int(std::floor(p.x))), blockToChunk(int(std::floor(p.z)))};
    int eligible = 0;
    for (int dz = -8; dz <= 8; ++dz)
        for (int dx = -8; dx <= 8; ++dx)
            if (const Chunk* c = ctx.world.chunk({pc.x + dx, pc.z + dz}); c && c->lit()) ++eligible;
    m_creatureCap = std::max(1, 10 * eligible / 289);
    const int start = int(ctx.rng.nextInt(289));
    for (int k = 0; k < 289 && m_creatures < m_creatureCap; ++k) {
        const int i = (start + k) % 289;
        const ChunkPos cp{pc.x + i % 17 - 8, pc.z + i / 17 - 8};
        const int x = cp.x * 16 + int(ctx.rng.nextInt(16)),
                  z = cp.z * 16 + int(ctx.rng.nextInt(16));
        const double dx = x + 0.5 - p.x, dz = z + 0.5 - p.z;
        if (dx * dx + dz * dz < 24.0 * 24.0) continue;
        spawnCreatureAt(ctx, x, z);
    }
}

void Mobs::spawnCreatureAt(Context& ctx, int x, int z) {
    const auto& r = blockRegistry();
    const Chunk* c = ctx.world.chunk({blockToChunk(x), blockToChunk(z)});
    if (!c || !c->lit() || !c->biomes()) return;
    const int y = rainHeight(ctx.world, x, z); // (on top of the column)
    if (!ctx.world.isInHeight(y) || !solid(ctx.world, x, y - 1, z)) return;
    const BlockId ground = r.blockOf(ctx.world.getBlock({x, y - 1, z}));
    if (solid(ctx.world, x, y, z) || c->skyLight(blockToLocal(x), y, blockToLocal(z)) < 9) return;
    const Biome biome = c->biomes()->at(blockToLocal(x), y, blockToLocal(z), ctx.world.height());
    // Mooshrooms (M29.1c; wiki: Mooshroom): mushroom fields' mycelium, groups of 4-8.
    if (biome == Biome::MushroomFields && ground == blocks::Mycelium) {
        const int n = 4 + int(ctx.rng.nextInt(5));
        for (int i = 0; i < n && m_creatures < 10; ++i) {
            const int gx = x + int(ctx.rng.nextInt(5)) - 2, gz = z + int(ctx.rng.nextInt(5)) - 2;
            if (!solid(ctx.world, gx, y - 1, gz) || solid(ctx.world, gx, y, gz)) continue;
            if (add(ctx.world, make(MobType::Mooshroom, {gx + 0.5, double(y), gz + 0.5}, ctx.rng)))
                ++m_creatures;
        }
        return;
    }
    // Horses, donkeys, llamas and camels (M26.2, Mounts.cpp): grass, or a desert's sand.
    if ((ground == blocks::GrassBlock || (biome == Biome::Desert && ground == blocks::Sand)) &&
        (biome == Biome::Plains || biome == Biome::Savanna || biome == Biome::WindsweptHills ||
         biome == Biome::Desert) &&
        ctx.rng.nextInt(2) == 0) {
        spawnMounts(ctx, biome, x, y, z);
        return;
    }
    // Rabbits, foxes, polar bears, pandas, goats, armadillos (M26.3, Wildlife.cpp).
    if (ctx.rng.nextInt(2) == 0 && biome != Biome::Forest && biome != Biome::SparseJungle &&
        biome != Biome::OldGrowthPineTaiga && biome != Biome::SavannaPlateau) {
        const auto before = m_creatures;
        spawnWildlife(ctx, biome, ground, x, y, z);
        if (m_creatures != before) return;
    }
    if (ground != blocks::GrassBlock && ground != blocks::Podzol && ground != blocks::Snow &&
        ground != blocks::SnowBlock)
        return;
    MobType kind = MobType::Count;
    int group = 1, variant = 0;
    // Farm animals (M32.1; wiki: Spawn - the grassy biomes' creature lists: sheep 12, pig 10,
    // chicken 10, cow 8, groups of 4); wolves, ocelots and parrots below with their own odds.
    const bool grassy =
        ground == blocks::GrassBlock &&
        (biome == Biome::Plains || biome == Biome::SunflowerPlains || biome == Biome::Meadow ||
         biome == Biome::Forest || biome == Biome::FlowerForest || biome == Biome::BirchForest ||
         biome == Biome::OldGrowthBirchForest || biome == Biome::DarkForest ||
         biome == Biome::Taiga || biome == Biome::OldGrowthPineTaiga ||
         biome == Biome::OldGrowthSpruceTaiga || biome == Biome::Jungle ||
         biome == Biome::SparseJungle || biome == Biome::BambooJungle || biome == Biome::Savanna ||
         biome == Biome::SavannaPlateau || biome == Biome::WindsweptHills ||
         biome == Biome::WindsweptForest || biome == Biome::Swamp || biome == Biome::CherryGrove ||
                         biome == Biome::DappledForest); // (M33.3b)
    if (grassy && ctx.rng.nextInt(5) != 0) {
        const uint32_t roll = ctx.rng.nextInt(40);
        const MobType farm = roll < 12   ? MobType::Sheep
                             : roll < 22 ? MobType::Pig
                             : roll < 32 ? MobType::Chicken
                                         : MobType::Cow;
        for (int i = 0, placed = 0; i < 4 && m_creatures < m_creatureCap; ++i) {
            const int gx = x + int(ctx.rng.nextInt(5)) - 2, gz = z + int(ctx.rng.nextInt(5)) - 2;
            if (!solid(ctx.world, gx, y - 1, gz) || solid(ctx.world, gx, y, gz) ||
                solid(ctx.world, gx, y + 1, gz))
                continue;
            MobData a = make(farm, {gx + 0.5, double(y), gz + 0.5}, ctx.rng);
            // (M32.6; wiki: Mob spawning) after the first of a group, 5% are young.
            if (placed > 0 && ctx.rng.nextFloat() < 0.05f) a.age = -24000;
            if (add(ctx.world, a)) {
                ++m_creatures;
                ++placed;
            }
        }
        return;
    }
    switch (biome) { // (wiki: Wolf › Variants - each biome's wolf)
    case Biome::Taiga:
        kind = MobType::Wolf, variant = 0;
        break;
    case Biome::Forest:
        kind = MobType::Wolf, variant = 1;
        break;
    case Biome::SnowyTaiga:
        kind = MobType::Wolf, variant = 2;
        break;
    case Biome::OldGrowthSpruceTaiga:
        kind = MobType::Wolf, variant = 4;
        break;
    case Biome::OldGrowthPineTaiga:
        kind = MobType::Wolf, variant = 3;
        break; // (M27.1: black)
    case Biome::SavannaPlateau:
        kind = MobType::Wolf, variant = 6;
        break; // (spotted)
    case Biome::SparseJungle:
        kind = MobType::Wolf, variant = 5;
        break;
    case Biome::Savanna:
        kind = MobType::Wolf, variant = 6;
        break;
    case Biome::WoodedBadlands:
        kind = MobType::Wolf, variant = 7;
        break;
    case Biome::Grove:
        kind = MobType::Wolf, variant = 8;
        break;
    case Biome::Jungle:
    case Biome::BambooJungle:
        kind = ctx.rng.nextInt(3) == 0 ? MobType::Ocelot : MobType::Parrot;
        break;
    default:
        return;
    }
    if (kind == MobType::Wolf && (biome == Biome::Forest || biome == Biome::SparseJungle) &&
        ctx.rng.nextInt(4) != 0)
        return; // (rarer there)
    // (wiki: Wolf › pack sizes - chestnut/rusty 2-4, spotted/striped 4-8, snowy alone, else 4)
    group = kind == MobType::Wolf ? (variant >= 3 && variant <= 5   ? 2 + int(ctx.rng.nextInt(3))
                                     : variant == 6 || variant == 7 ? 4 + int(ctx.rng.nextInt(5))
                                     : variant == 8                 ? 1
                                                                    : 4)
            : kind == MobType::Parrot ? 1 + int(ctx.rng.nextInt(2))
                                      : 1 + int(ctx.rng.nextInt(3));
    for (int i = 0; i < group; ++i) {
        const int gx = x + int(ctx.rng.nextInt(5)) - 2, gz = z + int(ctx.rng.nextInt(5)) - 2;
        if (!solid(ctx.world, gx, y - 1, gz) || solid(ctx.world, gx, y, gz)) continue;
        MobData m = make(kind, {gx + 0.5, double(y), gz + 0.5}, ctx.rng);
        if (kind == MobType::Wolf) m.woolColour = uint8_t(variant);
        if (add(ctx.world, m)) ++m_creatures;
    }
}

} // namespace mc
