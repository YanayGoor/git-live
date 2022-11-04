NAME := git-live

CC = gcc
LD = ld
CFLAGS := -Wall -Wextra -Werror -g

SRC += src/main.c
OBJ = $(patsubst %.c,%.o,$(SRC))

all: $(NAME)

$(NAME): $(OBJ)
	$(CC) -o $@ $< -lc -lgit2 -lncurses -ltinfo

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -rf $(NAME) $(OBJ)

format:
	clang-format -i $(SRC)

.PHONY: clean all