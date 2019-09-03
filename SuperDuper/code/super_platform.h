#ifndef SUPER_PLATFORM_H

#include <stdio.h>
#include <stdint.h>

typedef wchar_t wchar;
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

typedef float real32;
typedef double real64;

typedef uint32_t b32;

#define Assert(Expression) if(!(Expression)) { *(int *)0 = 0; }

#define internal static
#define global_variable static
#define local_persist static

#define Kilobytes(Size) ((Size)*1024LL)
#define Megabytes(Size) (Kilobytes(Size)*1024LL)

#define ArrayCount(Array) (sizeof((Array)) / sizeof((Array)[0]))

struct arena
{
    u8 *Base;
    u32 Used;
    u64 Size;
};

internal void
InitializeArena(struct arena *Arena, u64 Size)
{
    if(Arena != 0)
    {
        Arena->Base = (u8 *)VirtualAlloc(0, sizeof(char)*Size, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
        Arena->Size = Size;
        Arena->Used = 0;
    }
}

#define PushStruct(Arena, Type) (Type *)_PushSize(Arena, sizeof(Type))
#define PushArray(Arena, Count, Type) (Type *)_PushSize(Arena, sizeof(Type)*Count)

inline u8 *
_PushSize(struct arena *Arena, u32 Size)
{
    Assert(Arena->Used + Size <= Arena->Size);
    u8 *Result = Arena->Base + Arena->Used;

    // TODO(rick): Make the alignment requirement a parameter
    size_t Alignment = 16;
    size_t AlignmentMask = Alignment - 1;
    size_t Offset = 0;
    if(((size_t)Result) & AlignmentMask)
    {
        Offset = Alignment - (((size_t)Result) & AlignmentMask);
    }

    Result = Result + Offset;
    Arena->Used += (u32)(Size + Offset);

    return(Result);
}

struct temp_memory
{
    u8 *Base;
    u32 Used;
    u64 Size;
    struct arena *Arena;
};

inline struct temp_memory
BeginTemporaryMemory(struct arena *Arena)
{
    Assert(Arena != 0);

    struct temp_memory Result = {};
    Result.Base = Arena->Base;
    Result.Used = Arena->Used;
    Result.Size = Arena->Size;
    Result.Arena = Arena;

    return(Result);
}

inline void
EndTemporaryMemory(struct temp_memory *TempMemory)
{
    if(TempMemory != 0)
    {
        TempMemory->Arena->Base = TempMemory->Base;
        TempMemory->Arena->Used = TempMemory->Used;
        TempMemory->Arena->Size = TempMemory->Size;
    }
}

struct file_buffer
{
    size_t Size;
    u8 *Memory;
};

struct entire_file
{
    size_t Size;
    u8 *Contents;
};

#define SUPER_PLATFORM_H
#endif
