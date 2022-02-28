# KeyMapper

This is a project designed to run on a teensy 4.1 that can remap keyboard input
and provides some basic support for layers. It also supports emulating mouse
input, but does not support forwarding mouse input. It requires an SD card with
a set of config files to be present on the teensy. A sample config file is
present in the config/ directory.

# TODOs

+ Tap/hold keys
+ Ways to configure mouse settings
+ Copy config to internal storage and use that if sd card is not present (i.e.
  sd card is used for "reprogramming")
+ Key macros?
+ Maybe some kind of display for better aesthetics?
+ Allow host to mount the sd card without removing it from the teensy
