# Makefile for MQTT Parallel Server Project
# ????????

CC      = gcc
CFLAGS  = -Wall -Wextra -O2
TARGETS = mqtt_server mqtt_client
OBJS    = mqtt_protocol.o

.PHONY: all clean

all: $(TARGETS)

mqtt_server: mqtt_server.c $(OBJS)
	$(CC) $(CFLAGS) -o $@ mqtt_server.c $(OBJS)

mqtt_client: mqtt_client.c $(OBJS)
	$(CC) $(CFLAGS) -o $@ mqtt_client.c $(OBJS)

mqtt_protocol.o: mqtt_protocol.c mqtt_protocol.h
	$(CC) $(CFLAGS) -c -o $@ mqtt_protocol.c

clean:
	rm -f $(TARGETS) $(OBJS)
