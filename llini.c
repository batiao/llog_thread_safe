#include "llini.h"

int main(int argc, char **argv)
{
	///usr/local/bin/gcc7.3.0 ./llini.c -DDEBUG_TEST -O0 -g
	return parse_ini(argv[1], NULL, NULL);
}
