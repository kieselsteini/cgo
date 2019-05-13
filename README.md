# cgo - a terminal gopher client

## Summary

cgo is a UNIX/Linux terminal based gopher client. It has no other dependencies
than libc and some syscalls. It should run on every VT100 compatible terminal.
To show media like images, music or webpages it relies on external programs
you can specify.


## What does cgo mean?

cgo means more or less, the "c go"pher client. And c could stand for C
(the programming language), colorful or console. You may choose one of the
meanings or propose other :) (but please not crappy!)


## How to install

Clone the repository and simply type ```make``` in order to build it.
You can install the program using ```make install```.

If you prefer you can also modify the defaults directly in the ```cgo.c```
file prior to compilation. Or you change the defaults in the configuration file
```cgorc``` provided with that repository.


## Parameters

In case you omit all parameters cgo will show you the default
gopherhole specified in the source file.

* -H               show usage
* -v               print version
* gopher URI       opens the given gopher URI


## Usage

When "surfing" in the gopherspace cgo only present you directory listings.
Every selector is preceeded with two ascii chars, or three if we run out of
selectors in the range 'aa', 'ab' ... 'zz'. By typing in these chars cgo will
jump to the given selector. Every time you jump to another directory listing
cgo generates a history entry (like every browser). To show other media cgo
uses external programs to present it (e.g. less, display, mplayer, firefox).
Following commands are understood by cgo:

* ?           help
* <           jump back in history
* *           reload directory
* [link]      show / jump to selector
* .[link]     download selector
* H           show history
* H[link]     jump to specified history item
* G[URI]      jumps right to the specified gopher URI
* B           show bookmarks
* B[link]     jump to specified bookmark item

[link] stands for the two (or three) colored letters in front of selectors.


## Configuration

 cgo reads "/etc/cgorc" and then "$(HOME)/.cgorc" for defaults. If both
 files are missing, hardcoded defaults will be used. Following configuration
 keys are recognized by cgo:

* start_uri        the gopher URI which is displayed at start
* cmd_text         command to show text files
* cmd_browser      command to HTML links
* cmd_image        command to show images
* cmd_player       command to play audio files
* color_prompt     ANSI color sequence for the prompt
* color_selector   ANSI color sequence for selectors
* bookmarkN        configure bookmarks


## Todo

* refactor the source code
    * don't use as much global variables
    * unify the the link_t / selector_t stuff
* add an option for color-less mode (no ANSI sequences)


## Bugs

* none I'm aware of :)


Feel free to use this small gopher client. I hope you'll find it as useful as
I do. Send me comments or patches it you like. I would appreciate it.
