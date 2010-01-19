#!/bin/bash

qemu -tftp `pwd`/tftpboot -bootp `pwd`/boot/grub/pxegrub -boot n -m 512 -serial stdio -no-reboot
