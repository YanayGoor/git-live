NAME := liblayout

CC = gcc
LD = ld
CFLAGS := -Wall -Wextra -Werror -g  -std=c11 -D_BSD_SOURCE -D_DEFAULT_SOURCE

SRC += layout.c
OBJ = $(patsubst %.c,%.o,$(SRC))

all: $(NAME).so $(NAME).a

$(NAME).so: $(OBJ)
	$(CC) -o $@ $^ -lc -shared

$(NAME).a: $(OBJ)
	ar rcs $@ $^

%.o: %.c
	$(CC) -fPIC $(CFLAGS) -c -o $@ $<

clean:
	rm -rf $(NAME).so $(NAME).a $(OBJ)

format:
	clang-format -i $(SRC)

.PHONY: clean all
