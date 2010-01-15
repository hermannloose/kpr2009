#!/bin/bash

qemu -tftp `pwd`/tftpboot -bootp  /boot/grub/pxegrub -boot n -m 512 -serial stdio -no-reboot
