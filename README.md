cgo - a terminal gopher client
==============================

Summary
-------

cgo is a UNIX/Linux terminal based gopher client. It has no other
dependencies than libc and some syscalls. It should run on every
VT100 compatible terminal. To show media like images, music or
webpages it relies on external programs you can specify.


What does cgo mean?
-------------------

cgo means more or less, the "c go"pher client. And c could
stand for C (the programming language), colorful or console.
You may choose one of the meanings or propose other :)
(but please not crappy!)


How to install
--------------

Grab the source code, open cgo.c and adjust the external programs
to your needs. You can also change the default gopherhole where
cgo connects on startup without any parameters (e.g. you can
tell cgo to connect directly to some Veronica search engine).
And if you don't like the default colors, you're able to change
them also here.
If you're done with changing the defaults, just "make" the
final binary.


Parameters
----------

In case you omit all parameters cgo will show you the default
gopherhole specified in the source file.

 * -H               show usage
 * -v               print version
 * gopher URI       opens the given gopher URI


Usage
-----

 When "surfing" in the gopherspace cgo only presents you
 with directory listings. Every selector is preceeded with two
 ascii chars, or three if we run out of selectors in the
 range 'aa', 'ab' ... 'zz'. By typing in these chars cgo will
 jump to the given selector. Every time you jump to another
 directory listing cgo generates a history entry (like every
 browser). To show other media cgo uses external programs
 to present it (e.g. less, display, mplayer, firefox).
 The following commands are understood by cgo:

  * <kbd>?</kbd>           help
  * <kbd><</kbd>           jump back in history
  * <kbd>*</kbd>           reload directory
  * [link]      show / jump to selector
  * <kbd>.</kbd>[link]     download selector
  * <kbd>H</kbd>           show history
  * <kbd>H</kbd>[link]     jump to specified history item
  * <kbd>G</kbd>[URI]      jumps right to the specified gopher URI
  * <kbd>B</kbd>           show bookmarks
  * <kbd>B</kbd>[link]     jump to specified bookmark item

[link] stands for the two (or three) colored letters in front of selectors.

Configuration
-------------

 cgo reads `/etc/cgorc` and then `$(HOME)/.cgorc` for defaults. If both
 files are missing, hardcoded defaults will be used. Following configuration
 keys are recognized by cgo:

 * `start_uri`        the gopher URI which is displayed at start
 * `cmd_text`         command to show text files
 * `cmd_browser`      command to HTML links
 * `cmd_image`        command to show images
 * `cmd_player`       command to play audio files
 * `color_prompt`     ANSI color sequence for the prompt
 * `color_selector`   ANSI color sequence for selectors
 * `verbose`          If not "false" or "off" it will show messages like "downloading" / "executing" when downloading a selector
 * `bookmarkN`        configure bookmarks

Todo
----

 * [x] Clear on New page
 * [ ] Command history
 * [ ] More colors



Bugs
----

 * none I'm aware of :)
 * many that i am not aware of :( 


Feel free to use this small gopher client. I hope you'll
find it as useful as I do. Send me comments or patches if you
like. I would appreciate it.
