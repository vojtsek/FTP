Simple implementation of FTP server according to RFC 959.
Author: Vojtech Hudecek, 2014
Developed as a school project.

Installation
1. copy the program to the desired directory
2. set this directory as ROO_DIR in headers/conf.h
3. set other variables in this file, paths are relative to ROOT_DIR, must start by slash, but cannot end with it
4. run make in parent directory
5 ./run [-c port_to_listen_on]

NOTE: the program must be run with root priveleges because of use of chroot function
after the chroot it switches the UID to a one defined in header/conf.h.
However, it's essential to have root privileges if you want to use well known ports.
