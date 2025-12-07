CC=gcc
CFLAGS=-pthread -Wall -Wextra -O2

SRCS=src/main.c src/kiosk.c src/scheduler.c src/queue.c
OBJS=$(SRCS:.c=.o)

TARGET=er_sim

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS)

clean:
	rm -f $(TARGET) src/*.o