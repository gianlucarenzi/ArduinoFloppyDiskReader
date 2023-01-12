Run this as standalone Xorg application with:

xinit ./[EXEC] -platform xcb

For VNC usage:

xinit ./[EXEC] -platform xcb -platform vnc:size=1280x800

After connecting to 5900 port with a viewer it will be possible to use it.

R.G.
