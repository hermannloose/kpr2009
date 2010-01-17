#!/bin/bash

kvm -no-kvm -tftp `pwd`/tftpboot -bootp /boot/grub/pxegrub -boot n -m 512 -serial stdio -no-reboot
