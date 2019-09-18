#include "super.h"

struct filesystem_array
{
    u32 ArrayLength;
    wchar **StringsArray;
    arena *StringsArena;

    u64 At;
};


internal struct win32_file_list *
BuildFileList(struct arena *StringsArena, u32 FileCount)
{
    struct win32_file_list *FileList = (struct win32_file_list *)Win32AllocateMemory(sizeof(struct win32_file_list) + (sizeof(wchar *)*FileCount));
    FileList->FileNames = (wchar **)(FileList + 1);
    FileList->FileNameCount = FileCount;
    FileList->StringsArena = StringsArena;
    wchar *StringPtr = (wchar *)StringsArena->Base;
    for(u32 Index = 0; Index < FileCount; ++Index)
    {
        if(Index == FileList->FileNameCount - 2)
        {
            int a = 5;
        }
        while(*StringPtr == 0)
        {
            if((u8 *)StringPtr >= (StringsArena->Base + StringsArena->Size))
            {
                StringsArena = StringsArena->Next;
                StringPtr = (wchar *)StringsArena->Base;
            }
            else
            {
                ++StringPtr;
            }
        }

        *(FileList->FileNames + Index) = StringPtr;

        while(*StringPtr != 0)
        {
            ++StringPtr;
        }
    }

    return(FileList);
}
