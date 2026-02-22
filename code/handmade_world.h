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

    int32 ChunkX;
    int32 ChunkY;
    int32 ChunkZ;

    // NOTE(Kevin): These are offsets from the chunk center
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

    // TODO(Kevin): profile this and determine if a pointer would be better here
    world_entity_block FirstBlock;

    world_chunk *NextInHash;
};

struct world
{
    real32 TileSideInMeters;
    real32 ChunkSideInMeters;

    world_entity_block *FirstFree;

    // TODO(Kevin): WorldChunkHash should probably switch to pointers, IF
    // tile entity blocks continue to be stored en masse directly in the world chunk!
    // NOTE(Kevin): at the moment, this must be a power of two!
    world_chunk WorldChunkHash[4096];
};

#endif // HANDMADE_WORLD_H
