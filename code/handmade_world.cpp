
// TODO(Kevin): think about what the real safe margin is
#define TILE_CHUNK_SAFE_MARGIN (INT32_MAX/64)
#define TILE_CHUNK_UNINITIALIZED INT32_MAX

inline world_chunk *
GetWorldChunk(world *World, int32 WorldChunkX, int32 WorldChunkY, int32 WorldChunkZ,
              memory_arena *Arena = 0)
{
    Assert(WorldChunkX > -TILE_CHUNK_SAFE_MARGIN);
    Assert(WorldChunkY > -TILE_CHUNK_SAFE_MARGIN);
    Assert(WorldChunkZ > -TILE_CHUNK_SAFE_MARGIN);
    Assert(WorldChunkX < TILE_CHUNK_SAFE_MARGIN);
    Assert(WorldChunkY < TILE_CHUNK_SAFE_MARGIN);
    Assert(WorldChunkZ < TILE_CHUNK_SAFE_MARGIN);

    // TODO(Kevin): BETTER HASH FUNCTION!!!!
    uint32 HashValue = (19*WorldChunkX + 7*WorldChunkY + 3*WorldChunkZ);
    uint32 HashSlot = HashValue & (ArrayCount(World->WorldChunkHash) - 1);
    Assert(HashSlot < ArrayCount(World->WorldChunkHash));

    world_chunk *WorldChunk = World->WorldChunkHash + HashSlot;
    do
    {
        if (WorldChunkX == WorldChunk->ChunkX &&
            WorldChunkY == WorldChunk->ChunkY &&
            WorldChunkZ == WorldChunk->ChunkZ)
        {
            break;
        }

        if (Arena && (WorldChunk->ChunkX != TILE_CHUNK_UNINITIALIZED) && (!WorldChunk->NextInHash))
        {
            WorldChunk->NextInHash = PushStruct(Arena, world_chunk);
            WorldChunk = WorldChunk->NextInHash;
            WorldChunk->ChunkX = TILE_CHUNK_UNINITIALIZED;
        }

        if (Arena && (WorldChunk->ChunkX == TILE_CHUNK_UNINITIALIZED))
        {
            uint32 TileCount = World->ChunkDim*World->ChunkDim;

            WorldChunk->ChunkX = WorldChunkX;
            WorldChunk->ChunkY = WorldChunkY;
            WorldChunk->ChunkZ = WorldChunkZ;

            WorldChunk->NextInHash = 0;

            break;
        }

        WorldChunk = WorldChunk->NextInHash;
    } while(WorldChunk);

    return WorldChunk;
}

internal void
InitializeWorld(world *World, real32 TileSideInMeters)
{
    World->ChunkShift = 4;
    World->ChunkMask = (1 << World->ChunkShift) - 1;
    World->ChunkDim = (1 << World->ChunkShift);
    World->TileSideInMeters = TileSideInMeters;

    for (uint32 WorldChunkIndex = 0;
         WorldChunkIndex < ArrayCount(World->WorldChunkHash);
         ++WorldChunkIndex)
    {
        World->WorldChunkHash[WorldChunkIndex].ChunkX = TILE_CHUNK_UNINITIALIZED;
    }
}

#if 0
inline world_position
GetChunkPositionFor(world *World, uint32 AbsTileX, uint32 AbsTileY, uint32 AbsTileZ)
{
    world_position Result;

    Result.WorldChunkX = AbsTileX >> World->ChunkShift;
    Result.WorldChunkY = AbsTileY >> World->ChunkShift;
    Result.WorldChunkZ = AbsTileZ;
    Result.RelTileX = AbsTileX & World->ChunkMask;
    Result.RelTileY = AbsTileY & World->ChunkMask;

    return Result;
}
#endif

inline void
RecanonicalizeCoord(world *World, int32 *Tile, real32 *TileRel)
{
    // TODO(Kevin): Need to do something that doens't use the divide/multiply method
    // for recanonicalizing because this can end up rounding back on to the tile you
    // just came from.

    // NOTE(Kevin): World is assumed to be toroidal topology, if you
    // step off one end you come back on the other!
    int32 Offset = RoundReal32ToInt32(*TileRel / World->TileSideInMeters);
    *Tile += Offset;
    *TileRel -= Offset * World->TileSideInMeters;

    Assert(*TileRel >= -0.5f*World->TileSideInMeters);
    Assert(*TileRel <= 0.5f*World->TileSideInMeters);
}

inline world_position
MapIntoTileSpace(world *World, world_position BasePos, v2 Offset)
{
    world_position Result = BasePos;

    Result.Offset_ += Offset;
    RecanonicalizeCoord(World, &Result.AbsTileX, &Result.Offset_.X);
    RecanonicalizeCoord(World, &Result.AbsTileY, &Result.Offset_.Y);

    return Result;
}

inline bool32
AreOnSameTile(world_position *A, world_position *B)
{
    bool32 Result = ((A->AbsTileX == B->AbsTileX) &&
                     (A->AbsTileY == B->AbsTileY) &&
                     (A->AbsTileZ == B->AbsTileZ));
    return Result;
}

inline world_difference
Subtract(world *World, world_position *A, world_position *B)
{
    world_difference Result;

    v2 dTileXY = { (real32)A->AbsTileX - (real32)B->AbsTileX,
                   (real32)A->AbsTileY - (real32)B->AbsTileY };
    real32 dTileZ = (real32)A->AbsTileZ - (real32)B->AbsTileZ;
    
    Result.dXY = World->TileSideInMeters*dTileXY + (A->Offset_ - B->Offset_);

    // TODO(Kevin): Think about what we want to do about Z
    Result.dZ = World->TileSideInMeters*dTileZ;

    return(Result);
}
