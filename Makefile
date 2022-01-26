setup:
	gcc -std=c99 -g -Wall -o smallsh main.c
clean:
	rm -f smallsh
debug:
	valgrind -s --leak-check=yes --track-origins=yes --show-reachable=yes ./smallsh
