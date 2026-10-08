#include "gameplay/BlockInteraction.h"

#include "world/ItemExtras.h"

#include "gameplay/Furnace.h"

#include "gameplay/Mining.h"
#include "world/BlockUpdates.h"
#include "world/Blocks.h"
#include "world/Enchantments.h"
#include "world/ItemContainers.h"
#include "world/Rotation.h"

#include <cmath>

namespace mc {

namespace {

// A container's contents drop when it breaks (wiki: Furnace › Breaking).
void dropContents(world::World& world, const world::BlockPos& p, std::vector<BlockInteraction::Drop>* drops) {
    world::Chunk* c = world.chunk(p.chunk());
    if (!c || !drops) return;
    if (const world::FurnaceData* f = c->furnace(world::blockToLocal(p.x), p.y, world::blockToLocal(p.z)))
        for (const world::ItemStack* s : {&f->input, &f->fuel, &f->output})
            if (!s->empty()) drops->push_back({{p.x + 0.5, p.y + 0.5, p.z + 0.5}, *s});
    if (const world::BrewingData* br = c->brewing(world::blockToLocal(p.x), p.y, world::blockToLocal(p.z))) {
        for (const world::ItemStack& s : br->bottles)
            if (!s.empty()) drops->push_back({{p.x + 0.5, p.y + 0.5, p.z + 0.5}, s});
        for (const world::ItemStack* s : {&br->ingredient, &br->fuel})
            if (!s->empty()) drops->push_back({{p.x + 0.5, p.y + 0.5, p.z + 0.5}, *s});
    }
    if (const world::ChestData* ch = c->chest(world::blockToLocal(p.x), p.y, world::blockToLocal(p.z))) {
        if (ch->shulker) {
            // A shulker box keeps its slots in the dropped box (wiki: Shulker Box): the
            // box's own drop carries them, or (creative) a new box if it holds anything.
            const uint32_t id = world::addItemContents(ch->items);
            const world::ItemId boxItem = world::itemRegistry().blockItem(world::blockRegistry().blockOf(world.getBlock(p)));
            bool attached = false;
            for (BlockInteraction::Drop& d : *drops)
                if (!attached && d.stack.item == boxItem && d.stack.contents == 0 &&
                    glm::distance(d.pos, glm::dvec3(p.x + 0.5, p.y + 0.5, p.z + 0.5)) < 1.0) {
                    d.stack.contents = id;
                    attached = true;
                }
            if (!attached && id) {
                world::ItemStack box{boxItem, 1};
                box.contents = id;
                drops->push_back({{p.x + 0.5, p.y + 0.5, p.z + 0.5}, box});
            }
        } else {
            for (const world::ItemStack& s : ch->items)
                if (!s.empty()) drops->push_back({{p.x + 0.5, p.y + 0.5, p.z + 0.5}, s});
        }
    }
    if (const world::BannerLayers* bl = c->banner(world::blockToLocal(p.x), p.y, world::blockToLocal(p.z))) {
        // A banner keeps its layers (M28.3d): on its own drop, or (creative) none.
        const uint32_t id = world::addBannerLayers(*bl);
        for (BlockInteraction::Drop& d : *drops)
            if (world::blockRegistry().kind(world::itemRegistry().item(d.stack.item).block) == world::BlockKind::Banner &&
                d.stack.extra == 0)
                d.stack.extra = id;
    }
    if (const world::CampfireData* cf = c->campfire(world::blockToLocal(p.x), p.y, world::blockToLocal(p.z)))
        for (const world::ItemStack& s : cf->items) // (its food: wiki: Campfire)
            if (!s.empty()) drops->push_back({{p.x + 0.5, p.y + 0.5, p.z + 0.5}, s});
    if (const world::JukeboxData* j = c->jukebox(world::blockToLocal(p.x), p.y, world::blockToLocal(p.z)); j && !j->record.empty())
        drops->push_back({{p.x + 0.5, p.y + 0.5, p.z + 0.5}, j->record}); // (M23.6: its disc)
    if (const world::HopperData* h = c->hopper(world::blockToLocal(p.x), p.y, world::blockToLocal(p.z))) // (M21.3)
        for (const world::ItemStack& s : h->items)
            if (!s.empty()) drops->push_back({{p.x + 0.5, p.y + 0.5, p.z + 0.5}, s});
    if (const world::DispenserData* d = c->dispenser(world::blockToLocal(p.x), p.y, world::blockToLocal(p.z)))
        for (const world::ItemStack& s : d->items)
            if (!s.empty()) drops->push_back({{p.x + 0.5, p.y + 0.5, p.z + 0.5}, s});
}

} // namespace

std::optional<world::RayHit> BlockInteraction::target(const world::World& world,
                                                      const Player& player) {
    const glm::dvec3 eye = player.position() + glm::dvec3(0.0, player.eyeHeight(), 0.0);
    return world::raycastBlocks(world, eye,
                                glm::dvec3(world::lookVector(player.yaw(), player.pitch())),
                                world::kCreativeReach);
}

world::BlockStateId BlockInteraction::orientedState(world::BlockStateId state,
                                                    world::Direction face) {
    const auto& r = world::blockRegistry();
    if (!r.value(state, "axis")) return state;
    const char* axis = (face == world::Direction::East || face == world::Direction::West) ? "x"
                       : (face == world::Direction::Up || face == world::Direction::Down) ? "y"
                                                                                          : "z";
    return r.with(state, "axis", axis).value_or(state);
}

void BlockInteraction::tick(world::World& world, const Player& player,
                            const std::optional<world::RayHit>& hit, world::BlockStateId placeState,
                            const InteractionInput& input, std::vector<world::BlockPos>& changed,
                            std::vector<Drop>* drops, bool holdingSword) {
    if (drops) drops->clear();
    changed.clear();
    // Cooldowns only run while the button is held; releasing allows an instant click.
    // A fresh click always acts (even a press shorter than a tick); holding repeats.
    m_destroyCooldown = input.attackClick ? 0
                        : input.attack    ? std::max(0, m_destroyCooldown - 1)
                                          : 0;
    m_useCooldown = input.useClick ? 0 : input.use ? std::max(0, m_useCooldown - 1) : 0;
    const bool attack = input.attack || input.attackClick;
    const bool use = input.use || input.useClick;

    if (!hit) return;

    if (attack && m_destroyCooldown == 0 && !holdingSword) {
        dropContents(world, hit->block, drops); // containers drop their items in every mode
        m_broken = world::blockRegistry().blockOf(world.getBlock(hit->block));
        world.levelEvent(world::LevelEvent::Type::BlockBreak, hit->block.x, hit->block.y, hit->block.z,
                         world.getBlock(hit->block));
        world.updateBlock(hit->block, world::leftAfterBreaking(world.getBlock(hit->block))); // (its water stays)
        changed.push_back(hit->block);
        m_destroyCooldown = kDestroyDelay;
        return; // one action per tick
    }
    if (use && m_useCooldown == 0) {
        if (useBlock(player, *hit, placeState != 0)) return;
        if (placeState == 0) return;
        m_useCooldown = kUseDelay;
        bool placed = false;
        place(world, player, *hit, placeState, changed, placed);
        if (placed) m_used = world::itemRegistry().blockItem(world::blockRegistry().blockOf(placeState));
    }
}

bool BlockInteraction::useBlock(const Player& player, const world::RayHit& hit, bool holding) {
    // Right-click acts on usable blocks, repeating while held; sneaking with an item in
    // hand places it instead (vanilla).
    if (!m_updates || (player.sneaking() && holding) || !m_updates->use(hit.block)) return false;
    m_useCooldown = kUseDelay;
    return true;
}

void BlockInteraction::place(world::World& world, const Player& player, const world::RayHit& hitRef,
                             world::BlockStateId placeState, std::vector<world::BlockPos>& changed,
                             bool& placed) {
    const auto& reg = world::blockRegistry();
    const world::RayHit* hit = &hitRef;
    // Where on the face the click landed (slab and stair halves).
    const glm::dvec3 point = player.eyePosition(1.0) +
                             glm::dvec3(world::lookVector(player.yaw(), player.pitch())) * hit->distance;
    const double hitY = std::clamp(point.y - double(hit->block.y), 0.0, 1.0);
    // A slab put on a slab of its kind fills the other half: a double slab (wiki: Slab) -
    // the clicked one from its open face, or a single slab in the cell in front.
    if (reg.kind(reg.blockOf(placeState)) == world::BlockKind::Slab) {
        auto merge = [&](const world::BlockPos& p, bool fromClick) {
            const world::BlockStateId s = world.getBlock(p);
            if (reg.blockOf(s) != reg.blockOf(placeState)) return false;
            const int type = reg.get(s, world::properties::slabType);
            if (type == 2) return false;
            if (fromClick && !((type == 1 && hit->face == world::Direction::Up) ||
                               (type == 0 && hit->face == world::Direction::Down)))
                return false;
            world.updateBlock(p, reg.set(s, world::properties::slabType, 2));
            world.levelEvent(world::LevelEvent::Type::BlockPlace, p.x, p.y, p.z, s);
            changed.push_back(p);
            placed = true;
            return true;
        };
        if (merge(hit->block, true) || merge(world::neighbour(hit->block, hit->face), false)) return;
    }
    // Another sea pickle into a clump of 1-3 (wiki: Sea Pickle).
    if (reg.blockOf(placeState) == world::blocks::SeaPickle) {
        const world::BlockStateId s = world.getBlock(hit->block);
        if (reg.blockOf(s) == world::blocks::SeaPickle && reg.get(s, world::properties::pickles) < 3) {
            world.updateBlock(hit->block, reg.set(s, world::properties::pickles, reg.get(s, world::properties::pickles) + 1));
            world.levelEvent(world::LevelEvent::Type::BlockPlace, hit->block.x, hit->block.y, hit->block.z, s);
            changed.push_back(hit->block);
            placed = true;
            return;
        }
    }
    {
        const world::BlockPos at = world::neighbour(hit->block, hit->face);
        if (!world.isInHeight(at.y)) return;
        const world::BlockStateId existing = world.getBlock(at);
        // Air and fluids can be replaced (not by torches, dust...: they can't exist in water).
        const bool torch = reg.blockOf(placeState) == world::blocks::Torch;
        // Ocean plants (M25.1) go into water: kelp and seagrass only into a water source,
        // corals and pickles anywhere (waterlogged when placed in a source).
        const world::BlockId placeBlock = reg.blockOf(placeState);
        const bool oceanPlant = world::BlockUpdates::isOceanPlant(placeBlock) && reg.kind(placeBlock) == world::BlockKind::Plain &&
                                !reg.block(placeBlock).id.ends_with("_coral_block");
        const bool waterSource =
            reg.blockOf(existing) == world::blocks::Water && reg.get(existing, world::properties::level) == 0;
        if (reg.waterlogged(reg.defaultState(placeBlock)) && reg.get(placeState, world::properties::waterlogged) < 0 &&
            !waterSource)
            return; // (kelp and seagrass need water)
        if (oceanPlant) {
            const world::BlockStateId below = world.getBlock({at.x, at.y - 1, at.z});
            const bool onKelp = placeBlock == world::blocks::Kelp &&
                                (reg.blockOf(below) == world::blocks::Kelp || reg.blockOf(below) == world::blocks::KelpPlant);
            if (!onKelp && !reg.collides(below)) return; // (they stand on a block)
        }
        if (existing != 0 && !(oceanPlant && reg.blockOf(existing) == world::blocks::Water) &&
            (reg.blockOf(existing) != world::blocks::Water || !reg.collides(placeState)))
            return;
        const Aabb blockBox{{at.x, at.y, at.z}, {at.x + 1.0, at.y + 1.0, at.z + 1.0}};
        if (reg.collides(placeState) && player.box().intersects(blockBox))
            return; // not inside the player
        if (!world.chunk(world::ChunkPos{world::blockToChunk(at.x), world::blockToChunk(at.z)}))
            return;
        // Torches stand on a block's top or hang on its side (BlockUpdates::placement);
        // never on its underside.
        if (torch && hit->face == world::Direction::Down) return;
        world::BlockStateId state = orientedState(placeState, hit->face);
        // Horizontal facing blocks (furnace) face the player (wiki: Furnace).
        if (reg.likeOf(reg.blockOf(state)) == world::blocks::Furnace ||
            reg.blockOf(state) == world::blocks::EndPortalFrame) {
            const float yaw = std::fmod(std::fmod(player.yaw(), 360.0f) + 360.0f, 360.0f);
            const int q = static_cast<int>(std::floor((yaw + 45.0f) / 90.0f)) % 4; // 0 S,1 W,2 N,3 E (look)
            static constexpr const char* kTowardPlayer[4] = {"north", "east", "south", "west"};
            state = reg.with(state, "facing", kTowardPlayer[q]).value_or(state);
        }
        // Redstone components: wall torches, attachment faces, facings, support.
        const auto fitted =
            world::BlockUpdates::placement(world, state, at, hit->face, player.yaw(), player.pitch(), hitY);
        if (!fitted) return;
        state = *fitted;
        if (reg.blockOf(state) == world::blocks::Kelp) // (an age 0-24, how much it may still grow - wiki: random;
            // ours from the position)
            state = reg.set(state, world::properties::age25,
                            int((uint32_t(at.x) * 73856093u ^ uint32_t(at.y) * 19349663u ^ uint32_t(at.z) * 83492791u) % 25u));
        if (reg.get(state, world::properties::waterlogged) >= 0) // (holds the water it went into)
            state = reg.set(state, world::properties::waterlogged, waterSource ? 0 : 1);
        world.updateBlock(at, state);
        if (m_placeExtra) // (M28.3d) a banner item's layers go onto the placed banner
            if (const auto layers = world::bannerLayers(m_placeExtra))
                if (world::Chunk* pc = world.chunk(at.chunk()))
                    if (world::BannerLayers* bl = pc->banner(world::blockToLocal(at.x), at.y, world::blockToLocal(at.z)))
                        *bl = *layers;
        if (m_placeContents) // a shulker box item's slots go back into the placed box (M23.6)
            if (world::Chunk* pc = world.chunk(at.chunk()))
                if (world::ChestData* box = pc->chest(world::blockToLocal(at.x), at.y, world::blockToLocal(at.z));
                    box && box->shulker)
                    box->items = world::itemContents(m_placeContents);
        world.levelEvent(world::LevelEvent::Type::BlockPlace, at.x, at.y, at.z, state);
        changed.push_back(at);
        placed = true;
    }
}

std::optional<glm::dvec3> chorusTeleport(const world::World& world, const glm::dvec3& feet, world::Xoroshiro& rng) {
    // wiki: Chorus Fruit - up to 16 tries at a random spot within 8 blocks on each
    // axis; the player drops down onto the first solid block below it (not into
    // fluids) where their body fits.
    const auto& r = world::blockRegistry();
    for (int attempt = 0; attempt < 16; ++attempt) {
        const int x = int(std::floor(feet.x + (rng.nextDouble() - 0.5) * 16.0));
        const int z = int(std::floor(feet.z + (rng.nextDouble() - 0.5) * 16.0));
        int y = std::clamp(int(std::floor(feet.y + (rng.nextDouble() - 0.5) * 16.0)), world.height().minY + 1,
                           world.height().maxY() - 2);
        while (y > world.height().minY + 1 && !r.collides(world.getBlock({x, y - 1, z})))
            --y;
        const world::BlockStateId below = world.getBlock({x, y - 1, z});
        if (!r.collides(below)) continue;
        const world::BlockStateId a = world.getBlock({x, y, z}), b = world.getBlock({x, y + 1, z});
        auto free = [&](world::BlockStateId s) {
            const world::BlockId id = r.blockOf(s);
            return !r.collides(s) && id != world::blocks::Water && id != world::blocks::Lava;
        };
        if (free(a) && free(b)) return glm::dvec3(x + 0.5, double(y), z + 0.5);
    }
    return std::nullopt;
}

bool BlockInteraction::tickDrinking(Inventory& inventory, Vitals& vitals, bool use, bool survival) {
    const auto& items = world::itemRegistry();
    static const world::ItemId potionItem = *items.find("potion"), milk = *items.find("milk_bucket"),
                               bottle = *items.find("glass_bottle"), bucket = *items.find("bucket"),
                               ominous = *items.find("ominous_bottle");
    const world::ItemStack heldStack = inventory.selectedStack();
    if (!use || (heldStack.item != potionItem && heldStack.item != milk && heldStack.item != ominous)) return false;
    if (++m_eatTicks >= kEatTicks) {
        m_eatTicks = 0;
        if (heldStack.item == ominous) { // Bad Omen for 100 minutes (wiki: Ominous Bottle; ours: level I)
            vitals.addEffect(world::Effect::BadOmen, 0, 120000);
            if (survival) inventory.consumeSelected(1);
        } else if (heldStack.item == milk) {
            vitals.clearEffects();
            if (survival) inventory.setSlot(inventory.selected(), {bucket, 1});
        } else {
            const world::PotionInfo& p = world::potionInfo(static_cast<world::Potion>(heldStack.potion));
            if (p.effect != world::Effect::None) vitals.addEffect(p.effect, p.amplifier, p.duration);
            if (survival) inventory.setSlot(inventory.selected(), {bottle, 1});
        }
    }
    return true;
}

void BlockInteraction::tickSurvival(world::World& world, const Player& player,
                                    const std::optional<world::RayHit>& hit, Inventory& inventory,
                                    Vitals& vitals, const InteractionInput& input, bool eyesInWater,
                                    world::Xoroshiro& rng, std::vector<world::BlockPos>& changed,
                                    std::vector<Drop>& drops) {
    changed.clear();
    drops.clear();
    const auto& items = world::itemRegistry();
    const bool attack = input.attack || input.attackClick;
    const bool use = input.use || input.useClick;
    const bool pausing = m_destroyCooldown > 0; // after a break: 5 idle ticks
    if (pausing) --m_destroyCooldown;
    m_useCooldown = input.useClick ? 0 : input.use ? std::max(0, m_useCooldown - 1) : 0;

    // Eating: hold use with food while hungry (wiki: Food). Food that plants (carrots,
    // potatoes) is planted instead when aimed at farmland's top.
    const world::ItemDef& held = items.item(inventory.selectedStack().item);
    const bool planting = held.block && hit && hit->face == world::Direction::Up &&
                          world::blockRegistry().blockOf(world.getBlock(hit->block)) == world::blocks::Farmland;
    if (m_chorusCooldown > 0) --m_chorusCooldown;
    if (tickDrinking(inventory, vitals, use, true)) {
    } else if (use && !planting && held.food > 0 && (vitals.food() < Vitals::kMaxFood || held.alwaysEdible) &&
               !(m_chorusCooldown > 0 && held.id == "minecraft:chorus_fruit")) {
        // (dried kelp is eaten twice as fast - wiki: Dried Kelp)
        if (++m_eatTicks >= (held.id == "minecraft:dried_kelp" ? kEatTicks / 2 : kEatTicks)) {
            vitals.eat(held.food, held.saturation);
            if (held.id == "minecraft:golden_apple") // (wiki: Regeneration II for 5 s; no Absorption yet)
                vitals.addEffect(world::Effect::Regeneration, 1, 100);
            if (held.id == "minecraft:pufferfish") { // (wiki: Hunger III 15 s, Poison II 60 s; no Nausea yet)
                vitals.addEffect(world::Effect::Hunger, 2, 300);
                vitals.addEffect(world::Effect::Poison, 1, 1200);
            }
            if (held.id == "minecraft:chorus_fruit") { // main teleports (wiki: 1 s cooldown)
                m_ateChorus = true;
                m_chorusCooldown = 20;
            }
            if (held.id == "minecraft:honey_bottle") { // (M26.3b; wiki: Honey Bottle - cures Poison, keeps the bottle)
                vitals.removeEffect(world::Effect::Poison);
                inventory.consumeSelected(1);
                inventory.add({*items.find("glass_bottle"), 1});
            } else {
                inventory.consumeSelected(1);
            }
            m_eatTicks = 0;
        }
    } else {
        m_eatTicks = 0;
    }

    // Breaking.
    if (!attack || !hit || !m_mayBuild) {
        m_breaking.reset();
        m_progress = 0.0f;
        m_progressExact = 0.0;
    } else if (!pausing) {
        if (!m_breaking || !(*m_breaking == hit->block)) { // a new target starts over
            m_breaking = hit->block;
            m_progressExact = 0.0;
            // Hitting a note block plays it (wiki: Note Block; M23.6).
            if (m_updates && world::blockRegistry().blockOf(world.getBlock(hit->block)) == world::blocks::NoteBlock)
                m_updates->playNote(hit->block);
        }
        const world::BlockStateId state = world.getBlock(hit->block);
        // Haste speeds mining; Conduit Power counts as Haste I (wiki: Conduit Power).
        const int haste = std::max(vitals.effectLevel(world::Effect::Haste),
                                   vitals.effectLevel(world::Effect::ConduitPower) > 0 ? 1 : 0);
        const int ticks = breakTicks(state, inventory.selectedStack(), player.onGround(), eyesInWater, haste,
                                     vitals.effectLevel(world::Effect::MiningFatigue));
        if (ticks >= 0) {
            // Progress grows by the current tool's per-tick share (vanilla), so switching
            // tools mid-break changes the remaining time, not the progress made.
            m_progressExact += ticks == 0 ? 1.0 : 1.0 / double(ticks);
            // Cracking particles fly off the face being mined (vanilla: one per tick).
            world.levelEvent(world::LevelEvent::Type::BlockHit, hit->block.x, hit->block.y, hit->block.z,
                             uint32_t(state) | uint32_t(hit->face) << 16);
            m_progress = static_cast<float>(std::min(1.0, m_progressExact));
            if (m_progressExact >= 1.0 - 1e-9) {
                m_dropScratch.clear();
                if (m_blockDrops) blockDrops(state, inventory.selectedStack(), rng, m_dropScratch);
                if (m_blockDrops && canHarvest(state, inventory.selectedStack()) &&
                    world::enchantLevel(inventory.selectedStack(), world::Enchantment::SilkTouch) == 0) // (wiki)
                    m_experience += blockExperience(state, rng);
                m_xpAt = hit->block;
                for (const world::ItemStack& d : m_dropScratch)
                    drops.push_back({{hit->block.x + 0.5, hit->block.y + 0.25, hit->block.z + 0.5}, d});
                dropContents(world, hit->block, &drops);
                // A broken furnace releases the experience it stored (vanilla).
                if (world::Chunk* fc = world.chunk(hit->block.chunk()))
                    if (world::FurnaceData* f =
                            fc->furnace(world::blockToLocal(hit->block.x), hit->block.y, world::blockToLocal(hit->block.z)))
                        m_experience += takeFurnaceExperience(*f, rng);
                world.levelEvent(world::LevelEvent::Type::BlockBreak, hit->block.x, hit->block.y, hit->block.z, state);
                m_broken = world::blockRegistry().blockOf(state);
                world.updateBlock(hit->block, world::leftAfterBreaking(state)); // (its water stays)
                changed.push_back(hit->block);
                vitals.exhaust(0.005f); // wiki: Hunger - breaking a block
                // Tools wear 1 per block, swords 2 (wiki: Durability); blocks that break
                // instantly by hand (hardness 0) don't count.
                const world::ItemStack& tool = inventory.selectedStack();
                const world::ItemDef& def = items.item(tool.item);
                const auto& breg = world::blockRegistry();
                const bool handInstant = breg.block(breg.blockOf(state)).settings.hardness == 0.0f;
                if (def.durability > 0 && !handInstant) {
                    m_used = tool.item;
                    const world::ItemId toolItem = tool.item;
                    inventory.setSlot(inventory.selected(),
                                      wearItem(tool, def.tool == world::ToolType::Sword ? 2 : 1, rng));
                    if (inventory.selectedStack().empty()) m_brokenTool = toolItem;
                }
                m_breaking.reset();
                m_progress = 0.0f;
                m_progressExact = 0.0;
                if (ticks > 0) m_destroyCooldown = kSurvivalBreakDelay; // instant breaks: no pause
                return; // one action per tick
            }
        }
    }

    // Using a block (lever, button...), else placing, which uses up the held block.
    if (use && m_useCooldown == 0 && hit && useBlock(player, *hit, !inventory.selectedStack().empty())) {
        // used
    } else if (use && m_useCooldown == 0 && hit && held.block && m_mayBuild) {
        m_useCooldown = kUseDelay;
        bool placed = false;
        place(world, player, *hit, inventory.placeState(), changed, placed);
        if (placed) m_used = inventory.selectedStack().item;
        if (placed) inventory.consumeSelected(1);
    }
}

} // namespace mc
