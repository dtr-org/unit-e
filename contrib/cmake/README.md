cmake project
=============

Unit E uses the GNU autotools to build the project. Some IDEs like IntelliJ/CLion do not support
the autotools, but CMake. This directory contains scripts to assist with creating a CMakeLists.txt
suitable for these IDEs.

Currently the script has only been tested on macOS 10.13 using cmake 3.11 in CLion 2018.2. It
assumes that dependencies like Qt have been installed using homebrew.

usage
-----

1. execute `gen-cmakelists.sh` (this will create `CMakeLists.txt` in the git repositories root).
2. import into CLion (also works when already imported, refresh the project definition then)

caveats
-------

This project definition is good enough to view and edit code in CLion with some reasonable
amount of comfort. It does not equib CLion to actually build your project (for this you will
still want to to `./autogen.sh` once, `./configure [your config]`, and `make`). There are ways
to make the build button execute this scripts for example by adding the following to your
`CMakeLists.txt`:

```
add_custom_target(build-unite ALL
        COMMAND ./autogen.sh
        COMMAND ./configure --enable-debug
        COMMAND $(MAKE) WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
```

Beware this will re-run `./configure` every time.

debugging
---------

When choosing to run a target in CLion it will ask you for the actual executable to run.
If you already built the project (with `--enable-debug` enabled) you can just pick the
executable from `src/` and start debugging. CLion should pick that up and connect the code
with your running executable (breakpoints etc. work).

