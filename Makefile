NAME := git-live

CC = gcc
LD = ld
CFLAGS := -Wall -Wextra -g

SRC += src/main.c
SRC += src/ncurses_layout.c
OBJ = $(patsubst %.c,%.o,$(SRC))

STATIC_LIBS += layout/liblayout.a

all: $(NAME)

$(NAME): $(OBJ) $(STATIC_LIBS)
	$(CC) -o $@ $^ -lc -lgit2 -lncurses -ltinfo

layout/layout.a:
	$(MAKE) -C layout layout.a

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -rf $(NAME) $(OBJ)

format:
	clang-format -i $(SRC)

.PHONY: clean all