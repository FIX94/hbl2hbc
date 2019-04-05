# hbl2hbc
Boot from the Wii U homebrew launcher into the vWii homebrew channel, skipping controller and screen option prompts.

# Usage
1. Grab the elf from the [releases section](https://github.com/FIX94/hbl2hbc/releases/latest), put it into your `sd:/wiiu/apps/hbl2hbc/folder`.
2. If you don't want to be able to launch other Wii titles than the Open Homebrew Channel (OHBC) you can skip to step 4.
3. Create/edit `sd:/wiiu/apps/hbl2hbc/hbl2hbc.txt`, in the form of one or multiple lines in the format of "TitleID=TileName", to change the target channel, or create a menu of target channels.
4. Every time you want to get into the vWii open homebrew channel just start it in the Wii U homebrew launcher. 

If you only want LOLZ (Homebrew Channel) download [the sample `.txt`](https://github.com/FIX94/hbl2hbc/blob/master/hbl2hbc.txt).
Also, it seems only Homebrew titles are working.

This works with the [Wii U Menu forwarder by brienj](https://gbatemp.net/threads/release-wiiu2hbc-a-hbl2hbc-forwarder-channel.455991/), but of course you can't press HOME to return to the hbl then.

If you have trouble reading this Readme, please open an issue explaining what you didn't understand!
