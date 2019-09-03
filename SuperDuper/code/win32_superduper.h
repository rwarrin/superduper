#ifndef WIN32_SUPERDUPER_H

#define VERSION L"0.0.1"

#define APP_MENU_FILE_SAVE          1000
#define APP_MENU_FILE_LOAD          1010
#define APP_MENU_FILE_EXIT          1020
#define APP_MENU_RUN_RUN            1100

#define WINDOWID_TREEVIEW           5000
#define WINDOWID_STATUSBAR          5010

#define MESSAGE_MAIN_WINDOW_RESIZE  9000
#define MESSAGE_MENU_SELECT_RUN     9010
#define MESSAGE_MENU_SELECT_SAVE    9020
#define MESSAGE_MENU_SELECT_LOAD    9030
#define MESSAGE_NOTIFY_THREAD_END   (WM_USER + 1000)

struct win32_dimensions
{
    u32 Width;
    u32 Height;
};

struct win32_enum_directory_bundle
{
    struct arena *StringsArena;
    struct arena *TableArena;
    struct file_table *FileTable;
    struct file_buffer *FileReadBuffer;
    wchar *SelectedFolderPath;
    HWND TreeView;
    HWND StatusBar;
    DWORD ParentThreadID;
};

enum StatusBarItems
{
    StatusBarItems_State = 0,
    StatusBarItems_CountTotal,
    StatusBarItems_CountDuplicate,

    StatusBarItems_Count,
};

#define WIN32_SUPERDUPER_H
#endif
