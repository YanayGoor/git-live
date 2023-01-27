NAME := git-live
VERSION := 0.1.0
DEB_ARCH := $(shell dpkg --print-architecture)
DEB_PATH := $(NAME)_$(VERSION)_$(DEB_ARCH)

CC = gcc
LD = ld
CFLAGS := -Wall -Wextra -Werror -g

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
	echo "Description: A Live git Dashboard." >> $(DEB_PATH)/DEBIAN/control
	mkdir -p debian/control
	RESULT=`dpkg-shlibdeps -O $(NAME)` && echo "Depends: $${RESULT#'shlibs:Depends='}" >> $(DEB_PATH)/DEBIAN/control
	rm -r debian
	dpkg-deb --build --root-owner-group $(DEB_PATH)
	dpkg-name $(DEB_PATH).deb


.PHONY: clean all