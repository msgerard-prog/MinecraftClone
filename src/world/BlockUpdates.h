#pragma once

#include "world/Direction.h"
#include "world/Items.h"
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
    // degrees); nullopt when the block can't stand there.
    static std::optional<BlockStateId> placement(const World& world, BlockStateId state, const BlockPos& at,
                                                 Direction face, float yaw, float pitch);

    // Positions changed here since the caller last cleared it (relight / re-mesh), and
    // items dropped by blocks that broke (dust losing its support, pushed torches).
    std::vector<BlockPos>& changed() { return m_changed; }
    // Changes that don't affect light (dust power...): re-mesh only.
    std::vector<BlockPos>& remeshOnly() { return m_remesh; }
    struct Drop {
        BlockPos pos;
        ItemStack stack;
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

    // Fluids (M14, Fluids.cpp). Amount 1..8 (8 = source or falling); 0 if not a fluid.
    static bool isFluid(BlockId b);
    static int fluidAmount(BlockStateId s);
    static BlockStateId fluidState(BlockId kind, int amount, bool falling);
    static bool breaksInFluid(BlockId b); // washed away (plants, torches, redstone...)

private:
    enum class FluidInto { No, Empty, Same, Breaks };
    int fluidDelay(BlockId kind) const;
    int fluidDrop(BlockId kind) const;
    int slopeFindDistance(BlockId kind) const;
    FluidInto fluidInto(BlockStateId target, BlockId kind) const;
    bool isHole(const BlockPos& p, BlockId kind) const;
    BlockStateId newFluidState(const BlockPos& p, BlockId kind) const;
    bool lavaMeetsWater(const BlockPos& p, BlockStateId s);
    void fluidNeighbourChanged(const BlockPos& p, BlockStateId s);
    void placeFluid(const BlockPos& p, BlockStateId state);
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
    std::vector<Drop> m_drops;
    std::vector<BlockPos> m_push;      // blocks a piston moves (reused)
    std::vector<BlockStateId> m_pushStates;
    struct Toggle {
        BlockPos pos;
        int64_t time;
    };
    std::vector<Toggle> m_toggles; // redstone torch burnout
};

} // namespace mc::world
