#!/bin/bash

l4/tool/bin/isocreator iso rom.iso -f
qemu -cdrom rom.iso -m 512 -serial stdio -no-reboot &
vinagre localhost
kill %1
