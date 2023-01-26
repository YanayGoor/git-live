NAME := git-live

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
	$(MAKE) -C lib/layout clean

format:
	clang-format -i $(SRC)
	$(MAKE) -C lib/layout format

.PHONY: clean all