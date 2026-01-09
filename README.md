Sklaffkom is a Swedish, command based BBS system (also known as [KOM](https://en.wikipedia.org/wiki/KOM_(bulletin_board_system))) written for SunOS 5.x, later ported to Interactive Unix, NetBSD, FreeBSD and later (sort-of) ported to Linux. As of mid-2025 the Linux port is fully working. 

The BBS system was originally written by Torbjörn Bååth, Peter Forsberg, Peter Lindberg, Odd Petersson and Carl Sundbom in the winter of 1992. Later additions was written by Daniel Grönjord and Olof Runborg. 

Sklaffkom was, and still is, distributed under the GPL 2.0 license from 1991. 

It was never meant to be a complete, permanent BBS system but rather a temporary solution in wait for the grand new big BBS system that arrived 10+ years later in the form of [EasyKOM](https://sv.wikipedia.org/wiki/EasyKOM). 

The reason for keeping this old codebase alive and working on modern operating systems is simple: it is a big part of the cultural heritage of a small BBS community in Sweden. The goal is, of course, to have Sklaffkom support a modern version of a modern operating system to be able to open this BBS system to online users again. Because we can. 

This version of Sklaffkom is based on the last publically released version by the original team behind Sklaffkom. Later additions to Sklaffkom was added but never released to the world.  

The source code and documentation was updated in June-July 2025 (and onwards) to run on modern 64-bit OSes by Petri Stenberg, Fredrik Björeman, Peter London, Marcus Sundberg and Joakim Melin. Please see the [changelog](https://github.com/joakimmelin/sklaffkom/blob/master/ChangeLog) (not a 100% complete list) for information on what has been changed. 

As of 2025-06-09 it has been verified to run with FreeBSD, Linux and MacOS 15.5 x86_64. Complete instructions on how to compile and install Sklaffkom can be found [here](https://github.com/joakimmelin/sklaffkom/wiki). 

Since the ancient nlist() hack to find command functions has now been replaced with plain ISO C it should be able to run on almost any modern *nix-like OS (both 32- and 64-bit) with only minor modifications.

Sklaffkom is dedicated to the memory of Staffan Bergström, a BBS-user who died much too young.   
