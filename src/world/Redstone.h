#pragma once

#include "world/Direction.h"
#include "world/Items.h"
#include "world/World.h"

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
class Redstone final : public BlockUpdateListener {
public:
    explicit Redstone(World& world);
    ~Redstone() override;
    Redstone(const Redstone&) = delete;
    Redstone& operator=(const Redstone&) = delete;

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

private:
    struct Due {
        BlockPos pos;
        Chunk::BlockTick tick;
    };
    struct Event {
        BlockPos pos;
        bool extend;
    };

    BlockStateId at(const BlockPos& p) const { return m_world.getBlock(p); }
    void set(const BlockPos& p, BlockStateId s);    // with updates
    void setRaw(const BlockPos& p, BlockStateId s); // no updates (piston moves)
    void afterChange(const BlockPos& p, BlockStateId old, BlockStateId now);
    void notifyNeighbours(const BlockPos& p);
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
    bool m_creative = false;
    std::vector<Due> m_due;
    std::vector<Event> m_events;
    std::vector<BlockPos> m_changed;
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
