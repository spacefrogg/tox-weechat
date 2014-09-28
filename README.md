Tox-WeeChat
===========
Tox-WeeChat is a plugin for [WeeChat][1] that enables it to connect to the [Tox][2] network. It is functional, but currently only intended for experimental use.

Current build status: [![Build Status](https://travis-ci.org/haavardp/tox-weechat.svg?branch=master)](https://travis-ci.org/haavardp/tox-weechat)

Installation
------------
> Tox-WeeChat is available in the [AUR][3].

Tox-WeeChat requires [WeeChat][1] >= 1.0 and the latest-ish [libtoxcore][4]. It also requires CMake to be built. Installation is fairly simple; after getting the source, compile and install using CMake:

    $ mkdir build && cd build
    $ cmake -DHOME_FOLDER_INSTALL=ON ..
    $ make
    $ make install

This installs the plugin binary `tox.so` to the recommended location `~/.weechat/plugins`. Without the home folder flag, the binary is placed in `/usr/local/lib/weechat/plugins`. Installing to a custom WeeChat folder or elsewhere is achieved by setting `INSTALL_PATH`.

Usage
-----
 - If the plugin does not automatically load, load it with `/plugin load tox`. You may have to specify the full path to the plugin binary.
 - Create a new profile with `/tox create <name>`. The data file is stored in `~/.weechat/tox/` by default.
 - Load your profile and connect to the Tox network with `/tox load <name>`.
 - Change your name with `/name <new name>`.
 - Get your Tox ID with `/myid`.
 - To add friends or respond to friend requests, `/help friend` will get you started.
 - Message a friend with `/msg <friend number>`. Get their friend number with `/friend list`.

Run `/help -list tox` to get a list of all available commands.

TODO
----
 - Persist data (friend requests etc.)
 - Support encrypted save files
 - Tox DNS
 - Group chats
 - Support proxies (e.g. TOR)
 - A/V

License
---------
Copyright (c) 2014 Håvard Pettersson <haavard.pettersson@gmail.com>

This file is part of Tox-WeeChat.

Tox-WeeChat is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Tox-WeeChat is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Tox-WeeChat.  If not, see <http://www.gnu.org/licenses/>.

[1]: http://weechat.org
[2]: http://tox.im
[3]: https://aur.archlinux.org/packages/tox-weechat-git
[4]: https://github.com/irungentoo/toxcore

