BUILDDIR = _build

all: $(BUILDDIR)/build.ninja
	ninja -C $(BUILDDIR)

$(BUILDDIR)/build.ninja:
	meson setup $(BUILDDIR) -Dprefix=/usr

install: all
	ninja -C $(BUILDDIR) install

uninstall:
	ninja -C $(BUILDDIR) uninstall

clean:
	rm -rf $(BUILDDIR)

.PHONY: all install uninstall clean
