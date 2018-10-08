VERSION=1.0
PKGVERSION=irq-heatmap-$(VERSION)
RPM_BUILD_DIR=build/$(PKGVERSION)
FILES=irq_heatmap.c irq_numa.c irq_numa.h
EMPTY_DIRS=log

all: irq_heatmap

irq_heatmap: $(FILES) 
	gcc -Wall -g -o irq_heatmap irq_heatmap.c irq_numa.c -l numa

irq_numa: irq_numa.h irq_numa.c 
	gcc -Wall -g -DDEBUG -o irq_numa irq_numa.c -l numa

clean: 
	rm -f *.o *~ irq_heatmap irq_numa $(PKGVERSION).tar.gz
	rm -rf build/*

$(PKGVERSION).tar.gz:
	make clean
	test ! -d $(RPM_BUILD_DIR) && mkdir -p $(RPM_BUILD_DIR) 
	cp -rp $(FILES)  $(RPM_BUILD_DIR)
	cp -rp irq-heatmap.spec Makefile configure $(RPM_BUILD_DIR)
	cp -rp etc $(RPM_BUILD_DIR)
	cd build; tar czvf ../$(PKGVERSION).tar.gz --exclude=".svn" --exclude=".git" $(PKGVERSION)
	
install: irq_heatmap
	mkdir -p $(DESTDIR)/opt/irq-heatmap/bin
	cp -p irq_heatmap $(DESTDIR)/opt/irq-heatmap/bin/
	mkdir -p $(DESTDIR)/etc/profile.d
	cp -p etc/irq-heatmap.sh $(DESTDIR)/etc/profile.d/
	 
rpm:  $(PKGVERSION).tar.gz
	cp -f $(PKGVERSION).tar.gz ~/rpmbuild/SOURCES/
	rpmbuild -bb irq-heatmap.spec
		 