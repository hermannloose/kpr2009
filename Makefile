all: kernel l4_build

.PHONY : kernel l4_build clean cleanall

kernel:
	PWD=$(PWD)/kernel/fiasco $(MAKE) -C kernel/fiasco

$(PWD)/build/.config.all:
	PWD=$(PWD)/l4 $(MAKE) O=$(PWD)/build -C l4 oldconfig

l4_build: $(PWD)/build/.config.all
	PWD=$(PWD)/build $(MAKE) -C build

clean:
	PWD=$(PWD)/kernel/fiasco/build $(MAKE) -C kernel/fiasco/build $@
	PWD=$(PWD)/build $(MAKE) -C build $@

cleanall:
	find kernel/fiasco/build -maxdepth 1 -mindepth 1 ! -name globalconfig.out ! -name .svn -print0 | xargs -0 $(RM) -rf
	$(RM) -rf build
