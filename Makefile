CC=gcc
CFLAGS=-Wall -Wextra -Wpedantic -std=c99 -O3

dualpriotest: dualpriotest.c
	$(CC) $(CFLAGS) -o dualpriotest dualpriotest.c
