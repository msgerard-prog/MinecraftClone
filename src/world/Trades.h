#pragma once

#include "world/Items.h"
#include "world/Mob.h"
#include "world/Random.h"

namespace mc::world {

// Villager trading (M24.2; wiki: Trading). Each profession has a pool of trades per
// level; a villager gets 2 from its level's pool when it reaches that level (1 at
// master for some). Trades whose items aren't in the game yet are skipped.
//
// Experience to the next level (wiki: Trading › Villager experience): 10 apprentice,
// 70 journeyman, 150 expert, 250 master.
int villagerLevelFor(int xp);
// Adds the new level's trades to a villager (on taking a profession and on levelling).
void addLevelTrades(MobData& villager, Xoroshiro& rng);
// The first stack's price now: its base count, raised by demand (base x multiplier x
// demand) and lowered by special prices; at least 1, at most the stack size.
// `heroLevel`: the buyer's Hero of the Village level (0: none) - vanilla takes
// 30% + 6.25% per level above I of the base price off, at least 1 (wiki: Hero of the Village).
// (M32.5) `reputation`: the villager's of the player - vanilla takes floor(reputation x the
// trade's price multiplier) off (or adds it, when negative).
int offerPrice(const TradeOffer& offer, int heroLevel = 0, int reputation = 0);
// The ItemStacks a trade asks and gives (the sold one with its enchantment).
ItemStack offerBuyA(const TradeOffer& offer, int heroLevel = 0, int reputation = 0);
ItemStack offerBuyB(const TradeOffer& offer);
ItemStack offerSell(const TradeOffer& offer);
// A trade was made: one use, the villager's experience (and a level-up's trades).
// Returns true if the villager levelled up.
bool useOffer(MobData& villager, int offerIndex, Xoroshiro& rng);
// A wandering trader's wares (wiki: Wandering Trader › Trades): 5 of its common
// trades, 1 rare one and 2 things it buys.
void wanderingTraderTrades(MobData& trader, Xoroshiro& rng);
// Restocking at the job site (vanilla: up to twice a day): every trade's uses back to 0
// and its demand adjusted: demand + uses - (maxUses - uses), at least 0.
void restock(MobData& villager);

// Gossip (M32.5; wiki: Villager › Gossiping): adds to one kind (up to its maximum), the
// villager's reputation of the player, and a day's decay.
void addGossip(MobData& villager, Gossip kind, int amount);
int reputation(const MobData& villager);
void decayGossip(MobData& villager);

} // namespace mc::world
