#define WIN32_LEAN_AND_MEAN
#define _CRT_SECURE_NO_WARNINGS
#define UNICODE

#include <windows.h>
#include <commctrl.h>
#include <shlobj.h>
#include <commdlg.h>
#include <wchar.h>
#include <shellapi.h>

#include "meow_hash_x64_aesni.h"
#include "super_platform.h"
#include "win32_superduper.h"

#include "super_hashtable.cpp"

global_variable HWND GlobalTreeViewHandle;
global_variable HWND GlobalStatusBarHandle;

inline void
Win32ResizeFileReadBuffer(struct file_buffer *FileBuffer, u64 Size)
{
    if(FileBuffer->Memory)
    {
        VirtualFree(FileBuffer->Memory, 0, MEM_RELEASE);
    }

    FileBuffer->Size = Size;
    FileBuffer->Memory = (u8 *)VirtualAlloc(0, FileBuffer->Size, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);

#if SUPER_INTERNAL
    FileBuffer->MaxUsed = MAX(FileBuffer->MaxUsed, Size);
    ++FileBuffer->AllocationsMade;
#endif
}

internal wchar *
Win32CreateFullPath(arena *Arena, wchar *BasePath, wchar *DirectoryName)
{
    u32 StringLength = (u32)(wcslen(BasePath) + wcslen(DirectoryName) + 2);
    wchar *Result = PushArray(Arena, MAX_PATH, wchar);
    wchar *At = Result;

    while(*BasePath != 0)
    {
        *At++ = *BasePath++;
    }

    *At++ = '\\';
    while(*DirectoryName != 0)
    {
        *At++ = *DirectoryName++;
    }
    *At = 0;

    return(Result);
}

internal struct entire_file
Win32ReadEntireFile(struct arena *Arena, struct file_buffer *FileReadBuffer,
                    wchar *Directory, WIN32_FIND_DATAW *FileData)
{
    struct entire_file Result = {};

    struct temp_memory TempArena = BeginTemporaryMemory(Arena);
    wchar *FullFilePath = Win32CreateFullPath(Arena, Directory, FileData->cFileName);
    FILE *File = _wfopen(FullFilePath, L"rb");
    if(File)
    {
        _fseeki64(File, 0, SEEK_END);
        Result.Size = _ftelli64(File);
        _fseeki64(File, 0, SEEK_SET);

        if(Result.Size > FileReadBuffer->Size)
        {
            Win32ResizeFileReadBuffer(FileReadBuffer, Result.Size + Kilobytes(8));
        }

        Result.Contents = FileReadBuffer->Memory;
        fread(Result.Contents, Result.Size, 1, File);
        fclose(File);

    }
    EndTemporaryMemory(&TempArena);

    return(Result);
}

internal void
Win32EnumerateDirectoryContents(arena *StringsArena, arena *TableArena, struct file_table *FileTable,
                                struct file_buffer *FileReadBuffer, wchar *Directory)
{
    WIN32_FIND_DATAW FileData = {};
    struct temp_memory TempMemory = BeginTemporaryMemory(StringsArena);

    wchar *DirectoryPtr = Directory;
    wchar *DirectorySearchString = PushArray(StringsArena, MAX_PATH, wchar);
    wchar *DirectorySearchStringPtr = DirectorySearchString;
    while(*DirectoryPtr != 0)
    {
        *DirectorySearchStringPtr++ = *DirectoryPtr++;
    }
    *DirectorySearchStringPtr++ = '\\';
    *DirectorySearchStringPtr++ = '*';
    *DirectorySearchStringPtr++ = 0;

    HANDLE FindHandle = FindFirstFileW(DirectorySearchString, &FileData);
    if(FindHandle)
    {
        b32 NextFileIsValid = true;
        while(NextFileIsValid != 0)
        {
            if((FileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == FILE_ATTRIBUTE_DIRECTORY)
            {
                if((FileData.cFileName[0] != L'.') &&
                   ((FileData.cFileName[1] != L'.') || (FileData.cFileName[1] != 0)))
                {
                    wchar *ThisDirectoryPath = Win32CreateFullPath(StringsArena, Directory, FileData.cFileName);
                    Win32EnumerateDirectoryContents(StringsArena, TableArena, FileTable, FileReadBuffer, ThisDirectoryPath);
                }
            }
            else
            {
                struct entire_file EntireFile = Win32ReadEntireFile(StringsArena, FileReadBuffer, Directory, &FileData);
                meow_u128 Hash = MeowHash(MeowDefaultSeed, EntireFile.Size, EntireFile.Contents);

                u32 FileNameSize = (u32)(wcslen(FileData.cFileName) + wcslen(Directory) + 2);
                wchar *FileName = Win32CreateFullPath(TableArena, Directory, FileData.cFileName);
                FileTableAddFileInfo(TableArena, FileTable, Hash, EntireFile.Size, FileNameSize, FileName);
            }

            NextFileIsValid = FindNextFileW(FindHandle, &FileData);
        }
    }

    EndTemporaryMemory(&TempMemory);
}

internal struct win32_dimensions
Win32GetWindowDimensions(HWND Window)
{
    struct win32_dimensions Result = {};

    RECT ClientRect = {};
    GetClientRect(Window, &ClientRect);

    Result.Width = ClientRect.right - ClientRect.left;
    Result.Height = ClientRect.bottom - ClientRect.top;

    return(Result);
}

internal void
Win32CreateMenu(HWND Window)
{
    HMENU MainMenu = CreateMenu();

    HMENU FileMenu = CreateMenu();
    AppendMenuW(FileMenu, MF_STRING, APP_MENU_FILE_SAVE, L"&Save");
    AppendMenuW(FileMenu, MF_STRING, APP_MENU_FILE_LOAD, L"&Load");
    AppendMenuW(FileMenu, MF_SEPARATOR, 0, 0);
    AppendMenuW(FileMenu, MF_STRING, APP_MENU_FILE_EXIT, L"&Exit");

    HMENU RunMenu = CreateMenu();
    AppendMenuW(RunMenu, MF_STRING, APP_MENU_RUN_RUN, L"Ru&n");

    AppendMenuW(MainMenu, MF_POPUP, (UINT_PTR)FileMenu, L"&File");
    AppendMenuW(MainMenu, MF_POPUP, (UINT_PTR)RunMenu, L"&Run");
    SetMenu(Window, MainMenu);
}

inline HTREEITEM
Win32InsertTreeViewItem(HWND TreeView, wchar *String, u32 StringLength, HTREEITEM ParentTreeItem)
{
    TVITEMW TVItem = {};
    TVItem.mask = TVIF_TEXT;
    TVItem.state = TVIS_EXPANDED;
    TVItem.pszText = String;
    TVItem.cchTextMax = StringLength;

    TVINSERTSTRUCTW TVInsertStruct = {};
    TVInsertStruct.hParent = ParentTreeItem;
    TVInsertStruct.hInsertAfter = TVI_LAST;
    TVInsertStruct.item = TVItem;

    HTREEITEM Result = (HTREEITEM)SendMessage(TreeView, TVM_INSERTITEM, 0,
                                              (LPARAM)&TVInsertStruct);

    return(Result);
}

internal void
Win32FileTableToTreeView(HWND TreeView, struct file_table *FileTable)
{
    for(u32 Index = 0; Index < FileTable->Size; ++Index)
    {
        for(struct file_info *FileInfo = FileTable->Buckets[Index];
            FileInfo != 0;
            FileInfo = FileInfo->Next)
        {
            u32 TempStringLength = 0;
            wchar TempStringBuffer[1024] = {};

            if(FileInfo->FilesCount > 1)
            {
                TempStringLength = _snwprintf(TempStringBuffer, ArrayCount(TempStringBuffer),
                                              L"%llu%llu - (%u files) - (%llu bytes)\n",
                                              FileInfo->Hash.m128i_u64[0],
                                              FileInfo->Hash.m128i_u64[1],
                                              FileInfo->FilesCount,
                                              FileInfo->FileSize);
                HTREEITEM RootNode = Win32InsertTreeViewItem(TreeView, TempStringBuffer,
                                                             TempStringLength, 0);

                TempStringLength = _snwprintf(TempStringBuffer, ArrayCount(TempStringBuffer),
                                              L"%ws\n", FileInfo->FileName);
                Win32InsertTreeViewItem(TreeView, TempStringBuffer, TempStringLength, RootNode);

                for(struct file_info *DuplicateFiles = FileInfo->Files;
                    DuplicateFiles != 0;
                    DuplicateFiles = DuplicateFiles->Files)
                {
                    TempStringLength = _snwprintf(TempStringBuffer, ArrayCount(TempStringBuffer),
                                                  L"%ws\n", DuplicateFiles->FileName);
                    Win32InsertTreeViewItem(TreeView, TempStringBuffer, TempStringLength, RootNode);
                }
            }
        }
    }
}

internal void
Win32SetStatusBarText(HWND StatusBar, wchar *Text, u32 Index)
{
    SendMessage(StatusBar, SB_SETTEXT, (WPARAM)Index, (LPARAM)Text);
}

internal void
Win32ExportFileTableToFile(struct file_table *FileTable)
{
    wchar FilePath[MAX_PATH] = {};
    OPENFILENAMEW FileData = {};
    FileData.lStructSize = sizeof(FileData);
    FileData.lpstrFilter = L".dat";
    FileData.lpstrFile = FilePath;
    FileData.nMaxFile = ArrayCount(FilePath);
    if(GetSaveFileName(&FileData))
    {
        FILE *File = _wfopen(FilePath, L"w");
        if(File)
        {
            for(u32 Index = 0; Index < FileTable->Size; ++Index)
            {
                for(struct file_info *FileInfo = FileTable->Buckets[Index];
                    FileInfo != 0;
                    FileInfo = FileInfo->Next)
                {
                    if(FileInfo->FilesCount > 1)
                    {
                        fwprintf(File, L"%llu%llu - (%u files) - (%llu bytes)\n",
                                 FileInfo->Hash.m128i_u64[0],
                                 FileInfo->Hash.m128i_u64[1],
                                 FileInfo->FilesCount,
                                 FileInfo->FileSize);
                        fwprintf(File, L"\t%ws\n", FileInfo->FileName);

                        for(struct file_info *DuplicateFiles = FileInfo->Files;
                            DuplicateFiles != 0;
                            DuplicateFiles = DuplicateFiles->Files)
                        {
                            fwprintf(File, L"\t%ws\n", DuplicateFiles->FileName);
                        }
                    }
                }
            }

            fclose(File);
        }
    }
}

internal void
Win32ImportFileTableFromFile(HWND TreeView)
{
    wchar FilePath[MAX_PATH] = {};
    OPENFILENAMEW FileData = {};
    FileData.lStructSize = sizeof(FileData);
    FileData.lpstrFilter = L".dat";
    FileData.lpstrFile = FilePath;
    FileData.nMaxFile = ArrayCount(FilePath);
    if(GetOpenFileName(&FileData))
    {
        FILE *File = _wfopen(FilePath, L"r");
        if(File)
        {
            u32 DuplicateCount = 0;
            wchar ReadBuffer[1024] = {};

            HTREEITEM RootNode = 0;
            while((fgetws(ReadBuffer, ArrayCount(ReadBuffer), File)) != 0)
            {
                if(ReadBuffer[0] == '\t')
                {
                    wchar *ReadBufferPlusOne = ReadBuffer + 1;
                    Win32InsertTreeViewItem(TreeView, ReadBuffer + 1, ArrayCount(ReadBuffer) - 1, RootNode);
                }
                else
                {
                    RootNode = Win32InsertTreeViewItem(TreeView, ReadBuffer, ArrayCount(ReadBuffer), 0);
                    ++DuplicateCount;
                }
            }

            fclose(File);


            wchar StatusBarBuffer[128] = {};
            _snwprintf(StatusBarBuffer, ArrayCount(StatusBarBuffer),
                    L"%u duplicates found", DuplicateCount);
            Win32SetStatusBarText(GlobalStatusBarHandle, StatusBarBuffer, StatusBarItems_CountDuplicate);
            Win32SetStatusBarText(GlobalStatusBarHandle, L"", StatusBarItems_CountTotal);
        }
    }
}

internal DWORD
Win32EnumerateDirectoryContentsThreaded(void *Data)
{
    struct win32_enum_directory_bundle *Bundle = (struct win32_enum_directory_bundle *)Data;

    Win32SetStatusBarText(Bundle->StatusBar, L"Working...", StatusBarItems_State);

    SendMessage(Bundle->TreeView, WM_SETREDRAW, FALSE, 0);
    TreeView_DeleteAllItems(Bundle->TreeView);
    SendMessage(Bundle->TreeView, WM_SETREDRAW, TRUE, 0);
    RedrawWindow(Bundle->TreeView, NULL, NULL, RDW_ERASE | RDW_FRAME | RDW_INVALIDATE | RDW_ALLCHILDREN);
    SendMessage(Bundle->TreeView, WM_SETREDRAW, FALSE, 0);

    Win32EnumerateDirectoryContents(Bundle->StringsArena, Bundle->TableArena,
                                    Bundle->FileTable, Bundle->FileReadBuffer,
                                    Bundle->SelectedFolderPath);

    Win32FileTableToTreeView(Bundle->TreeView, Bundle->FileTable);

    VirtualFree(Bundle->FileReadBuffer->Memory, 0, MEM_RELEASE);
    SendMessage(Bundle->TreeView, WM_SETREDRAW, TRUE, 0);
    RedrawWindow(Bundle->TreeView, NULL, NULL, RDW_ERASE | RDW_FRAME | RDW_INVALIDATE | RDW_ALLCHILDREN);

    wchar StatusBarBuffer[128] = {};
    _snwprintf(StatusBarBuffer, ArrayCount(StatusBarBuffer),
               L"%u files scanned", Bundle->FileTable->Count);
    Win32SetStatusBarText(Bundle->StatusBar, StatusBarBuffer, StatusBarItems_CountTotal);

    _snwprintf(StatusBarBuffer, ArrayCount(StatusBarBuffer),
               L"%u duplicates found", Bundle->FileTable->DuplicateCount);
    Win32SetStatusBarText(Bundle->StatusBar, StatusBarBuffer, StatusBarItems_CountDuplicate);

    Win32SetStatusBarText(Bundle->StatusBar, L"Idle", StatusBarItems_State);

    PostThreadMessageW(Bundle->ParentThreadID, MESSAGE_NOTIFY_THREAD_END, 0, 0);
    return(0);
}


internal void
InitializeMemoryBuffers(struct file_buffer *FileReadBuffer, u64 FileBufferSize,
                        struct file_table *FileTable, u32 FileTableSize,
                        struct arena *StringsArena, u64 StringsArenaSize,
                        struct arena *TableArena, u64 TableArenaSize)
{
    if(FileReadBuffer->Memory)
    {
        VirtualFree(FileReadBuffer->Memory, 0, MEM_RELEASE);
        FileReadBuffer->Size = 0;
    }
    Win32ResizeFileReadBuffer(FileReadBuffer, FileBufferSize);

    if(StringsArena->Base)
    {
        VirtualFree(StringsArena->Base, 0, MEM_RELEASE);
        StringsArena->Size = 0;
        StringsArena->Used = 0;
    }
    InitializeArena(StringsArena, StringsArenaSize);

    if(TableArena->Base)
    {
        VirtualFree(StringsArena, 0, MEM_RELEASE);
        TableArena->Size = 0;
        TableArena->Used = 0;
    }
    InitializeArena(TableArena, TableArenaSize);

    InitializeFileTable(TableArena, FileTable, FileTableSize);
}

internal b32
Win32GetDirectoryFromUser(HWND Window, wchar *PathOut)
{
    b32 Result = 0;
    BROWSEINFOW BrowseInfo = {};
    BrowseInfo.hwndOwner = Window;
    BrowseInfo.lpszTitle = L"Choose a folder";
    BrowseInfo.ulFlags = BIF_NEWDIALOGSTYLE;

    PIDLIST_ABSOLUTE SelectedFolderPID = SHBrowseForFolderW(&BrowseInfo);
    if(SelectedFolderPID)
    {
        SHGetPathFromIDListW(SelectedFolderPID, PathOut);
        Result = 1;
    }

    return(Result);
}

LRESULT CALLBACK
Win32WindowsCallback(HWND Window, UINT Message, WPARAM WParam, LPARAM LParam)
{
    LRESULT Result = 0;

    switch(Message)
    {
        case WM_SIZE:
        {
            struct win32_dimensions WindowDims = Win32GetWindowDimensions(Window);
            MoveWindow(GlobalTreeViewHandle, 0, 0,
                       WindowDims.Width, WindowDims.Height - 20, true);
            MoveWindow(GlobalStatusBarHandle, 0, 0,
                       WindowDims.Width, WindowDims.Height, true);

            u32 StatusBarWidths[StatusBarItems_Count] = {100, (WindowDims.Width + 100)/2, WindowDims.Width};
            SendMessage(GlobalStatusBarHandle, SB_SETPARTS, (WPARAM)StatusBarItems_Count, (LPARAM)StatusBarWidths);
        } break;
        case WM_DESTROY:
        {
            PostQuitMessage(0);
        } break;
        case WM_NOTIFY:
        {
            NMHDR *NotificationHeader = (NMHDR *)LParam;
            switch(NotificationHeader->code)
            {
                case NM_DBLCLK:
                {
                    HWND TreeView = NotificationHeader->hwndFrom;
                    HTREEITEM SelectedItem = TreeView_GetSelection(TreeView);
                    if(SelectedItem)
                    {
                        wchar PathBuffer[MAX_PATH] = {};

                        TVITEMW TreeViewItem = {};
                        TreeViewItem.mask = TVIF_HANDLE | TVIF_TEXT;
                        TreeViewItem.hItem = SelectedItem;
                        TreeViewItem.pszText = PathBuffer;
                        TreeViewItem.cchTextMax = ArrayCount(PathBuffer);

                        TreeView_GetItem(TreeView, &TreeViewItem);

                        wchar *LastSlash = PathBuffer + (ArrayCount(PathBuffer) - 1);
                        while(LastSlash != PathBuffer)
                        {
                            if(*LastSlash == '\\')
                            {
                                *LastSlash = 0;
                                break;
                            }
                            --LastSlash;
                        }

                        HINSTANCE Status = ShellExecuteW(Window, L"explore", PathBuffer, 0, 0, SW_SHOWNORMAL);
#if 0
                        if(Status == 0)
                        {
                            Assert(!"The operating system is out of memory or resources.");
                        }
                        else if(Status == (HINSTANCE)ERROR_FILE_NOT_FOUND)
                        {
                            Assert(!"The specified file was not found.");
                        }
                        else if(Status == (HINSTANCE)ERROR_PATH_NOT_FOUND)
                        {
                            Assert(!"The specified path was not found.");
                        }
                        else if(Status == (HINSTANCE)ERROR_BAD_FORMAT)
                        {
                            Assert(!"The .exe file is invalid (non-Win32 .exe or error in .exe image).");
                        }
                        else if(Status == (HINSTANCE)SE_ERR_ACCESSDENIED)
                        {
                            Assert(!"The operating system denied access to the specified file.");
                        }
                        else if(Status == (HINSTANCE)SE_ERR_ASSOCINCOMPLETE)
                        {
                            Assert(!"The file name association is incomplete or invalid.");
                        }
                        else if(Status == (HINSTANCE)SE_ERR_DDEBUSY)
                        {
                            Assert(!"The DDE transaction could not be completed because other DDE transactions were being processed.");
                        }
                        else if(Status == (HINSTANCE)SE_ERR_DDEFAIL)
                        {
                            Assert(!"The DDE transaction failed.");
                        }
                        else if(Status == (HINSTANCE)SE_ERR_DDETIMEOUT)
                        {
                            Assert(!"The DDE transaction could not be completed because the request timed out.");
                        }
                        else if(Status == (HINSTANCE)SE_ERR_DLLNOTFOUND)
                        {
                            Assert(!"The specified DLL was not found.");
                        }
                        else if(Status == (HINSTANCE)SE_ERR_FNF)
                        {
                            Assert(!"The specified file was not found.");
                        }
                        else if(Status == (HINSTANCE)SE_ERR_NOASSOC)
                        {
                            Assert(!"There is no application associated with the given file name extension. This error will also be returned if you attempt to print a file that is not printable.");
                        }
                        else if(Status == (HINSTANCE)SE_ERR_OOM)
                        {
                            Assert(!"There was not enough memory to complete the operation.");
                        }
                        else if(Status == (HINSTANCE)SE_ERR_PNF)
                        {
                            Assert(!"The specified path was not found.");
                        }
                        else if(Status == (HINSTANCE)SE_ERR_SHARE)
                        {
                            Assert(!"A sharing violation occurred.");
                        }
#endif
                    }
                } break;
                default:
                {
                    Result = DefWindowProc(Window, Message, WParam, LParam);
                } break;
            }
        } break;
        case WM_COMMAND:
        {
            u32 WParamHigh = HIWORD(WParam);
            u32 WParamLow = LOWORD(WParam);

            if(WParamLow == APP_MENU_FILE_EXIT)
            {
                PostQuitMessage(0);
            }
            else if(WParamLow == APP_MENU_RUN_RUN)
            {
                PostMessageW(0, MESSAGE_MENU_SELECT_RUN, 0, 0);
            }
            else if(WParamLow == APP_MENU_FILE_SAVE)
            {
                PostMessageW(0, MESSAGE_MENU_SELECT_SAVE, 0, 0);
            }
            else if(WParamLow == APP_MENU_FILE_LOAD)
            {
                PostMessage(0, MESSAGE_MENU_SELECT_LOAD, 0, 0);
            }
        } break;
        default:
        {
            Result = DefWindowProc(Window, Message, WParam, LParam);
        } break;
    }

    return(Result);
}

int WINAPI
WinMain(HINSTANCE Instance, HINSTANCE PrevInstance, LPSTR CmdLine, int CmdShow)
{
    INITCOMMONCONTROLSEX CommonControls = {};
    CommonControls.dwSize = sizeof(CommonControls);
    CommonControls.dwICC = ICC_TREEVIEW_CLASSES;
    InitCommonControlsEx(&CommonControls);

    WNDCLASSEXW WindowClass = {};
    WindowClass.cbSize = sizeof(WindowClass);
    WindowClass.style = CS_HREDRAW | CS_VREDRAW;
    WindowClass.lpfnWndProc = Win32WindowsCallback;
    WindowClass.hInstance = Instance;
    WindowClass.hIcon = LoadIcon(GetModuleHandle(0), MAKEINTRESOURCE(1));
    WindowClass.hCursor = LoadCursor(NULL, IDC_ARROW);
    WindowClass.hbrBackground = (HBRUSH)COLOR_WINDOW;
    WindowClass.lpszClassName = L"superduperwindowclass";

    if(!RegisterClassExW(&WindowClass))
    {
        MessageBoxW(0, L"Failed to register window class.", L"Error", MB_OK);
        return(1);
    }

    HWND Window = CreateWindowExW(0, WindowClass.lpszClassName, L"SuperDuper v" VERSION,
                                  WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                                  CW_USEDEFAULT, CW_USEDEFAULT,
                                  960, 480,
                                  0, 0, Instance, 0);
    if(!Window)
    {
        MessageBoxW(0, L"Failed to create window.", L"Error", MB_OK);
        return(2);
    }

    struct win32_dimensions WindowDims = Win32GetWindowDimensions(Window);
    HWND TreeView = CreateWindowExW(0, WC_TREEVIEW, 0,
                                    WS_CHILD | WS_VISIBLE | TVS_HASBUTTONS | TVS_HASLINES | TVS_LINESATROOT | TVS_FULLROWSELECT,
                                    0, 0,
                                    WindowDims.Width, WindowDims.Height,
                                    Window, (HMENU)WINDOWID_TREEVIEW, Instance, 0);
    GlobalTreeViewHandle = TreeView;

    HWND StatusBar = CreateWindowExW(0, STATUSCLASSNAME, 0,
                                     WS_CHILD | WS_VISIBLE,
                                     0, 0, 0, 0,
                                     Window, (HMENU)WINDOWID_STATUSBAR, Instance, 0);
    GlobalStatusBarHandle = StatusBar;

    HFONT Font = CreateFontA(14, 0, 0, 0, FW_REGULAR, 0, 0, 0, DEFAULT_CHARSET,
                             OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                             FIXED_PITCH, "Consolas");
    PostMessageW(TreeView, WM_SETFONT, (WPARAM)Font, TRUE);

    Win32CreateMenu(Window);

    struct file_buffer FileReadBuffer = {};
    struct arena StringsArena = {};
    struct arena TableArena = {};
    struct file_table FileTable = {};

    Win32SetStatusBarText(StatusBar, L"Idle", StatusBarItems_State);

    wchar SelectedFolderPath[MAX_PATH] = {};
    b32 ThreadRunning = 0;
    struct win32_enum_directory_bundle Bundle = {};
    Bundle.StringsArena = &StringsArena;
    Bundle.TableArena = &TableArena;
    Bundle.FileTable = &FileTable;
    Bundle.FileReadBuffer = &FileReadBuffer;
    Bundle.SelectedFolderPath = SelectedFolderPath;
    Bundle.TreeView = TreeView;
    Bundle.StatusBar = StatusBar;
    Bundle.ParentThreadID = GetCurrentThreadId();

    SendMessage(Window, WM_SIZE, 0, 0);
    b32 MessageResult = 0;
    MSG Message;
    for(;;)
    {
        MessageResult = GetMessage(&Message, 0, 0, 0);
        if(MessageResult <= 0)
        {
            break;
        }
        else
        {
            b32 WasDialogMessage = IsDialogMessage(Window, &Message);
            if(!WasDialogMessage)
            {
                switch(Message.message)
                {
                    case MESSAGE_MAIN_WINDOW_RESIZE:
                    {
                        struct win32_dimensions WindowDims = Win32GetWindowDimensions(Window);
                        MoveWindow(TreeView, 0, 0,
                                    WindowDims.Width, WindowDims.Height, true);
                    } break;
                    case MESSAGE_NOTIFY_THREAD_END:
                    {
                        ThreadRunning = 0;
                    } break;
                    case MESSAGE_MENU_SELECT_RUN:
                    {
                        if(!ThreadRunning)
                        {
                            b32 ValidDirectory = Win32GetDirectoryFromUser(Window, SelectedFolderPath);
                            if(ValidDirectory)
                            {
                                InitializeMemoryBuffers(&FileReadBuffer, Megabytes(32),
                                                        &FileTable, 4093,
                                                        &StringsArena, Kilobytes(512),
                                                        &TableArena, Megabytes(32));

                                Win32SetStatusBarText(StatusBar, L"", StatusBarItems_CountTotal);
                                Win32SetStatusBarText(StatusBar, L"", StatusBarItems_CountDuplicate);
                                ThreadRunning = 1;

                                HANDLE ThreadHandle = CreateThread(0, 0, Win32EnumerateDirectoryContentsThreaded,
                                                                   (LPVOID)&Bundle, 0, 0);
                            }
                        }
                        else
                        {
                            MessageBoxW(0, L"Application is busy processing another directory.", L"Warning", MB_OK);
                        }
                    } break;
                    case MESSAGE_MENU_SELECT_SAVE: 
                    {
                        Win32ExportFileTableToFile(&FileTable);
                    } break;
                    case MESSAGE_MENU_SELECT_LOAD:
                    {
                        SendMessage(TreeView, WM_SETREDRAW, FALSE, 0);
                        TreeView_DeleteAllItems(TreeView);

                        Win32ImportFileTableFromFile(TreeView);

                        SendMessage(TreeView, WM_SETREDRAW, TRUE, 0);
                        RedrawWindow(TreeView, NULL, NULL, RDW_ERASE | RDW_FRAME | RDW_INVALIDATE | RDW_ALLCHILDREN);
                    } break;
                    default:
                    {
                        TranslateMessage(&Message);
                        DispatchMessage(&Message);
                    } break;
                }
            }
        }
    }

#if SUPER_INTERNAL
    FILE *Log = fopen("log.txt", "w");
    if(Log)
    {
        fprintf(Log, "StringsArena\n\tMax Used: %llu\n\tAllocations: %llu\n",
                StringsArena.MaxUsed, StringsArena.AllocationsMade);
        fprintf(Log, "TableArena\n\tMax Used: %llu\n\tAllocations: %llu\n",
                TableArena.MaxUsed, TableArena.AllocationsMade);

        u32 MaxChainLength = 0;
        u32 ChainLength = 0;
        u32 UnusedBuckets = 0;
        for(u32 Index = 0; Index < FileTable.Size; ++Index)
        {
            ChainLength = 0;
            for(struct file_info *FileInfo = FileTable.Buckets[Index];
                FileInfo != 0;
                FileInfo = FileInfo->Next)
            {
                ++ChainLength;
            }

            if(ChainLength == 0)
            {
                ++UnusedBuckets;
            }

            MaxChainLength = MAX(MaxChainLength, ChainLength);
        }
        fprintf(Log, "File Table\n\tCount: %u\n\tMax Chain Length: %u\n"
                     "\tUnused Buckets: %u\n",
                FileTable.Count, MaxChainLength, UnusedBuckets);

        fprintf(Log, "File Read Buffer\n\tMax Size: %llu\n\tAllocations: %llu\n",
                FileReadBuffer.MaxUsed, FileReadBuffer.AllocationsMade);

        fclose(Log);
    }
#endif

    return(0);
}
