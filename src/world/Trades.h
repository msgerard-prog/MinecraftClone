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
int offerPrice(const TradeOffer& offer);
// The ItemStacks a trade asks and gives (the sold one with its enchantment).
ItemStack offerBuyA(const TradeOffer& offer);
ItemStack offerBuyB(const TradeOffer& offer);
ItemStack offerSell(const TradeOffer& offer);
// A trade was made: one use, the villager's experience (and a level-up's trades).
// Returns true if the villager levelled up.
bool useOffer(MobData& villager, int offerIndex, Xoroshiro& rng);
// Restocking at the job site (vanilla: up to twice a day): every trade's uses back to 0
// and its demand adjusted: demand + uses - (maxUses - uses), at least 0.
void restock(MobData& villager);

} // namespace mc::world
