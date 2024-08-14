# fx

This is a simple X11 file explorer for Linux. I wrote this for personal use, so it
isn't very polished and it may look different on your machine.

## How to build

1. You need Xlib - if you are using a Debian-based distro you can get it with `apt-get install libx11-dev`

2. Build a release binary with `make`. The binary will be called `fx_bin`.

3. Install the binary and the wrapper script with `make install`. The tool will be installed to /usr/local/bin
   by default, but you can change this by setting `INSTALL_DIR`.

## How to use it

1. Have an X server running

2. Start fx with `. fx`

2. A window will pop up with the contents of the current working directory. You can scroll the list up
   or down, and click into directories to move into them. There are some keys you can press as well:
     - 'c' to close fx and `cd` to the selected directory. You need to start fx with `. fx` for this to work.
     - 'q' to quit
     - 'd' to show some debug boxes

## License

fx is licensed under the GNU Affero Public License 3 or any later version at your choice. See
[COPYING](https://github.com/Dezzmeister/fx/blob/master/COPYING) for details.
