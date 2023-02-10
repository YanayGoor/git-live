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

define DEBIAN_CONTROL
Package: $(NAME)
Version: $(VERSION)
Architecture: $(DEB_ARCH)
Maintainer: Yanay Goor <yanay.goor@gmail.com>
Description: A live git dashboard.
Depends: $(shell mkdir -p debian/control; RESULT=`dpkg-shlibdeps -O $(NAME)` && echo "$${RESULT#'shlibs:Depends='}"; rm -r debian)
endef
export DEBIAN_CONTROL

define RPM_SPEC
Name: $(NAME)
Version: $(VERSION)
Release: 1%{?dist}
Summary: A live git dashboard.
License: GPLv3
URL: https://github/YanayGoor/git-live
Requires: libgit2 >=
%description
%build
%install
mkdir -p %{buildroot}/usr/bin
mkdir -p %{buildroot}/etc/profile.d/
%{__cp} %{_sourcedir}/$(NAME) %{buildroot}/usr/bin
%{__cp} %{_sourcedir}/$(NAME)-attach %{buildroot}/usr/bin
%{__cp} %{_sourcedir}/$(NAME)-detach %{buildroot}/usr/bin
%{__cp} %{_sourcedir}/$(NAME).sh %{buildroot}/etc/profile.d
%files
/usr/bin/$(NAME)
/usr/bin/$(NAME)-attach
/usr/bin/$(NAME)-detach
/etc/profile.d/$(NAME).sh
endef
export RPM_SPEC

all: $(NAME)

$(NAME): $(OBJ) $(STATIC_LIBS)
	$(CC) -o $@ $^ -lc -lgit2 -lncurses -ltinfo

lib/layout/liblayout.a: lib/layout/layout.c
	$(MAKE) -C lib/layout liblayout.a

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -rf $(NAME) $(OBJ)
	rm -rf build/
	rm -rf $(DEB_PATH).deb
	$(MAKE) -C lib/layout clean

format:
	clang-format -i $(SRC)
	$(MAKE) -C lib/layout format

deb: $(NAME)
	rm -rf $(DEB_PATH)
	mkdir $(DEB_PATH)
	mkdir -p $(DEB_PATH)/usr/bin
	mkdir -p $(DEB_PATH)/etc/profile.d/
	cp package_files/$(NAME).sh $(DEB_PATH)/etc/profile.d/
	cp package_files/$(NAME)-attach.sh $(DEB_PATH)/usr/bin/$(NAME)-attach
	cp package_files/$(NAME)-detach.sh $(DEB_PATH)/usr/bin/$(NAME)-detach
	cp $(NAME) $(DEB_PATH)/usr/bin/
	mkdir -p $(DEB_PATH)/DEBIAN
	touch $(DEB_PATH)/DEBIAN/control
	echo "$$DEBIAN_CONTROL" >> $(DEB_PATH)/DEBIAN/control
	dpkg-deb --build --root-owner-group $(DEB_PATH)
	mkdir -p build/
	mv $(DEB_PATH).deb build/
	

rpm: $(NAME)
	rm -rf ~/rpmbuild
	rpmdev-setuptree
	echo "$$RPM_SPEC" >> ~/rpmbuild/SPECS/$(NAME).spec
	cp $(NAME) ~/rpmbuild/SOURCES
	cp package_files/$(NAME).sh ~/rpmbuild/SOURCES
	cp package_files/$(NAME)-attach.sh ~/rpmbuild/SOURCES/$(NAME)-attach
	cp package_files/$(NAME)-detach.sh ~/rpmbuild/SOURCES/$(NAME)-detach
	rpmbuild -ba  ~/rpmbuild/SPECS/$(NAME).spec
	mkdir -p build/
	cp ~/rpmbuild/RPMS/*/$(NAME)* build
	

.PHONY: clean all
