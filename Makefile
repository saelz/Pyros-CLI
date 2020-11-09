WARNINGS=-Wall -Werror -Wextra -Wdeclaration-after-statement

BUILD_NAME = pyros
LIBS = '-lpyros'

CFLAGS=$(WARNINGS) -std=c99 -pedantic

LDFLAGS=$(LIBS)

SRC=pyros.c files.c commands.c
OBJS=$(SRC:.c=.o)

all: $(BUILD_NAME)

%.o: %.c
	$(CC) -c -o $(@F) $(CFLAGS) $<

$(BUILD_NAME): $(OBJS)
	$(CC) -o $(@F) $(OBJS) $(LDFLAGS)

clean:
	rm $(OBJS)
	rm $(BUILD_NAME)
