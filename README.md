# fx

This is a simple X11 file explorer for Linux. I wrote this for personal use, so it
isn't very polished and it may look different on your machine.

## How to build

1. You need Xlib - if you are using a Debian-based distro you can get it with `apt-get install libx11-dev`

2. Build a release binary with `make`. The binary will be named `fx`; install it wherever you want.

## How to use it

1. Have an X server running

2. Start `fx` from anywhere, and a window will pop up with the contents of the current working directory.
   You can scroll the list up or down, and click into directories to move into them. Press `q` to quit
   and `d` to show some debug boxes.
