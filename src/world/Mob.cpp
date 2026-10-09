#include "world/Mob.h"

#include <iterator>

namespace mc::world {

const MobInfo& mobInfo(MobType t) {
    // wiki: Zombie (20 HP, 0.6 x 1.95, speed 0.23, 3 damage on normal), Cow (10 HP,
    // 0.9 x 1.4, speed 0.2, passive).
    static constexpr MobInfo kInfo[] = {
        {"minecraft:zombie", 20.0f, 0.6, 1.95, 0.23, 3.0f, true},
        {"minecraft:cow", 10.0f, 0.9, 1.4, 0.2, 0.0f, false},
        // wiki: Sheep (8 HP, 0.9 x 1.3, 0.23), Pig (10 HP, 0.9 x 0.9, 0.25), Chicken
        // (4 HP, 0.4 x 0.7, 0.25).
        {"minecraft:sheep", 8.0f, 0.9, 1.3, 0.23, 0.0f, false},
        {"minecraft:pig", 10.0f, 0.9, 0.9, 0.25, 0.0f, false},
        {"minecraft:chicken", 4.0f, 0.4, 0.7, 0.25, 0.0f, false},
        // wiki: Skeleton (20, 0.6 x 1.99, 0.25, arrows), Creeper (20, 0.6 x 1.7, 0.25,
        // explodes), Spider (16, 1.4 x 0.9, 0.3, 2), Enderman (40, 0.6 x 2.9, 0.3, 7).
        {"minecraft:skeleton", 20.0f, 0.6, 1.99, 0.25, 0.0f, true},
        {"minecraft:creeper", 20.0f, 0.6, 1.7, 0.25, 0.0f, true},
        {"minecraft:spider", 16.0f, 1.4, 0.9, 0.3, 2.0f, true},
        {"minecraft:enderman", 40.0f, 0.6, 2.9, 0.3, 7.0f, true},
        // wiki: Ghast (10, 4 x 4, fireballs, flies), Blaze (20, 0.6 x 1.8, 6 melee,
        // fireballs, hovers), Magma Cube (large: 16, 2.08 x 2.08, 6; smaller sizes scale
        // in Mobs::box), Zombified Piglin (20, 0.6 x 1.95, 8 with its golden sword) -
        // all immune to fire and lava.
        {"minecraft:ghast", 10.0f, 4.0, 4.0, 0.7, 0.0f, true, true, true, 4.5f},
        {"minecraft:blaze", 20.0f, 0.6, 1.8, 0.23, 6.0f, true, true, true},
        {"minecraft:magma_cube", 16.0f, 2.08, 2.08, 0.2, 6.0f, true, true},
        {"minecraft:zombified_piglin", 20.0f, 0.6, 1.95, 0.23, 8.0f, true, true},
        // wiki: Piglin (16, 0.6 x 1.95, 0.35, 8 with a golden sword on normal), Hoglin
        // (40, 1.3965 x 1.4, 0.3, 3-8: ours 6), Strider (20, 0.9 x 1.7, 0.175, passive,
        // fire immune).
        {"minecraft:piglin", 16.0f, 0.6, 1.95, 0.35, 8.0f, true},
        {"minecraft:hoglin", 40.0f, 1.3965, 1.4, 0.3, 6.0f, true, false, false, 2.0f},
        {"minecraft:strider", 20.0f, 0.9, 1.7, 0.175, 0.0f, false, true},
        // wiki: End Crystal - a 2x2x2 entity that any damage destroys (an explosion of
        // power 6); no gravity; fire doesn't harm it.
        {"minecraft:end_crystal", 1.0f, 2.0, 2.0, 0.0, 0.0f, false, true, true, 1.6f},
        // wiki: Ender Dragon - 200 health, 10 damage from its head (Normal); flies
        // through blocks; drawn 4x its model (about 14 blocks long, 12 across the wings).
        // Our box is its body (vanilla: several part boxes, 16 x 8 overall).
        {"minecraft:ender_dragon", 200.0f, 6.0, 3.0, 0.0, 10.0f, true, true, true, 4.0f},
        // wiki: Shulker - 30 health, a 1x1x1 box stuck to a block, its bullets hit for
        // 4 and levitate; armour 20 while closed (our: 80% less damage).
        {"minecraft:shulker", 30.0f, 1.0, 1.0, 0.0, 4.0f, true, true, true},
        // wiki: Minecart - 0.98 x 0.7; a couple of hits break it (ours: 2 health).
        {"minecraft:minecart", 2.0f, 0.98, 0.7, 0.0, 0.0f, false, false, true},
        // wiki: Slime (large: 16, 2.08 x 2.08, 4; sizes as the magma cube's)
        {"minecraft:slime", 16.0f, 2.08, 2.08, 0.2, 4.0f, true},
        // wiki: Villager - 20 health, 0.6 x 1.95, movement speed 0.5 (strolls at 0.6 of it).
        {"minecraft:villager", 20.0f, 0.6, 1.95, 0.5, 0.0f, false},
        // wiki: Zombie Villager - as a zombie (20, 0.6 x 1.95, 0.23, 3 on normal).
        {"minecraft:zombie_villager", 20.0f, 0.6, 1.95, 0.23, 3.0f, true},
        // wiki: Iron Golem - 100 health, 1.4 x 2.7, speed 0.25, hits for 7.5-21.5 (ours:
        // the attack adds up to 15 at random), never hostile on its own.
        {"minecraft:iron_golem", 100.0f, 1.4, 2.7, 0.25, 7.5f, false},
        // wiki: Witch - 26 health, 0.6 x 1.95, speed 0.25, throws potions.
        {"minecraft:witch", 26.0f, 0.6, 1.95, 0.25, 0.0f, true},
        // wiki: Wandering Trader - 20 health, 0.6 x 1.95, speed 0.5, passive.
        {"minecraft:wandering_trader", 20.0f, 0.6, 1.95, 0.7, 0.0f, false},
        // wiki: Pillager - 24 health, 0.6 x 1.95, speed 0.35, shoots a crossbow.
        {"minecraft:pillager", 24.0f, 0.6, 1.95, 0.35, 0.0f, true},
        // wiki: Vindicator (24, 0.6 x 1.95, 0.35, an axe for 13 on Normal), Evoker (24,
        // 0.6 x 1.95, 0.5, spells), Vex (14, 0.4 x 0.8, flies, 9), Ravager (100, 1.95 x
        // 2.2, 0.3, 12).
        {"minecraft:vindicator", 24.0f, 0.6, 1.95, 0.35, 13.0f, true},
        {"minecraft:evoker", 24.0f, 0.6, 1.95, 0.5, 0.0f, true},
        {"minecraft:vex", 14.0f, 0.4, 0.8, 1.0, 9.0f, true, false, true},
        {"minecraft:ravager", 100.0f, 1.95, 2.2, 0.3, 12.0f, true},
        // Water mobs (M25.2; wiki: Cod 0.5 x 0.3, Salmon 0.7 x 0.4, Tropical Fish 0.5 x
        // 0.4, Pufferfish 0.7 x 0.7 - all 3 health; Squid and Glow Squid 10, 0.8 x 0.8).
        // Speed: their swim speed in blocks a tick (ours).
        {"minecraft:cod", 3.0f, 0.5, 0.3, 0.12, 0.0f, false, false, false, 1.0f, true},
        {"minecraft:salmon", 3.0f, 0.7, 0.4, 0.14, 0.0f, false, false, false, 1.0f, true},
        {"minecraft:tropical_fish", 3.0f, 0.5, 0.4, 0.12, 0.0f, false, false, false, 1.0f, true},
        {"minecraft:pufferfish", 3.0f, 0.7, 0.7, 0.08, 0.0f, false, false, false, 1.0f, true},
        {"minecraft:squid", 10.0f, 0.8, 0.8, 0.1, 0.0f, false, false, false, 1.0f, true},
        {"minecraft:glow_squid", 10.0f, 0.8, 0.8, 0.1, 0.0f, false, false, false, 1.0f, true},
        // wiki: Boat - 1.375 x 0.5625; a few hits break it (ours: 4 health, like the cart's 2).
        {"minecraft:oak_boat", 4.0f, 1.375, 0.5625, 0.0, 0.0f, false, false, false, 2.0f},
        // wiki: Drowned - as a zombie (20, 0.6 x 1.95, 0.23, 3); a thrown trident hits for 8.
        {"minecraft:drowned", 20.0f, 0.6, 1.95, 0.23, 3.0f, true},
        // wiki: Dolphin - 10 health, 0.9 x 0.6, swims fast; Turtle - 30 health, 1.2 x 0.4,
        // slow on land (0.25 our walk), drawn 1.5x its model.
        {"minecraft:dolphin", 10.0f, 0.9, 0.6, 0.22, 0.0f, false, false, false, 1.4f, true},
        {"minecraft:turtle", 30.0f, 1.2, 0.4, 0.12, 0.0f, false, false, false, 1.5f},
        // wiki: Guardian - 30 health, 0.85 x 0.85, laser 6; Elder Guardian - 80 health,
        // 1.9975 x 1.9975, laser 8 (Normal). Their swim speed is ours.
        {"minecraft:guardian", 30.0f, 0.85, 0.85, 0.1, 0.0f, true, false, false, 1.1f, true},
        {"minecraft:elder_guardian", 80.0f, 1.9975, 1.9975, 0.06, 0.0f, true, false, false, 2.6f, true},
        // wiki: Wolf - 8 health wild (40 tamed), 0.6 x 0.85, speed 0.3, bites for 4; Cat - 10,
        // 0.6 x 0.7, 0.3, 3; Ocelot - 10, 0.6 x 0.7, 0.3, 3; Parrot - 6, 0.5 x 0.9, flies.
        {"minecraft:wolf", 8.0f, 0.6, 0.85, 0.3, 4.0f, false},
        {"minecraft:cat", 10.0f, 0.6, 0.7, 0.3, 3.0f, false},
        {"minecraft:ocelot", 10.0f, 0.6, 0.7, 0.3, 3.0f, false},
        {"minecraft:parrot", 6.0f, 0.5, 0.9, 0.2, 0.0f, false},
        // wiki: Horse - 15-30 health (each its own; 30 here is the cap), 1.3965 x 1.6,
        // speed 0.1125-0.3375; Donkey and Mule - 15-30, 1.3965 x 1.5 / 1.6, speed 0.175;
        // Llama - 15-30 (by strength), 0.9 x 1.87, 0.175, spits for 1; Camel - 32,
        // 1.7 x 2.375, 0.09.
        {"minecraft:horse", 30.0f, 1.3965, 1.6, 0.225, 0.0f, false},
        {"minecraft:donkey", 30.0f, 1.3965, 1.5, 0.175, 0.0f, false},
        {"minecraft:mule", 30.0f, 1.3965, 1.6, 0.175, 0.0f, false},
        {"minecraft:llama", 30.0f, 0.9, 1.87, 0.175, 1.0f, false},
        {"minecraft:trader_llama", 30.0f, 0.9, 1.87, 0.175, 1.0f, false},
        {"minecraft:camel", 32.0f, 1.7, 2.375, 0.09, 0.0f, false},
        // wiki: Rabbit - 3 health, 0.4 x 0.5, speed 0.3; Fox - 10, 0.6 x 0.7, 0.3, bites
        // for 2; Polar Bear - 30, 1.4 x 1.4, 0.25, 6; Panda - 20, 1.3 x 1.25, 0.15, 6;
        // Goat - 10, 0.9 x 1.3, 0.2, rams for 2; Armadillo - 12, 0.7 x 0.65, 0.14.
        {"minecraft:rabbit", 3.0f, 0.4, 0.5, 0.3, 0.0f, false},
        {"minecraft:fox", 10.0f, 0.6, 0.7, 0.3, 2.0f, false},
        {"minecraft:polar_bear", 30.0f, 1.4, 1.4, 0.25, 6.0f, false},
        {"minecraft:panda", 20.0f, 1.3, 1.25, 0.15, 6.0f, false},
        {"minecraft:goat", 10.0f, 0.9, 1.3, 0.2, 2.0f, false},
        {"minecraft:armadillo", 12.0f, 0.7, 0.65, 0.14, 0.0f, false},
        // wiki: Bee - 10 health, 0.7 x 0.6, flying speed 0.6, stings for 2 (+ Poison).
        {"minecraft:bee", 10.0f, 0.7, 0.6, 0.3, 2.0f, false, false, true},
        // wiki: Frog - 10 health, 0.5 x 0.5; Tadpole - 6, 0.4 x 0.3, swims; Axolotl - 14,
        // 0.75 x 0.42, swims (walks slowly on land), bites for 2.
        {"minecraft:frog", 10.0f, 0.5, 0.5, 0.25, 0.0f, false},
        {"minecraft:tadpole", 6.0f, 0.4, 0.3, 0.1, 0.0f, false, false, false, 1.0f, true},
        {"minecraft:axolotl", 14.0f, 0.75, 0.42, 0.1, 2.0f, false, false, false, 1.0f, true},
        // wiki: Cave Spider - 12 health, 0.7 x 0.5, 0.3, 2 + Poison; Silverfish - 8, 0.4 x
        // 0.3, 0.25, 1; Wither Skeleton - 20, 0.7 x 2.4, 0.25, 8 (Normal, stone sword) + Wither,
        // fireproof; Phantom - 20, 0.9 x 0.5, flies, 6 (Normal).
        {"minecraft:cave_spider", 12.0f, 0.7, 0.5, 0.3, 2.0f, true, false, false, 0.7f},
        {"minecraft:silverfish", 8.0f, 0.4, 0.3, 0.25, 1.0f, true},
        {"minecraft:wither_skeleton", 20.0f, 0.7, 2.4, 0.25, 8.0f, true, true, false, 1.2f},
        {"minecraft:phantom", 20.0f, 0.9, 0.5, 0.5, 6.0f, true, false, true},
        // wiki: Wither - 300 health (Java), 0.9 x 3.5, flies at 0.6, fireproof; skulls.
        {"minecraft:wither", 300.0f, 0.9, 3.5, 0.6, 0.0f, true, true, true},
        // wiki: Breeze - 30 health, 0.6 x 1.77, speed 0.63 (its leaps), wind charges for 1.
        {"minecraft:breeze", 30.0f, 0.6, 1.77, 0.63, 1.0f, true},
        // wiki: Allay - 20 health, 0.35 x 0.6, flies at 0.4; Nautilus - 15, 0.875 x 0.95,
        // swims (6.5 blocks/s), bites for 3 when provoked.
        {"minecraft:allay", 20.0f, 0.35, 0.6, 0.4, 0.0f, false, false, true},
        {"minecraft:nautilus", 15.0f, 0.875, 0.95, 0.3, 3.0f, false, false, false, 1.0f, true},
        // wiki: Happy Ghast - 20 health, 4 x 4 (ghastlings 0.95), flies at 0.05 (ridden
        // ~3.6 blocks/s), drawn as a ghast; Copper Golem - 12, 0.49 x 0.98, 0.2.
        {"minecraft:happy_ghast", 20.0f, 4.0, 4.0, 0.05, 0.0f, false, false, true, 4.5f},
        {"minecraft:copper_golem", 12.0f, 0.49, 0.98, 0.2, 0.0f, false},
        // wiki: Creaking - 1 health (only its heart can end it), 0.9 x 2.7, speed 0.3, hits
        // for 3 (Normal).
        {"minecraft:creaking", 1.0f, 0.9, 2.7, 0.3, 3.0f, true},
        // wiki: Warden - 500 health, 0.9 x 2.9, speed 0.3 (fast when angry), hits for 30
        // (Normal); its model is drawn twice size (half-size boxes on a 64x64 skin).
        {"minecraft:warden", 500.0f, 0.9, 2.9, 0.3, 30.0f, true, false, false, 2.0f},
        // wiki: Sniffer - 14 health, 1.9 x 1.75, speed 0.1; drawn twice size (half boxes).
        {"minecraft:sniffer", 14.0f, 1.9, 1.75, 0.1, 0.0f, false, false, false, 2.0f},
        // (M28.3a) hanging entities: one hit breaks them (their boxes come from the facing).
        {"minecraft:item_frame", 1.0f, 0.75, 0.75, 0.0, 0.0f, false},
        {"minecraft:glow_item_frame", 1.0f, 0.75, 0.75, 0.0, 0.0f, false},
        {"minecraft:painting", 1.0f, 1.0, 1.0, 0.0, 0.0f, false},
        // wiki: Armor Stand - 0.5 x 1.975; any damage but the player's knocks it over (ours: 1 health)
        {"minecraft:armor_stand", 1.0f, 0.5, 1.975, 0.0, 0.0f, false},
        // wiki: Leash Knot - 0.375 x 0.5 on its fence post
        {"minecraft:leash_knot", 1.0f, 0.375, 0.5, 0.0, 0.0f, false},
        // wiki: Husk - 20 health, 0.6 x 1.95, 0.23, 3 + Hunger; Stray - 20, 0.6 x 1.99, 0.25;
        // Bogged - 16; Parched - 16 (both skeleton-sized).
        {"minecraft:husk", 20.0f, 0.6, 1.95, 0.23, 3.0f, true},
        {"minecraft:stray", 20.0f, 0.6, 1.99, 0.25, 2.0f, true},
        {"minecraft:bogged", 16.0f, 0.6, 1.99, 0.25, 2.0f, true},
        {"minecraft:parched", 16.0f, 0.6, 1.99, 0.25, 2.0f, true},
        // wiki: Skeleton Horse - 15, 1.4 x 1.6, 0.2; Zombie Horse - 25, 0.25 (1.21.11);
        // Camel Husk - 32, 1.7 x 2.375, 0.09; Zombie Nautilus - 15, 0.875 x 0.95, swims.
        {"minecraft:skeleton_horse", 15.0f, 1.3965, 1.6, 0.2, 0.0f, false},
        {"minecraft:zombie_horse", 25.0f, 1.3965, 1.6, 0.25, 0.0f, false},
        {"minecraft:camel_husk", 32.0f, 1.7, 2.375, 0.09, 0.0f, false},
        {"minecraft:zombie_nautilus", 15.0f, 0.875, 0.95, 0.3, 3.0f, false, false, false, 1.0f, true},
        // wiki: Bat - 6, 0.5 x 0.9, flies; Endermite - 8, 0.4 x 0.3, 0.25, 2; Mooshroom - as a
        // cow; Snow Golem - 4, 0.7 x 1.9, 0.2; Piglin Brute - 50, 0.6 x 1.95, 0.35, 13 (golden
        // axe, Normal); Zoglin - 40, 1.3965 x 1.4, 0.25, 6; Illusioner - 32, 0.6 x 1.95, 0.5.
        {"minecraft:bat", 6.0f, 0.5, 0.9, 0.1, 0.0f, false, false, true},
        {"minecraft:endermite", 8.0f, 0.4, 0.3, 0.25, 2.0f, true},
        {"minecraft:mooshroom", 10.0f, 0.9, 1.4, 0.2, 0.0f, false},
        {"minecraft:snow_golem", 4.0f, 0.7, 1.9, 0.2, 0.0f, false},
        {"minecraft:piglin_brute", 50.0f, 0.6, 1.95, 0.35, 13.0f, true},
        {"minecraft:zoglin", 40.0f, 1.3965, 1.4, 0.25, 6.0f, true},
        {"minecraft:illusioner", 32.0f, 0.6, 1.95, 0.5, 0.0f, true},
    };
    static_assert(std::size(kInfo) == size_t(MobType::Count));
    return kInfo[static_cast<int>(t)];
}

float maxHealthOf(const MobData& m) {
    if (m.maxHealth > 0.0f) return m.maxHealth;
    if (m.type == MobType::Wolf && m.tamed) return 40.0f; // (wiki: Wolf - 40 once tamed)
    return mobInfo(m.type).maxHealth;
}

} // namespace mc::world
