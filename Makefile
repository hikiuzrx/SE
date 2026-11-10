CC = cc
CFLAGS = -Wall -Wextra
N = 100

all: premiers

premiers: premiers.c
	$(CC) $(CFLAGS) -o premiers premiers.c

run: premiers
	./premiers $(N)

clean:
	rm -f premiers f.txt f1.txt f2.txt

.PHONY: all run clean
