# Notepad for Developer - N4D
N4D is a fork of BowPad by Stefan Kueng and modified with my favorites.

# I will not publish any binary build from now. You should clone this repository and build it by yourself.
* Build tool: Visual Studio 2022 Community.
* Windows SDK version : 10.0 (latest installed version)
* Platform toolset : Visual Studio 2022 (v143)
* C++ Lanuage Standard : ISO C++20 Standard.
* Tested OS: Windows 11 insider build 22000 above.
# Changed from BowPad
* Remove Ribbon UI, instead by operate using Keyboard and Popupmenu 
* Custom Draw title bar and tab bar, quickbar
* UI change for some dialogs, such as Find/Replace, Goto Line, Change Tab width, style configurator, about box and etc.
* UI change for encoding selection, lexer selection, command palette.
* Launch command for sciter.js app, inspector (Removed)
* Add Open Folder command to show folder content in file tree
* Remove editor config, plugin, spell checker, language support
* Update scintilla to 5.33
* Show tooltip for tabs when mouse on tab
* Input goto line when left click on line info of status bar
* Change tab width by right click on tab width of status bar
* Change encoding/style by left click on encoding/style info of status bar.
* Show options menu when right click on margin or right click with VK_CONTROL down on editor.
* Add toolbar in titlebar as quickbar, and show tooltip for button when mouse on button of quickbar
* Right button of tab bar is used to show all opened files using filterable dialog.
* Add recents menu (Not implemented for recents persitent and load) 
* Toggle all fold when clicked with VK_CONTROL in folding margin
* Left parts of status bar are shown atrributs of current document, right parts are shown attributs of scintiall view.
* config files : n4d.settings - options, n4d.shortcuts - keyboard shortcuts, n4d.styles - user defined lexer styles, n4d.snippets - auto complete for code snippet

Customize Command Palette:
1. Open n4d.rc file
2. Add your command string to string table using command id as string id. (refer support command id in resource.h)
3. Command string follow this pattern: {short name}###{description}. 

Build Options:
Default optimizations are favor size, if you have performance issues, pls use favor speed.

Below is Screenshot :

Dark theme:

![image](https://user-images.githubusercontent.com/28701482/219947679-1442ab44-2783-4a9a-bdba-4130f230f88f.png)

Light theme:
![image](https://user-images.githubusercontent.com/28701482/219947715-ada36d03-088a-4057-8c47-332293d17bb4.png)

