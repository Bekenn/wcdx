[wcdx](https://github.com/Bekenn/wcdx)
======================================

DirectX DLL and other enhancements for Wing Commander Kilrathi Saga

Here are the main features:

* Replaced DirectDraw calls with Direct3D 9.
    * D3D should be more maintainable going forward should the need arise to add new features or fix unexpected bugs.
    * The game no longer tries to use a hardware palette, which was poorly supported in Vista and later operating systems.
    * The game no longer switches the display resolution.  Instead, it blits to a desktop-sized window.  I felt a need to do this because my display (an old 23" Apple Cinema Display) couldn't handle a 320x200 resolution, but there are other benefits.  By determining the precise boundaries of the image on your display, the game can ensure a correct 4:3 aspect ratio no matter what your display's size actually is.  Additionally, without a mode switch, the game now goes instantly into full screen mode and back.  (This is what some current games refer to as "windowed fullscreen.")
    * Oh, yeah, I also added windowed mode.  At any point in the game, hit Alt-Enter to toggle between full-screen and windowed modes.  The game will pick a pretty good default windowed size, but you can resize it to your heart's delight.
* Removed all privileged instructions/API calls.
    * The game can now be run without using compatibility mode and without requiring administrative privileges.
    * The game can now be run without using administrative privileges.
    * Really.  Please stop elevating the game's privilege level.  UAC exists for a reason; don't disable it.
* Compatible with Windows XP and up.
    * Actually, I haven't tested it on Windows XP; let me know how it works.
* Fixed that annoying music bug.
    * You know the one.  You take out that fleeing salthi, and then you get that victory fanfare, and then you get that victory fanfare, and then you get that victory fanfare, and then your roommate goes crazy and tosses your computer out the nearest window.  (Old times...)
    * Actually, this is technically a separate patch because it was a data issue, but in any case it's fixed.
* Secret Missions 1 and 2 support!
* Now with support for Wing Commander 2!
    * And Special Operations 1!
    * And Special Operations 2!
* Tools for all (well, some) of your data extraction needs!  This release includes tools for extracting game resources from the data files, including sprites, fonts, and more!
    * wcres for extracting resources
    * wcimg for converting extracted resources to PNG images
    * wc2font for converting the resources in fonts.fnt to PNG images
* Do you love George Oldziey's prerendered digital arrangements of the original MIDI scores?  With wcjukebox, now you can sit back, relax, and let the WAVs wash over you!

Caveats:

* The patch only works for the executables distributed with the add-on packs for WC1 and WC2 (available [here][1]).  If you haven't installed Secret Missions or Special Operations, the patch will do you no good.
* Cut scenes in WC2 are... well, they're not quite right.  They're overly responsive to player input, skipping important bits of dialogue here and there (especially if you click to move to the next line; this will often skip two lines of dialogue).  The timings are also pretty bad.  For whatever reason, Origin reworked the scripted sequences to play back at a very high frame rate, but didn't adjust all the timings properly, resulting in bugs such as fighter cockpits floating in the middle of space, without the rest of the fighter attached.  This is a data issue, and I haven't figured out a good approach to fixing it yet.
* I haven't yet touched WC3.  It's a lot of work, but it's not impossible that I'll do something with it at some point.

How to use it:

1.  Install the game.  You can use the installer from the CD if you wish, but my preferred method is to simply take the WC1 or WC2 folder and copy it somewhere.  If you use the installer, be sure to go back and copy the streams directory afterward.  This will allow you to run the game without the CD, and (more importantly) is required in order for the music patch to work.
2.  Install the Secret Missions and Special Operations [add-on packs][1].  Once again, you can use the included installer, or you can simply copy the files over.  If you choose to do it manually, note that every file that isn't an executable (.exe) belongs in the gamedat directory.
3.  For Wing Commander 1:
    1.  Copy wcdx.dll, wcpatch.exe, and patchmusic.exe into the directory containing Wing1.exe.
    2.  Open a command prompt and run this command: wcpatch Wing1.exe Wing1_wcdx.exe
    3.  Run this other command: patchmusic streams\mission.str
    4.  Try it out!  Double-click on Wing1_wcdx to start the game.  If everything works the way you expect it to, you can delete your old Wing1.exe and rename Wing1_wcdx back to Wing1.
    5.  To play the Secret Missions add-on packs, Repeat step 2 for SM1.EXE, SM2.EXE, and TRANSFER.EXE.
4.  For Wing Commander 2:
    1.  Do the same thing you did for Wing Commander 1.  You'll need to patch Wing2.exe, SO1.exe, and SO2.exe.
    2.  There's no need to run patchmusic for Wing Commander 2.

Acknowledgements
----------------

Wing Commander and Wing Commander Kilrathi Saga are products of the fine folks at [EA](http://www.ea.com/).  Just about all of the games from the series _except for Kilrathi Saga_ are currently cheaply available at [GOG](http://www.gog.com/) and via EA's [Origin](http://www.origin.com/) store.  If you don't have the game and would like to try Wing Commander, please visit one of those stores for an experience that's only slightly less awesome than Kilrathi Saga.

[1]: http://www.wcnews.com/wcpedia/Category:Downloads#Wing_Commander
