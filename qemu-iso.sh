#!/bin/bash

start_vinagre() {
	sleep 2
	vinagre localhost
}

l4/tool/bin/isocreator iso rom.iso -f
start_vinagre &
qemu -cdrom rom.iso -m 512 -serial stdio -no-reboot
