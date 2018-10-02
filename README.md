# F2F
### The meeting app for familiar strangers
With F2F (face-to-face), you can connect people nearby, which use the same application. It allows you to see other people in the map and it notifies you when someone with similar interests are nearby.

This is made in Oulu university course 521148S Ubiquitous Computing Fundamentals.


### How to build
#### Linux

1. Download U++ framework POSIX/X11 tarball from [Upp website](https://www.ultimatepp.org/www$uppweb$nightly$en-us.html).
2. Extarct tarball and install debendencies as described in `buildrequires` files. Then, run `domake` and `doinstall`.
3. At the home directory, run `git clone https://github.com/sppp/F2F.git`
4. Run U++ make, which is similar to GNU make and cmake `~/umk ~/F2F/src/,~/upp/uppsrc Client ~/.upp/theide/GCC.bm -br ~/Client`. If GCC.bm file doesn't exist, then run ~/theide once. You need to edit the file if your GNU C++ compiler is not g++.
5. Now you can run ~/Client executable.