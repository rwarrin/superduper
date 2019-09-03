#ifndef SUPER_HASHTABLE_H

#include <stdlib.h>

struct file_info
{
    meow_u128 Hash;
    u64 FileSize;

    u32 FileNameLength;
    wchar *FileName;

    u32 FilesCount;
    struct file_info *Next;
    struct file_info *Files;
};

struct file_table
{
    u32 Size;
    u32 Count;
    u32 DuplicateCount;
    struct file_info **Buckets;
};

internal void
InitializeFileTable(struct arena *TableArena, struct file_table *FileTable, u32 Size)
{
    FileTable->Size = Size;
    FileTable->Count = 0;
    FileTable->DuplicateCount = 0;
    FileTable->Buckets = PushArray(TableArena, Size, file_info *);
}

internal void
FileTableAddFileInfo(struct arena *TableArena, struct file_table *FileTable,
                     meow_u128 Hash, u64 FileSize, u32 FileNameLength, wchar *FileName)
{
    struct file_info *FileInfo = PushStruct(TableArena, file_info);
    FileInfo->Hash = Hash;
    FileInfo->FileSize = FileSize;
    FileInfo->FileNameLength = FileNameLength;
    FileInfo->FileName = FileName;
    FileInfo->FilesCount = 1;

    b32 Inserted = 0;
    u32 TableIndex = Hash.m128i_u64[1]%FileTable->Size;
    for(struct file_info *FileInfoPtr = FileTable->Buckets[TableIndex];
        FileInfoPtr != 0;
        FileInfoPtr = FileInfoPtr->Next)
    {
        meow_u128 OurHash = Hash;
        meow_u128 TheirHash = FileInfoPtr->Hash;
        u32 HashesAreEqual = MeowHashesAreEqual(OurHash, TheirHash);
        if(HashesAreEqual)
        {
            if(FileSize == FileInfoPtr->FileSize)
            {
                FileInfo->Files = FileInfoPtr->Files;
                FileInfoPtr->Files = FileInfo;
                ++FileInfoPtr->FilesCount;
                ++FileTable->DuplicateCount;
                Inserted = 1;
                break;
            }
        }
    }

    if(!Inserted)
    {
        FileInfo->Next = FileTable->Buckets[TableIndex];
        FileTable->Buckets[TableIndex] = FileInfo;
    }

    ++FileTable->Count;
}

#define SUPER_HASHTABLE_H
#endif
