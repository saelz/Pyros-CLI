BUILD_NAME = pyros
LIBS = '-lpyros'

CFLAGS +=-Wall -Werror -Wextra -Wdeclaration-after-statement
CFLAGS +=-std=c99 -pedantic -g

LDFLAGS=$(LIBS)

SRC=pyros.c files.c commands.c tagtree.c
OBJS=$(SRC:.c=.o)

all: $(BUILD_NAME)

%.o: %.c
	$(CC) -c -o $(@F) $(CFLAGS) $<

$(BUILD_NAME): $(OBJS)
	$(CC) -o $(@F) $^ $(LDFLAGS)

clean:
	rm $(OBJS)
	rm $(BUILD_NAME)
