# SuperDuper

SuperDuper is a simple utility for finding duplicate files. The application will
scan all files within a selected folder and then display groups of duplicate
files. To determine whether or not two files are duplicates the files are hashed
using [MeowHash](https://github.com/cmuratori/meow_hash) and the file sizes are compared.

## Usage

1. Run the application and select Run then Run from the menu bar.
2. Select a directory to start the scan at using the folder select dialog.
3. Expand root level tree nodes to view duplicate files with their paths
   included.
4. Double click a file path node to open an explorer window at that location.

You can save the results and load them again to view later by using the Save and
Load opens in the File menu.

## Building

1. Add the MSVC compiler tools to your environment by running the `vcvarsall.bat`
   found in the Visual Studio directory.
2. Run `build.bat`. 
