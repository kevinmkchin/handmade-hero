#ifndef HANDMADE_SIM_REGION_H
#define HANDMADE_SIM_REGION_H

struct move_spec
{
    bool32 UnitMaxAccelVector;
    real32 Speed;
    real32 Drag;
};

enum entity_type
{
    EntityType_Null,
    
    EntityType_Hero,
    EntityType_Wall,
    EntityType_Familiar,
    EntityType_Monstar,
    EntityType_Sword,
};

#define HIT_POINT_SUB_COUNT 4
struct hit_point
{
    // TODO(Kevin): Bake this down into one variable
    uint8 Flags;
    uint8 FilledAmount;
};

// TODO(Kevin): Rename sim_entity to entity!
struct sim_entity;
union entity_reference
{
    sim_entity *Ptr;
    uint32 Index;
};

struct sim_entity
{
    uint32 StorageIndex;

    entity_type Type;

    v2 P;
    uint32 ChunkZ;
    
    real32 Z;
    real32 dZ;
    
    v2 dP;
    real32 Width, Height;

    uint32 FacingDirection;
    real32 tBob;

    bool32 Collides;
    int32 dAbsTileZ;

    // TODO(Kevin): Should hitpoints themselves be entities?
    uint32 HitPointMax;
    hit_point HitPoint[16];

    entity_reference Sword;
    real32 DistanceRemaining;

    // TODO(Kevin): Generation index so we know how "up to date" this entity is.
};

struct sim_entity_hash
{
    sim_entity *Ptr;
    uint32 Index;
};

struct sim_region
{
    // TODO(Kevin): Need a hash table here to map stored entity indices
    // to sim entities!
    
    world *World;
    
    world_position Origin;
    rectangle2 Bounds;
    
    uint32 MaxEntityCount;
    uint32 EntityCount;
    sim_entity *Entities;

    // TODO(Kevin): Do I really want a hash for this??
    // NOTE(Kevin): Must be a power of two!
    sim_entity_hash Hash[4096];
};

#endif // HANDMADE_SIM_REGION_H
