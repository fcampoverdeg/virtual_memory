
CC = gcc

CFLAGS = -Wall -g -O2

TARGET = memalloc

OBJS = memalloc.o pagemap.o 

all: $(TARGET)

$(TARGET): $(OBJS)
			 $(CC) $(CFLAGS) -o $(TARGET) $(OBJS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

.PHONY: clean
clean: 
	rm -f $(TARGET) $(OBJS)

