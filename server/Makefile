CROSS_COMPILE ?=
CC ?= $(CROSS_COMPILE)gcc

TARGET = aesdsocket
SRCS = aesdsocket.c
OBJS = $(SRCS:.c=.o)
CFLAGS = -Wall

default: all
all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $(TARGET)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)
