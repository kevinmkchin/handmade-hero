
// TODO(Kevin): think about what the real safe margin is
#define TILE_CHUNK_SAFE_MARGIN (INT32_MAX/64)
#define TILE_CHUNK_UNINITIALIZED INT32_MAX
#define TILES_PER_CHUNK 16

inline world_position
NullPosition()
{
    world_position Result = {};

    Result.ChunkX = TILE_CHUNK_UNINITIALIZED;

    return Result;
}

inline bool32
IsValid(world_position P)
{
    bool32 Result = (P.ChunkX != TILE_CHUNK_UNINITIALIZED);
    return Result;
}

inline bool32
IsCanonical(world *World, real32 TileRel)
{
    // TODO(Kevin): fix floating point math so this can be exact?
    bool32 Result = ((TileRel >= -0.5f*World->ChunkSideInMeters) &&
                     (TileRel <= 0.5f*World->ChunkSideInMeters));
    return Result;
}

inline bool32
IsCanonical(world *World, v2 Offset)
{
    bool32 Result = (IsCanonical(World, Offset.X) && IsCanonical(World, Offset.Y));
    return Result;
}

inline bool32
AreInSameChunk(world *World, world_position *A, world_position *B)
{
    Assert(IsCanonical(World, A->Offset_));
    Assert(IsCanonical(World, B->Offset_));

    bool32 Result = ((A->ChunkX == B->ChunkX) &&
                     (A->ChunkY == B->ChunkY) &&
                     (A->ChunkZ == B->ChunkZ));
    return Result;
}

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
    World->TileSideInMeters = TileSideInMeters;
    World->ChunkSideInMeters = (real32)TILES_PER_CHUNK*TileSideInMeters;
    World->FirstFree = 0;

    for (uint32 WorldChunkIndex = 0;
         WorldChunkIndex < ArrayCount(World->WorldChunkHash);
         ++WorldChunkIndex)
    {
        World->WorldChunkHash[WorldChunkIndex].ChunkX = TILE_CHUNK_UNINITIALIZED;
        World->WorldChunkHash[WorldChunkIndex].FirstBlock.EntityCount = 0;
        World->WorldChunkHash[WorldChunkIndex].FirstBlock.Next = 0;
        World->WorldChunkHash[WorldChunkIndex].NextInHash = 0;
    }
}

inline void
RecanonicalizeCoord(world *World, int32 *Chunk, real32 *ChunkRel)
{
    // TODO(Kevin): Need to do something that doens't use the divide/multiply method
    // for recanonicalizing because this can end up rounding back on to the chunk you
    // just came from.

    // NOTE(Kevin): Wrapping IS NOT ALLOWED, so all coordinates are assumed to be
    // within the safe margin!
    // TODO(Kevin): Assert that we are nowhere near the edges of the world.
    int32 Offset = RoundReal32ToInt32(*ChunkRel / World->ChunkSideInMeters);
    *Chunk += Offset;
    *ChunkRel -= Offset * World->ChunkSideInMeters;

    Assert(IsCanonical(World, *ChunkRel));
}

inline world_position
MapIntoChunkSpace(world *World, world_position BasePos, v2 Offset)
{
    world_position Result = BasePos;

    Result.Offset_ += Offset;
    RecanonicalizeCoord(World, &Result.ChunkX, &Result.Offset_.X);
    RecanonicalizeCoord(World, &Result.ChunkY, &Result.Offset_.Y);

    return Result;
}

inline world_position
ChunkPositionFromTilePosition(world *World, int32 AbsTileX, int32 AbsTileY, int32 AbsTileZ)
{
    world_position Result = {};

    Result.ChunkX = AbsTileX / TILES_PER_CHUNK;
    Result.ChunkY = AbsTileY / TILES_PER_CHUNK;
    Result.ChunkZ = AbsTileZ / TILES_PER_CHUNK;

    // TODO(Kevin): fix
    if (AbsTileX < 0)
    {
        --Result.ChunkX;
    }
    if (AbsTileY < 0)
    {
        --Result.ChunkY;
    }
    if (AbsTileZ < 0)
    {
        --Result.ChunkZ;
    }

    // TODO(Kevin): Decide on tile alignment in chunks
    Result.Offset_.X = (real32)((AbsTileX - TILES_PER_CHUNK/2) - (Result.ChunkX * TILES_PER_CHUNK)) * World->TileSideInMeters;
    Result.Offset_.Y = (real32)((AbsTileY - TILES_PER_CHUNK/2) - (Result.ChunkY * TILES_PER_CHUNK)) * World->TileSideInMeters;
    // TODO(Kevin): Move to 3D Z!

    Assert(IsCanonical(World, Result.Offset_));

    return Result;
}


inline world_difference
Subtract(world *World, world_position *A, world_position *B)
{
    world_difference Result;

    v2 dChunkXY = { (real32)A->ChunkX - (real32)B->ChunkX,
                   (real32)A->ChunkY - (real32)B->ChunkY };
    real32 dChunkZ = (real32)A->ChunkZ - (real32)B->ChunkZ;
    
    Result.dXY = World->ChunkSideInMeters*dChunkXY + (A->Offset_ - B->Offset_);

    // TODO(Kevin): Think about what we want to do about Z
    Result.dZ = World->ChunkSideInMeters*dChunkZ;

    return(Result);
}

inline void
ChangeEntityLocationRaw(memory_arena *Arena, world *World, uint32 LowEntityIndex,
                        world_position *OldP, world_position *NewP)
{
    // TODO(Kevin): if this moves an entity into camera bounds, should it automatically
    //              go into the high set immediately?

    Assert(!OldP || IsValid(*OldP));

    if (OldP && AreInSameChunk(World, OldP, NewP))
    {
        // NOTE(Kevin): Leave entity where it is
    }
    else
    {
        if (OldP)
        {
            // NOTE(Kevin): pull entity out of its current entity block
            world_chunk *Chunk = GetWorldChunk(World, OldP->ChunkX, OldP->ChunkY, OldP->ChunkZ);
            Assert(Chunk);
            if (Chunk)
            {
                bool32 NotFound = true;
                for (world_entity_block *Block = &Chunk->FirstBlock;
                     Block && NotFound;
                     Block = Block->Next)
                {
                    world_entity_block *FirstBlock = &Chunk->FirstBlock;
                    for (uint32 Index = 0;
                         (Index < Block->EntityCount) && NotFound;
                         ++Index)
                    {
                        if (Block->LowEntityIndex[Index] == LowEntityIndex)
                        {
                            Block->LowEntityIndex[Index] = FirstBlock->LowEntityIndex[--FirstBlock->EntityCount];

                            if (FirstBlock->EntityCount == 0)
                            {
                                if (FirstBlock->Next)
                                {
                                    world_entity_block *NextBlock = FirstBlock->Next;
                                    *FirstBlock = *NextBlock;

                                    NextBlock->Next = World->FirstFree;
                                    World->FirstFree = NextBlock;
                                }
                            }

                            NotFound = false;
                        }
                    }
                }
            }
        }

        if (NewP)
        {
            // NOTE(Kevin): insert entity into its new entity block
            world_chunk *Chunk = GetWorldChunk(World, NewP->ChunkX, NewP->ChunkY, NewP->ChunkZ, Arena);
            Assert(Chunk);

            world_entity_block *Block = &Chunk->FirstBlock;
            if (Block->EntityCount == ArrayCount(Block->LowEntityIndex))
            {
                // NOTE(Kevin): We're out of room, get a new block
                world_entity_block *OldBlock = World->FirstFree;
                if (OldBlock)
                {
                    World->FirstFree = OldBlock->Next;
                }
                else
                {
                    OldBlock = PushStruct(Arena, world_entity_block);
                }

                *OldBlock = *Block;
                Block->Next = OldBlock;
                Block->EntityCount = 0;
            }

            Assert(Block->EntityCount < ArrayCount(Block->LowEntityIndex));
            Block->LowEntityIndex[Block->EntityCount++] = LowEntityIndex;
        }
    }
}

internal void
ChangeEntityLocation(memory_arena *Arena, world *World,
                     uint32 LowEntityIndex, low_entity *LowEntity,
                     world_position *OldP, world_position *NewP)
{
    ChangeEntityLocationRaw(Arena, World, LowEntityIndex, OldP, NewP);
    if (NewP)
    {
        LowEntity->P = *NewP;
    }
    else
    {
        LowEntity->P = NullPosition();
    }
}
