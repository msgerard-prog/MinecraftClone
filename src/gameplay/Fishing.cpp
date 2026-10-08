// Fishing (M25.2; wiki: Fishing, Fishing Rod).
#include "gameplay/Fishing.h"

#include "world/Blocks.h"
#include "world/Enchantments.h"
#include "world/Items.h"
#include "world/Potions.h"

#include <cmath>

namespace mc {

using namespace world;

namespace {

bool waterAt(const World& w, const glm::dvec3& p) {
    const BlockStateId s = w.getBlock({int(std::floor(p.x)), int(std::floor(p.y)), int(std::floor(p.z))});
    return blockRegistry().blockOf(s) == blocks::Water || blockRegistry().waterlogged(s);
}
bool solidAt(const World& w, const glm::dvec3& p) {
    return blockRegistry().collides(w.getBlock({int(std::floor(p.x)), int(std::floor(p.y)), int(std::floor(p.z))}));
}

} // namespace

void Fishing::cast(const glm::dvec3& eye, const glm::dvec3& look, int lure, int luck, Xoroshiro& rng) {
    // Thrown forward at ~0.6 blocks a tick with a little spread (vanilla FishingHook).
    m_active = true;
    m_state = State::Flying;
    m_pos = m_prev = eye + look * 0.3;
    m_vel = look * 0.6 + glm::dvec3(rng.nextDouble() - 0.5, rng.nextDouble() - 0.5, rng.nextDouble() - 0.5) * 0.01;
    m_lure = lure;
    m_luck = luck;
    m_timer = 0;
}

void Fishing::tick(const World& world, const glm::dvec3& player, Xoroshiro& rng) {
    if (!m_active) return;
    m_prev = m_pos;
    if (glm::length(m_pos - player) > 32.0) { // (the line snaps)
        m_active = false;
        return;
    }
    const bool inWater = waterAt(world, m_pos);
    if (m_state == State::Ground) return;
    if (inWater) {
        // Floats: pulled up to just under the surface, slowed by the water.
        const double cellTop = std::floor(m_pos.y) + (waterAt(world, m_pos + glm::dvec3(0, 1, 0)) ? 1.0 : 0.88);
        m_vel.y += (cellTop - 0.1 - m_pos.y) * 0.1;
        m_vel *= glm::dvec3(0.9, 0.8, 0.9);
        if (m_state == State::Flying) {
            m_state = State::Waiting;
            m_timer = std::max(20, 100 + int(rng.nextInt(501)) - m_lure * 100); // 5-30 s, -5 s a Lure level
        }
    } else {
        m_vel.y -= 0.03; // (vanilla: a thrown hook's gravity)
        m_vel *= 0.92;
        if (m_state != State::Flying) m_state = State::Flying; // (pulled out of the water)
    }
    const glm::dvec3 next = m_pos + m_vel;
    if (solidAt(world, next)) {
        if (m_state == State::Flying) m_state = State::Ground; // (stuck on the ground: nothing bites)
        m_vel = glm::dvec3(0.0);
        return;
    }
    m_pos = next;
    if (m_state == State::Waiting && --m_timer <= 0) {
        m_state = State::Nibble; // a fish swims up (the bubble trail)
        m_timer = 20 + int(rng.nextInt(61));
    } else if (m_state == State::Nibble && --m_timer <= 0) {
        m_state = State::Bite; // the bobber dips: reel now
        m_timer = 20 + int(rng.nextInt(21));
        m_vel.y -= 0.2;
    } else if (m_state == State::Bite && --m_timer <= 0) {
        m_state = State::Waiting; // missed it: wait for the next
        m_timer = std::max(20, 100 + int(rng.nextInt(501)) - m_lure * 100);
    }
}

ItemStack Fishing::rollCatch(int luck, Xoroshiro& rng) {
    const auto& items = itemRegistry();
    auto item = [&](const char* name) { return items.find(name).value_or(kNoItem); };
    // Categories (wiki: Fishing › Catching items, Luck of the Sea 0..3).
    const int treasure = 5 + 2 * luck, junk = std::max(0, 10 - 2 * luck);
    const int r = int(rng.nextInt(100));
    if (r < treasure) {
        // Treasure (each 1/6): an enchanted bow, an enchanted book, an enchanted rod, a
        // name tag, a nautilus shell, a saddle (the last three where the item exists).
        static constexpr const char* kTreasure[6] = {"bow", "enchanted_book", "fishing_rod", "name_tag",
                                                     "nautilus_shell", "saddle"};
        const char* pick = kTreasure[rng.nextInt(6)];
        ItemStack s{item(pick), 1};
        if (s.item == kNoItem) s = {item("nautilus_shell"), 1};
        if (s.item == kNoItem) return {item("cod"), 1};
        // Enchanted as if at level 30 (wiki): ours one or two random fitting enchantments.
        if (s.item == item("bow") || s.item == item("fishing_rod") || s.item == item("enchanted_book")) {
            const ItemId target = s.item == item("enchanted_book") ? item("fishing_rod") : s.item;
            for (int k = 0, n = 1 + int(rng.nextInt(2)); k < n; ++k) {
                const auto e = static_cast<Enchantment>(1 + rng.nextInt(uint32_t(Enchantment::Count) - 1));
                if (!canEnchant(target, e) && s.item != item("enchanted_book")) continue;
                if (enchantmentInfo(e).target == EnchantTarget::Armor && s.item == item("enchanted_book") && k > 0)
                    continue;
                setEnchantment(s, e, 1 + int(rng.nextInt(uint32_t(enchantmentInfo(e).maxLevel))));
            }
        }
        return s;
    }
    if (r < treasure + junk) {
        // Junk (wiki weights): lily pad 17, bowl/leather/leather boots/rotten flesh/
        // stick/string/water bottle/bone/tripwire hook 10 each, fishing rod 2, ink sac 1.
        struct J {
            const char* name;
            int weight;
        };
        static constexpr J kJunk[] = {{"lily_pad", 17}, {"bowl", 10}, {"leather", 10}, {"leather_boots", 10},
                                      {"rotten_flesh", 10}, {"stick", 5}, {"string", 5}, {"glass_bottle", 10},
                                      {"bone", 10}, {"tripwire_hook", 10}, {"fishing_rod", 2}, {"ink_sac", 10}};
        int total = 0;
        for (const J& j : kJunk) total += j.weight;
        int w = int(rng.nextInt(uint32_t(total)));
        for (const J& j : kJunk) {
            if ((w -= j.weight) >= 0) continue;
            const ItemId it = item(j.name);
            if (it == kNoItem) break; // (not in the game yet: a fish instead)
            ItemStack s{it, 1};
            if (std::string_view(j.name) == "glass_bottle") { // (the wiki's junk is a water bottle)
                s.item = item("potion");
                s.potion = uint8_t(Potion::Water);
            }
            if (items.item(it).durability > 0) // (worn: vanilla damages junk boots and rods)
                s.damage = uint16_t(items.item(it).durability * (0.1 + rng.nextDouble() * 0.8));
            return s;
        }
    }
    // Fish: cod 60, salmon 25, pufferfish 13, tropical fish 2.
    const int f = int(rng.nextInt(100));
    return {item(f < 60 ? "cod" : f < 85 ? "salmon" : f < 98 ? "pufferfish" : "tropical_fish"), 1};
}

int Fishing::reel(const World& world, const glm::dvec3& player, ItemEntities& items, ExperienceOrbs* orbs,
                  Xoroshiro& rng) {
    if (!m_active) return 0;
    m_active = false;
    if (m_state == State::Ground) return 2;
    if (m_state != State::Bite || !waterAt(world, m_pos)) return 0;
    // The catch flies to the player (vanilla: velocity d x 0.1, plus a lift of
    // sqrt(distance) x 0.08).
    const ItemStack caught = rollCatch(m_luck, rng);
    if (ItemEntity* e = items.spawn(m_pos, caught, rng, 0)) {
        const glm::dvec3 d = player - m_pos;
        e->vel = glm::dvec3(d.x * 0.1, d.y * 0.1 + std::sqrt(glm::length(d)) * 0.08, d.z * 0.1);
    }
    if (orbs) orbs->drop(player, 1 + int(rng.nextInt(6)), rng);
    return 1;
}

} // namespace mc
