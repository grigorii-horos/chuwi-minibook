all:
	$(MAKE) -f Makefile

install-arch:
	cd packaging/arch && makepkg -si

%:
	$(MAKE) -f Makefile $@

.PHONY: all install-arch
