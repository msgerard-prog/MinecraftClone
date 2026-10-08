#pragma once

#include "world/Direction.h"
#include "world/Items.h"
#include "world/Random.h"
#include "world/World.h"

#include <climits>
#include <optional>
#include <vector>

namespace mc::world {

// Block updates, scheduled block ticks and redstone (M11; wiki: Block update, Tick ›
// Scheduled tick, Redstone circuits, Redstone Dust, Redstone Torch, Redstone
// Repeater, Lever, Button, Redstone Lamp, Piston).
//
// Every block change made through World::updateBlock (players) or by this class
// notifies the six neighbours in vanilla's order (west, east, down, up, north, south);
// components also notify their neighbours' neighbours, as vanilla's do. Pistons move
// blocks in the block-event phase after the scheduled ticks.
//
// Power follows vanilla's model: a component sends weak power to a neighbour, and may
// strongly power a conductor (an opaque full block). A conductor passes on the strong
// power it receives to every neighbour; dust only reads strongly powered blocks.
struct Weather;

class BlockUpdates final : public BlockUpdateListener {
public:
    explicit BlockUpdates(World& world);
    ~BlockUpdates() override;
    BlockUpdates(const BlockUpdates&) = delete;
    BlockUpdates& operator=(const BlockUpdates&) = delete;

    // Game time of the tick being run (call at the start of each game tick, before
    // players act: their changes schedule ticks relative to it).
    void setTime(int64_t gameTime) { m_now = gameTime; }
    // Scheduled ticks due now (by time, priority, scheduling order), then block events.
    void tick();

    // Right-click: lever, button, repeater delay, dust dot/cross. True if it acted.
    bool use(const BlockPos& p);
    static bool usable(BlockStateId state);

    // The state to place for `state` at `at`, clicked on the face of the block behind
    // that points to `at` (`face`). Wall torches, lever/button faces, repeater and
    // piston facings come from the click and the player's look (yaw/pitch in vanilla
    // degrees); nullopt when the block can't stand there. `hitY`: how high on the
    // clicked face the click was (0..1; side faces' upper half places top slabs/stairs).
    static std::optional<BlockStateId> placement(const World& world, BlockStateId state, const BlockPos& at,
                                                 Direction face, float yaw, float pitch, double hitY = 0.25);

    // Positions changed here since the caller last cleared it (relight / re-mesh), and
    // items dropped by blocks that broke (dust losing its support, pushed torches).
    std::vector<BlockPos>& changed() { return m_changed; }
    // Changes that don't affect light (dust power...): re-mesh only.
    std::vector<BlockPos>& remeshOnly() { return m_remesh; }
    // Fluids flowing (between air, water and lava): re-mesh at once and relight in the
    // background (LightManager's `settling`), so springs flowing in newly loaded
    // chunks don't hold back the light of the chunks still streaming in.
    std::vector<BlockPos>& settling() { return m_settling; }
    struct Drop {
        BlockPos pos;
        ItemStack stack;
        // Or: the block's loot (leaves decaying: saplings, sticks, apples), rolled by
        // gameplay's block drop rules as if broken by hand.
        BlockStateId loot = 0;
    };
    std::vector<Drop>& drops() { return m_drops; }
    // Creative players breaking a piston head don't get the piston back (vanilla).
    void setCreative(bool creative) { m_creative = creative; }

    // Power (public for tests). `signalFrom(p, toward)`: what the block at p gives
    // its neighbour at p + toward. `strongInto(p)`: strong power reaching p.
    int signalFrom(const BlockPos& p, Direction toward) const;
    int strongInto(const BlockPos& p) const;
    int bestNeighbourSignal(const BlockPos& p) const;
    static bool conductor(BlockStateId s);

    // Schedules a tick (ignored if this block already has one pending, as in vanilla).
    void schedule(const BlockPos& p, BlockId block, int ticks, int priority);
    bool hasTick(const BlockPos& p, BlockId block) const;

    void onBlockChanged(const BlockPos& p, BlockStateId old, BlockStateId now) override;

    // Random ticks (M15, RandomTicks.cpp; wiki: Tick › Random tick): each game tick,
    // `speed` random blocks of every 16^3 section in the chunks within `distance` of
    // `centre` (the simulation distance) get a random tick, after the scheduled ticks.
    void setRandomTicks(ChunkPos centre, int distance, int speed) {
        m_rtCentre = centre;
        m_rtDistance = distance;
        m_rtSpeed = speed;
    }
    static constexpr int kDefaultRandomTickSpeed = 3; // game rule random_tick_speed
    // Sky light levels lost to the time of day (0 by day .. 11 at night): growth reads
    // max(block light, sky light - this), vanilla's raw brightness.
    void setSkyDarken(int levels) { m_skyDarken = levels; }
    // Weather (M22.1): rain puts out fires and waters farmland; in the ticking chunks
    // water freezes and snow settles in cold biomes, and thunderstorms strike lightning
    // (its spots, after fire is placed, in lightning() for main: damage, bolts).
    void setWeather(const Weather* weather) { m_weather = weather; }
    const std::vector<BlockPos>& lightning() const { return m_lightning; }
    void strikeLightning(const BlockPos& p); // (also /summon lightning_bolt)
    // Dirt-like blocks saplings can be planted on (wiki: Sapling).
    static bool plantableSoil(BlockStateId s);
    static bool isLeaves(BlockId b);
    static bool sugarCaneCanStay(const World& world, const BlockPos& p);
    // M18.1: a cactus stands on cactus or sand with nothing solid (or lava) beside it;
    // a mushroom on a solid block where the light (sky or block, not dimmed by night)
    // is below 13 (wiki: Cactus, Mushroom).
    static bool cactusCanStay(const World& world, const BlockPos& p);
    static bool mushroomCanStay(const World& world, const BlockPos& p);
    // M19.1: fungi, roots and sprouts stand on nylium, soul soil or dirt-like blocks;
    // weeping vines hang from a block (or vines) above, twisting vines stand on one
    // below (wiki: Crimson Fungus, Nether Sprouts, Weeping Vines, Twisting Vines).
    static bool netherPlantCanStay(const World& world, const BlockPos& p, BlockId plant);
    // Chorus plants (wiki: Chorus Plant): stand on end stone or another plant, or hang
    // off a plant beside them that stands (then nothing may be both above and below);
    // they connect to plants, flowers and the end stone below. Flowers need a plant or
    // end stone below.
    static bool chorusCanStay(const World& world, const BlockPos& p, BlockId block);
    static BlockStateId chorusConnected(const World& world, const BlockPos& p, BlockStateId plant);
    // Iron bars join bars and full solid blocks beside them (wiki: Iron Bars).
    static BlockStateId barsConnected(const World& world, const BlockPos& p, BlockStateId bars);
    // Fences join fences, fence gates and full solid blocks (wiki: Fence).
    static BlockStateId fenceConnected(const World& world, const BlockPos& p, BlockStateId fence);
    // Stairs (M23.1; wiki: Stairs): a corner shape when a stair at right angles sits
    // in front (outer) or behind (inner) with the same half.
    static BlockStateId stairsShaped(const World& world, const BlockPos& p, BlockStateId stairs);
    // Walls (wiki: Wall): arms to walls, bars, panes, fence gates and full blocks; tall
    // under a full block, a post unless a straight run.
    static BlockStateId wallConnected(const World& world, const BlockPos& p, BlockStateId wall);
    // Glass panes (M23.2; wiki: Glass Pane): arms to panes, bars, glass, walls, full blocks.
    static BlockStateId paneConnected(const World& world, const BlockPos& p, BlockStateId pane);
    // Concrete powder -> its concrete (M23.4a; nullopt for other blocks).
    static std::optional<BlockStateId> concreteFor(BlockStateId powder);
    static bool isDoor(BlockId b);
    static bool isPressurePlate(BlockId b);

    // Pressure plates (M21.1; wiki: Pressure Plate): gameplay reports each entity on a
    // plate during the tick (`item`: a dropped item or arrow - only wooden and weighted
    // plates feel those), then settlePlates() presses them. A plate stays down while
    // something is on it, checked every 20 ticks (weighted: 10); weighted plates give
    // min(15, n) (gold) or ceil(min(n, 150) / 10) (iron).
    void pressPlate(const BlockPos& p, bool item, bool minecart = false); // (detector rails: minecarts only)
    // TNT (M21.1b; wiki: TNT): redstone power, fire, flint and steel light it - the
    // block goes and gameplay spawns primed TNT where primedTnt() lists.
    void primeTnt(const BlockPos& p);
    // Dispensers and droppers fired this tick (M21.3b): a rising edge of power (also
    // one block above them: quasi-connectivity) fires them 4 ticks later; gameplay does
    // what their item does.
    std::vector<BlockPos>& dispensed() { return m_dispensed; }
    // Blocks in flight (M21.5; wiki: Piston › Behavior, Moving Piston): a push or pull
    // takes 2 ticks; meanwhile the target cell holds an invisible moving_piston and the
    // renderer draws the block sliding from `to - dir` to `to`. `visual`: the retracting
    // head (nothing lands). Entities in a target cell are carried along (main).
    struct Moving {
        BlockPos to;
        BlockStateId state;
        Direction dir;
        int64_t start;
        bool visual = false;
    };
    const std::vector<Moving>& moving() const { return m_moving; }
    // Lands every block in flight now (before saving or leaving the dimension).
    void landAll() {
        finishMoves(true);
        m_moving.clear();
    }
    int64_t now() const { return m_now; }
    std::vector<BlockPos>& primedTnt() { return m_tntPrimed; }
    void settlePlates();
    // Chests (M17.2): the other half of a double chest, if any; partner side rule.
    static std::optional<BlockPos> chestPartner(const World& world, const BlockPos& p);
    static Direction chestClockwise(Direction facing);
    // Farming (M17.1, Farming.cpp).
    static bool isCrop(BlockId b);
    static int cropMaxAge(BlockId b);
    static int cropAge(BlockStateId s);
    // A hoe used on `face` of the block at p (dirt/grass -> farmland). True if it acted.
    static bool till(World& world, const BlockPos& p, Direction face);
    // An axe strips a log, wood, stem, hyphae or bamboo block (M23.3b; wiki: Axe ›
    // Stripping): the stripped block with the same axis. True if it did.
    static bool strip(World& world, const BlockPos& p);
    // Copper (M23.4b, Copper.cpp): honeycomb waxes it, an axe scrapes it (wax off, or a
    // stage of oxidation). True if it acted.
    static bool waxCopper(World& world, const BlockPos& p);
    static bool scrapeCopper(World& world, const BlockPos& p);
    static bool isCopper(BlockId b);
    // Composters and cauldrons (M23.5, Workstations.cpp). compost: an item put in (true
    // if used up; hoppers too); takeCompost: a full composter's bone meal (else empty);
    // useCauldron: what one held bucket/bottle becomes, or nullopt if nothing happened.
    static int compostChance(ItemId item); // percent, 0 = not compostable
    bool compost(const BlockPos& p, ItemId item);
    ItemStack takeCompost(const BlockPos& p);
    std::optional<ItemStack> useCauldron(const BlockPos& p, const ItemStack& held);
    static int cauldronSignal(BlockStateId s); // comparator level; -1: not one of them
    // Note blocks (M23.6, NoteBlocks.cpp): the instrument a block below gives; playing
    // one (a left click, a power pulse; right-click tunes it up a note first).
    static int noteInstrument(BlockStateId below);
    void playNote(const BlockPos& p);
    // A jukebox's song started or ended: its redstone output changed (call from main).
    void jukeboxChanged(const BlockPos& p);
    // Bone meal used on the block at p. True if it was used up.
    bool boneMeal(const BlockPos& p);
    // An entity landed hard on this farmland (the caller rolls the chance).
    void trample(const BlockPos& farmland);
    // A crop's growth speed level (public for tests).
    float growthPoints(const BlockPos& p, BlockId crop) const;
    // Falling blocks (M16; wiki: Falling Block): sand, red sand and gravel fall 2 ticks
    // after the block below becomes free (air, fire, fluid, replaceable plants). The
    // block is removed and listed here for gameplay to turn into a falling entity.
    static bool hasGravity(BlockId b);
    static bool fallThrough(BlockStateId below); // vanilla FallingBlock "free" below
    static bool replaceable(BlockStateId s);     // a landing block may take its place
    struct FallStart {
        BlockPos pos;
        BlockStateId state;
    };
    std::vector<FallStart>& fallingStarts() { return m_falling; }
    // Fire (M15, Fire.cpp; wiki: Fire › Flammable blocks): ignite odds (how readily fire
    // spreads next to a block) and burn odds (how fast it destroys it); 0 = never.
    static int igniteOdds(BlockId b);
    static int burnOdds(BlockId b);
    static BlockStateId fireState(int age);
    bool fireSurvives(const BlockPos& p) const;
    // Whether fire placed at p stays (solid below or a flammable neighbour).
    static bool fireCanStay(const World& world, const BlockPos& p);
    // The player's position: fire only acts within kFireRadius blocks of it (1.21.11
    // game rule fire_spread_radius_around_player). Unset (tests): everywhere.
    void setPlayer(const glm::dvec3& feet) { m_player = feet; }
    static constexpr int kFireRadius = 128;
    static bool isLog(BlockId b);

    // Fluids (M14, Fluids.cpp). Amount 1..8 (8 = source or falling); 0 if not a fluid.
    static bool isFluid(BlockId b);
    static int fluidAmount(BlockStateId s);
    static BlockStateId fluidState(BlockId kind, int amount, bool falling);
    static bool breaksInFluid(BlockId b); // washed away (plants, torches, redstone...)
    static bool isOceanPlant(BlockId b);  // kelp, seagrass, sea pickles, corals (M25.1)
    // Turtle eggs that hatched this tick (M25.3b): main adds that many baby turtles
    // there, at home on that spot. Cleared by the caller.
    struct Hatch {
        BlockPos pos;
        int count;
    };
    std::vector<Hatch>& hatched() { return m_hatched; }

private:
    void runRandomTicks();
    void runWeatherTicks();
    bool rainingNear(const BlockPos& p) const; // on p or a horizontal neighbour
    bool hardenPowder(const BlockPos& p, BlockStateId s); // true if it turned into concrete
    void tickCopper(const BlockPos& p, BlockStateId s);    // oxidation (Copper.cpp)
    void updateBulb(const BlockPos& p, BlockStateId s);    // copper bulbs on power changes
    void fillCauldronByWeather(const BlockPos& p, bool snow); // rain/snow into a cauldron
    void noteBlockChanged(const BlockPos& p, BlockStateId s);  // instrument, power edges
    void pistonDrops(const BlockPos& p, BlockStateId s);        // what a piston-broken block drops
    int jukeboxPower(const BlockPos& q) const;                   // 15 while playing
    // Lava and water meeting: the hiss and a puff of smoke (wiki: Lava).
    void fizz(const BlockPos& p) {
        m_world.playSound(Sound::Fizz, p.x + 0.5, p.y + 0.5, p.z + 0.5);
        m_world.levelEvent(LevelEvent::Type::Extinguish, p.x + 0.5, p.y + 0.6, p.z + 0.5);
    }
    void randomTick(const BlockPos& p, BlockStateId s);
    // Ocean blocks (M25.1, Ocean.cpp).
    bool oceanSurvives(const BlockPos& p, BlockStateId s) const;
    bool oceanNeighbourChanged(const BlockPos& p, BlockStateId s); // true: an ocean block, handled
    bool coralWet(const BlockPos& p, BlockStateId s) const;
    bool tickOcean(const BlockPos& p, BlockStateId s); // true: a coral's tick, handled
    void growKelp(const BlockPos& p, BlockStateId s);
    void tickTurtleEgg(const BlockPos& p, BlockStateId s);
    int rawBrightness(const BlockPos& p) const;
    int blockLightAt(const BlockPos& p) const;
    bool grassSurvives(const BlockPos& p) const;
    void tickGrass(const BlockPos& p, BlockId kind); // grass or mycelium (same rules)
    int leafDistance(const BlockPos& p) const;
    void leavesChanged(const BlockPos& p, BlockStateId s);
    bool growTree(const BlockPos& p, BlockStateId sapling);
    bool nearWater(const BlockPos& p) const;
    void tickFarmland(const BlockPos& p, BlockStateId s);
    void tickCrop(const BlockPos& p, BlockStateId s);
    bool nextToFlammable(const BlockPos& p) const;
    void placeFire(const BlockPos& p, int age);
    void fireNeighbourChanged(const BlockPos& p);
    void burnNeighbour(const BlockPos& q, int bound, int fireAge);
    void tickFire(const BlockPos& p, BlockStateId s);
    void lavaIgnites(const BlockPos& p);
    bool nearPlayer(const BlockPos& p) const;

    enum class FluidInto { No, Empty, Same, Breaks };
    int fluidDelay(BlockId kind) const;
    int fluidDrop(BlockId kind) const;
    int slopeFindDistance(BlockId kind) const;
    FluidInto fluidInto(const BlockPos& p, BlockId kind) const;
    bool isHole(const BlockPos& p, BlockId kind) const;
    BlockStateId newFluidState(const BlockPos& p, BlockId kind) const;
    bool lavaMeetsWater(const BlockPos& p, BlockStateId s);
    void fluidNeighbourChanged(const BlockPos& p, BlockStateId s);
    void placeFluid(const BlockPos& p, BlockStateId state);
    void waterloggedFlow(const BlockPos& p);
    void tickFluid(const BlockPos& p, BlockStateId s);
    int slopeDistance(const BlockPos& p, int depth, Direction from, BlockId kind) const;
    void spreadSideways(const BlockPos& p, BlockStateId s);

    struct Due {
        BlockPos pos;
        Chunk::BlockTick tick;
    };
    struct Event {
        BlockPos pos;
        bool extend;
        bool nextTick = false; // caused by a player: runs a tick later
    };

    // Block reads go through a one-chunk cache (most queries stay in one chunk).
    BlockStateId at(const BlockPos& p) const {
        if (!m_world.isInHeight(p.y)) return 0;
        const ChunkPos cp = p.chunk();
        if (m_cacheEpoch != m_world.chunkEpoch() || !(m_cachePos == cp)) {
            m_cache = m_world.chunk(cp);
            m_cachePos = cp;
            m_cacheEpoch = m_world.chunkEpoch();
        }
        return m_cache ? m_cache->get(blockToLocal(p.x), p.y, blockToLocal(p.z)) : BlockStateId{0};
    }
    // The chunk holding p through the same one-chunk cache (nullptr if unloaded).
    Chunk* chunkAt(const BlockPos& p) const {
        const ChunkPos cp = p.chunk();
        if (m_cacheEpoch != m_world.chunkEpoch() || !(m_cachePos == cp)) {
            m_cache = m_world.chunk(cp);
            m_cachePos = cp;
            m_cacheEpoch = m_world.chunkEpoch();
        }
        return const_cast<Chunk*>(m_cache);
    }
    void set(const BlockPos& p, BlockStateId s);    // with updates
    void setRaw(const BlockPos& p, BlockStateId s); // no updates (piston moves)
    void setDiode(const BlockPos& p, BlockStateId s); // repeater on/off: front updates only
    void record(const BlockPos& p, BlockStateId old, BlockStateId now);
    void afterChange(const BlockPos& p, BlockStateId old, BlockStateId now);
    void notifyNeighbours(const BlockPos& p);
    void reach(const BlockPos& p, BlockStateId s); // a component's extra update range
    void makeAbsolute(Chunk& c);                  // loaded tick delays -> game times
    void neighbourChanged(const BlockPos& p);
    void pop(const BlockPos& p); // breaks the block with its drop
    void tickBlock(const BlockPos& p, BlockStateId s);

    int weak(BlockStateId s, Direction toward) const;
    int strong(BlockStateId s, Direction toward) const;
    int wirePower(const BlockPos& p) const;
    BlockStateId wireShape(const BlockPos& p, BlockStateId s) const;
    int wireTarget(const BlockPos& p) const;
    void updateWire(const BlockPos& p);
    bool survives(const BlockPos& p, BlockStateId s) const;
    bool torchInput(const BlockPos& p, BlockStateId s) const;
    int repeaterInput(const BlockPos& p, BlockStateId s) const;
    bool repeaterLocked(const BlockPos& p, BlockStateId s) const;
    bool toggledTooOften(const BlockPos& p, bool add);
    bool pistonPowered(const BlockPos& p, Direction facing) const;
    bool pushList(const BlockPos& base, Direction f, std::optional<BlockPos>& destroy);
    void extend(const BlockPos& p);
    void retract(const BlockPos& p);

    World& m_world;
    int64_t m_now = 0;
    Xoroshiro m_random{0x5eed'7a11'0b10'cc5ull}; // random ticks (not worldgen: any sequence)
    ChunkPos m_rtCentre{0, 0};
    int m_rtDistance = -1; // no random ticks until set
    int m_rtSpeed = kDefaultRandomTickSpeed;
    int m_skyDarken = 0;
    const Weather* m_weather = nullptr;
    std::vector<BlockPos> m_lightning;
    std::optional<glm::dvec3> m_player;
    uint64_t m_order = 0;
    mutable bool m_wiresMuted = false; // dust ignores other dust's power through blocks
    int m_depth = 0;                   // update recursion guard
    bool m_inTick = false;             // inside tick() (else: players acting)
    mutable const Chunk* m_cache = nullptr;
    mutable ChunkPos m_cachePos{INT32_MIN, INT32_MIN};
    mutable uint64_t m_cacheEpoch = ~0ull;
    bool m_creative = false;
    std::vector<Due> m_due;
    std::vector<Event> m_events;
    std::vector<BlockPos> m_changed;
    std::vector<BlockPos> m_remesh;
    std::vector<BlockPos> m_settling;
    std::vector<Hatch> m_hatched; // (M25.3b)
    std::vector<Drop> m_drops;
    std::vector<FallStart> m_falling;
    std::vector<BlockPos> m_push;      // blocks a piston moves (reused)
    std::vector<BlockStateId> m_pushStates;
    struct Toggle {
        BlockPos pos;
        int64_t time;
    };
    std::vector<Toggle> m_toggles; // redstone torch burnout
    struct Plate {
        BlockPos pos;
        int64_t time; // the last tick something was on it
        int count;    // entities on it during that tick
    };
    std::vector<Plate> m_plates;
    std::vector<BlockPos> m_tntPrimed;
    std::vector<BlockPos> m_dispensed;
    std::vector<Moving> m_moving;
    std::vector<BlockPos> m_pushDestroy;
    void finishMoves(bool force = false);
    void alertObservers(const BlockPos& p);
    bool gatherPush(const BlockPos& base, const BlockPos& first, Direction move, std::vector<BlockPos>& destroy);
    int plateTarget(BlockId b, int count) const;
    // Comparators and observers (M21.2).
    int weakAt(const BlockPos& q, Direction toward) const;   // weak() with block entities
    int strongAt(const BlockPos& q, Direction toward) const; // strong() likewise
    int containerSignal(const BlockPos& p) const;            // -1: not a container
    int comparatorTarget(const BlockPos& p, BlockStateId s) const;
    void comparatorChanged(const BlockPos& p, BlockStateId s);
    void watchComparators();
    void setDoor(const BlockPos& lower, BlockStateId lowerState, bool open, bool poweredNow);
    bool railPowered(const BlockPos& p, BlockStateId s) const;
};

} // namespace mc::world
