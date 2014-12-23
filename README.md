wcdx
====

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

The bad news:

* I've so far only done this for Wing Commander 1 itself.  I haven't yet touched WC2, WC3, or any of the mission packs.  It's a lot of work, but it's not impossible that I'll get around to the others.
* The patch only works for the version of Wing Commander 1 distributed with the Secret Missions pack (available here).  If you haven't installed the Secret Missions pack, the patch will do you no good.

The surprisingly awesome news:

* Because it's a data patch, the annoying music bug is fixed throughout both Wing Commander 1 and the Secret Missions pack.  (I vaguely recall that it wasn't an issue in WC2.)
