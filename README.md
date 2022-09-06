# Test for libxmlb

This project contains part of the initialization code from snap-store/gnome-software,
and several data files, to test a double-free bug in libxmlb.

If the bug is present in the library, valgrind will detect an invalid memory access and
six warnings will be shown in the screen, everything before the text "Loaded and processed
everything". If the bug is fixed, no warning will be shown before that text.

After that text, other errors can be shown due to the code being incomplete, but they
aren't relevant.

To launch the test, put the "libxmlb" code inside a folder called "libxmlb", and run
"launch.sh".
