#ifndef HANDMADE_WORLD_H
#define HANDMADE_WORLD_H

// TODO(Kevin): Replace this with a v3 once we get to v3
struct world_difference
{
    v2 dXY;
    real32 dZ;
};

struct world_position
{
    // TODO(Kevin): How can we get rid of abstile* here,
    // and still allow references to entities to be able
    // to figure out where they are (or rather, which world
    // chunk they are in?)

    // NOTE(Kevin): These are fixed point tile locations.
    // The high bits are the tile chunk index, and the low
    // bits are the tile index in the chunk.
    int32 AbsTileX;
    int32 AbsTileY;
    int32 AbsTileZ;

    // NOTE(Kevin): These are offsets from the tile center
    v2 Offset_;
};

// TODO(Kevin): could make this just tile_chunk and allow multiple tile chunks per XYZ
struct world_entity_block
{
    uint32 EntityCount;
    uint32 LowEntityIndex[16];
    world_entity_block *Next;
};

struct world_chunk
{
    int32 ChunkX, ChunkY, ChunkZ;

    world_entity_block FirstBlock;

    world_chunk *NextInHash;
};

struct world
{
    real32 TileSideInMeters;

    // TODO(Kevin): WorldChunkHash should probably switch to pointers, IF
    // tile entity blocks continue to be stored en masse directly in the world chunk!
    // NOTE(Kevin): at the moment, this must be a power of two!
    int32 ChunkShift;
    int32 ChunkMask;
    int32 ChunkDim;
    world_chunk WorldChunkHash[4096];
};

#endif // HANDMADE_WORLD_H
