#ifndef SUPER_PLATFORM_H

#include <stdio.h>
#include <stdint.h>
#include <intrin.h>

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

#define MAX(A, B) ((A) > (B) ? (A) : (B))

inline u8 *
Win32AllocateMemory(u64 Size)
{
    u8 *Result = (u8 *)VirtualAlloc(0, Size, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
    Assert(Result != 0);

    return(Result);
}

inline u8 *
Win32ReallocateMemory(u8 *Source, u64 Size)
{
    u8 *Result = (u8 *)VirtualAlloc(0, Size, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
    if(!Result)
    {
        Result = Source;
    }

    return(Result);
}

inline b32
Win32ResizeMemory(u8 *Source, u64 NewSize, u64 OldSize)
{
    b32 Result = 0;

    u8 *NewMemory = Win32ReallocateMemory(Source, NewSize);
    if(NewMemory != Source)
    {
        CopyMemory(NewMemory, Source, OldSize);
        Result = 1;
    }

    return(Result);
}


struct arena
{
    u8 *Base;
    u32 Used;
    u64 Size;

    struct arena *Next;

#if SUPER_INTERNAL
    // TODO(rick): Remove these eventually
    u64 MaxUsed;
    u64 MaxSize;
    u64 AllocationsMade;
#endif
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

internal struct arena *
CreateArena(u64 Size)
{
    struct arena *Result = 0;

    u64 ArenaSize = sizeof(struct arena) + Size;
    Result = (struct arena *)VirtualAlloc(0, ArenaSize, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
    Result->Used = 0;
    Result->Size = Size;
    Result->Base = (u8 *)(Result + 1);

    return(Result);
}

internal void
DestroyArena(struct arena *Arena)
{
    if(Arena)
    {
        Arena->Used = 0;
        Arena->Size = 0;
        u8 *Base = (u8 *)(((struct arena *)Arena->Base) - 1);
        VirtualFree(Arena->Base, 0, MEM_RELEASE);
    }
}

#define PushStruct(Arena, Type) (Type *)_PushSize(Arena, sizeof(Type))
#define PushArray(Arena, Count, Type) (Type *)_PushSize(Arena, sizeof(Type)*Count)

inline u8 *
_PushSize(struct arena *Arena, u32 Size)
{
    struct arena *UsingArena = Arena;
    struct arena *PreviousArena = 0;
    for(; UsingArena != 0; UsingArena = UsingArena->Next)
    {
        if(UsingArena->Used + Size < UsingArena->Size)
        {
            break;
        }
        PreviousArena = UsingArena;
    }

    if(UsingArena == 0)
    {
        u64 BlockSize = 0;
        if(PreviousArena != 0)
        {
            BlockSize = PreviousArena->Size*2;
        }
        if(Size > BlockSize)
        {
            BlockSize = Size*2;
        }

        UsingArena = CreateArena(BlockSize);

        if(PreviousArena != 0)
        {
            PreviousArena->Next = UsingArena;
        }
    }

    Assert(UsingArena->Used + Size <= UsingArena->Size);
    u8 *Result = UsingArena->Base + UsingArena->Used;

    // TODO(rick): Make the alignment requirement a parameter
    size_t Alignment = 16;
    size_t AlignmentMask = Alignment - 1;
    size_t Offset = 0;
    if(((size_t)Result) & AlignmentMask)
    {
        Offset = Alignment - (((size_t)Result) & AlignmentMask);
    }

    Result = Result + Offset;
    UsingArena->Used += (u32)(Size + Offset);

#if SUPER_INTERNAL
    UsingArena->MaxUsed = MAX(Arena->MaxUsed, UsingArena->Used);
    UsingArena->MaxSize = MAX(Arena->MaxSize, UsingArena->Size);
    ++Arena->AllocationsMade;
#endif

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
    // TODO(rick): Implement these again...
#if 0
    Result.Base = Arena->Base;
    Result.Used = Arena->Used;
    Result.Size = Arena->Size;
    Result.Arena = Arena;
#endif

    return(Result);
}

inline void
EndTemporaryMemory(struct temp_memory *TempMemory)
{
#if 0
    if(TempMemory != 0)
    {
        TempMemory->Arena->Base = TempMemory->Base;
        TempMemory->Arena->Used = TempMemory->Used;
        TempMemory->Arena->Size = TempMemory->Size;
    }
#endif
}

struct file_buffer
{
    size_t Size;
    u8 *Memory;

#if SUPER_INTERNAL
    // TODO(rick): Remove these eventually
    u64 MaxUsed;
    u64 AllocationsMade;
#endif
};

struct entire_file
{
    size_t Size;
    u8 *Contents;
};

enum
{
    MEOWHASH_STREAMING_THRESHOLD = Megabytes(8),
    MAX_ALLOWED_THREADS = 12,
};

struct ticket_mutex
{
    u64 volatile Ticket;
    u64 volatile Serving;
};

inline u64
AtomicAddU64(u64 volatile *Value, u64 Addend)
{
    u64 Result = _InterlockedExchangeAdd64((__int64 volatile *)Value, Addend);
    return(Result);
}

inline void
BeginTicketMutex(struct ticket_mutex *Mutex)
{
    u64 Ticket = AtomicAddU64(&Mutex->Ticket, 1);
    while(Ticket != Mutex->Serving)
    {
        _mm_pause();
    }
}

inline void
EndTicketMutex(struct ticket_mutex *Mutex)
{
    AtomicAddU64(&Mutex->Serving, 1);
}

#define SUPER_PLATFORM_H
#endif
