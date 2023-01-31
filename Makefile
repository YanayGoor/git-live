NAME := git-live
VERSION := 0.1.0
DEB_ARCH := $(shell dpkg --print-architecture)
DEB_PATH := $(NAME)_$(VERSION)_$(DEB_ARCH)

CC = gcc
LD = ld
CFLAGS := -Wall -Wextra -Werror -g -std=c11 -D_BSD_SOURCE -D_DEFAULT_SOURCE

SRC += src/main.c
SRC += src/ncurses_layout.c
OBJ = $(patsubst %.c,%.o,$(SRC))

STATIC_LIBS += lib/layout/liblayout.a

ifdef CHECK_BOUNDS
CFLAGS += -D CHECK_BOUNDS
endif

all: $(NAME)

$(NAME): $(OBJ) $(STATIC_LIBS)
	$(CC) -o $@ $^ -lc -lgit2 -lncurses -ltinfo

lib/layout/liblayout.a: lib/layout/layout.c
	$(MAKE) -C lib/layout liblayout.a

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -rf $(NAME) $(OBJ)
	rm -rf $(DEB_PATH)
	rm -rf $(DEB_PATH).deb
	$(MAKE) -C lib/layout clean

format:
	clang-format -i $(SRC)
	$(MAKE) -C lib/layout format

deb: $(NAME)
	rm -rf $(DEB_PATH)
	mkdir $(DEB_PATH)
	mkdir -p $(DEB_PATH)/usr/bin
	cp $(NAME) $(DEB_PATH)/usr/bin/
	mkdir -p $(DEB_PATH)/DEBIAN
	touch $(DEB_PATH)/DEBIAN/control
	echo "Package: $(NAME)" >> $(DEB_PATH)/DEBIAN/control
	echo "Version: $(VERSION)" >> $(DEB_PATH)/DEBIAN/control
	echo "Architecture: $(DEB_ARCH)" >> $(DEB_PATH)/DEBIAN/control
	echo "Maintainer: Yanay Goor <yanay.goor@gmail.com>" >> $(DEB_PATH)/DEBIAN/control
	echo "Description: A live git dashboard." >> $(DEB_PATH)/DEBIAN/control
	mkdir -p debian/control
	RESULT=`dpkg-shlibdeps -O $(NAME)` && echo "Depends: $${RESULT#'shlibs:Depends='}" >> $(DEB_PATH)/DEBIAN/control
	rm -r debian
	dpkg-deb --build --root-owner-group $(DEB_PATH)
	dpkg-name $(DEB_PATH).deb


rpm:
	rm -rf ~/rpmbuild
	sudo yum install -y rpmdevtools rpmlint
	rpmdev-setuptree
	tar --create --file $(NAME)-$(VERSION).tar.gz src/ lib/
	mv $(NAME)-$(VERSION).tar.gz ~/rpmbuild/SOURCES
	# rpmdev-newspec $(NAME)
	# mv $(NAME).spec ~/rpmbuild/SPECS
	echo "Name: $(NAME)" >> ~/rpmbuild/SPECS/$(NAME).spec
	echo "Version: $(VERSION)" >> ~/rpmbuild/SPECS/$(NAME).spec
	echo "Release: 1%{?dist}" >> ~/rpmbuild/SPECS/$(NAME).spec
	echo "Summary: A live git dashboard." >> ~/rpmbuild/SPECS/$(NAME).spec
	echo "License: GPLv3" >> ~/rpmbuild/SPECS/$(NAME).spec
	echo "URL: https://github/YanayGoor/git-live" >> ~/rpmbuild/SPECS/$(NAME).spec
	echo "Requires: libgit2 >= 0.26.8, ncurses >= 5.9"
	echo "%description" >> ~/rpmbuild/SPECS/$(NAME).spec
	echo "%build" >> ~/rpmbuild/SPECS/$(NAME).spec
	echo "%install" >> ~/rpmbuild/SPECS/$(NAME).spec
	echo "mkdir -p %{buildroot}/usr/bin" >> ~/rpmbuild/SPECS/$(NAME).spec
	echo "%{__cp} %{_sourcedir}/git-live %{buildroot}/usr/bin" >> ~/rpmbuild/SPECS/$(NAME).spec
	echo "%files" >> ~/rpmbuild/SPECS/$(NAME).spec
	echo "/usr/bin/git-live" >> ~/rpmbuild/SPECS/$(NAME).spec
	cp $(NAME) ~/rpmbuild/SOURCES
	rpmbuild -ba  ~/rpmbuild/SPECS/$(NAME).spec
	

.PHONY: clean all
