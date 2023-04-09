NAME := git-live
VERSION := 0.2.0
DEB_ARCH := $(shell dpkg --print-architecture 2> /dev/null)
DEB_PATH := $(NAME)_$(VERSION)_$(DEB_ARCH)

DEPS = Makefile.depends

CC = gcc
LD = ld
CFLAGS := -Wall -Wextra -Werror -std=c11 -D_BSD_SOURCE -D_DEFAULT_SOURCE
LDLAGS := -lc

SRCS += src/main.c
SRCS += src/utils.c
SRCS += src/dashboard.c
SRCS += src/attach.c
SRCS += src/ncurses_layout.c
SRCS += src/timing.c
SRCS += lib/err.c

OBJS = $(patsubst %.c,%.o,$(SRCS))

LIBS += -lgit2
LIBS += -lncurses
LIBS += -ltinfo

STATIC_LIBS += lib/layout/liblayout.a

# TODO: make this work correctly with clion
INCLUDES += -I./

ifdef CHECK_BOUNDS
CFLAGS += -D CHECK_BOUNDS
endif

ifdef DEBUG
CFLAGS += -g
endif

ifdef PROFILE
CFLAGS += -pg
endif

define DEBIAN_CONTROL
Package: $(NAME)
Version: $(VERSION)
Architecture: $(DEB_ARCH)
Maintainer: Yanay Goor <yanay.goor@gmail.com>
Description: A live git dashboard.
Depends: $(shell mkdir -p debian/control; RESULT=`dpkg-shlibdeps -O $(NAME) 2> /dev/null` && echo "$${RESULT#'shlibs:Depends='}"; rm -r debian)
endef
export DEBIAN_CONTROL

define RPM_SPEC
Name: $(NAME)
Version: $(VERSION)
Release: 1%{?dist}
Summary: A live git dashboard.
License: GPLv3
URL: https://github/YanayGoor/git-live
Requires: libgit2 >= 0.26.8, ncurses >= 5.9
%description
%build
%install
mkdir -p %{buildroot}/usr/bin
mkdir -p %{buildroot}/etc/profile.d/
%{__cp} %{_sourcedir}/$(NAME) %{buildroot}/usr/bin
%{__cp} %{_sourcedir}/$(NAME).sh %{buildroot}/etc/profile.d
%files
/usr/bin/$(NAME)
/etc/profile.d/$(NAME).sh
endef
export RPM_SPEC

all: $(NAME)

$(NAME): $(OBJS) $(STATIC_LIBS)
	$(CC) $(LDLAGS) -o $@ $^ $(LIBS) $(INCLUDES)

lib/layout/liblayout.a: lib/layout/layout.c
	$(MAKE) -C lib/layout liblayout.a

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -rf $(NAME) $(OBJS)
	rm -rf build/
	rm -rf $(DEB_PATH).deb
	$(MAKE) -C lib/layout clean

format:
	clang-format -i $(SRCS)
	$(MAKE) -C lib/layout format

deb: $(NAME)
	rm -rf $(DEB_PATH)
	mkdir $(DEB_PATH)
	mkdir -p $(DEB_PATH)/usr/bin
	mkdir -p $(DEB_PATH)/etc/profile.d/
	cp package_files/$(NAME).sh $(DEB_PATH)/etc/profile.d/
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
	rpmbuild -ba  ~/rpmbuild/SPECS/$(NAME).spec
	mkdir -p build/
	cp ~/rpmbuild/RPMS/*/$(NAME)* build

$(DEPS):
	$(CC) $(INCLUDES) -MM $(SRCS) > $(DEPS)

include $(DEPS)

.PHONY: all clean rpm deb depend
