# Notepad for Developer - N4D
N4D is a fork of BowPad by Stefan Kueng and modified with my favorites.

* Remove Ribbon UI, instead by operate using Keyboard and Popupmenu 
* Custom Draw title bar and tab bar
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
* Left click with VK_CONTROL on add button (left of title bar) to open folder, right click to show recent opened folders
* First item of FileTree is changed to show current folder name with RED color and BOLD font (Removed)
* Left button of tab bar is used to toggle file tree
* Show all opened files when right click on tab scroll arrow with filterable dialog.
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

![image](https://user-images.githubusercontent.com/28701482/184499462-af0cbeff-1369-49ca-b020-04e8d7857559.png)

Light theme:

![image](https://user-images.githubusercontent.com/28701482/184500277-6fa317c9-9f14-4510-ac26-84ccae5d3ac8.png)
