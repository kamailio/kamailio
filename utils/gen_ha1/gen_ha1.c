#include <stdio.h>
#include <stdlib.h>
#include "../../md5global.h"
#include "../../md5.h"
#include "calc.h"



int main(int argc, char** argv)
{
	HASHHEX ha1;

	if (argc != 4) {
		printf("Usage: gen_ha1 <username> <realm> <password> \n");
		return EXIT_SUCCESS;
	}

	DigestCalcHA1("md5", argv[1], argv[2], argv[3], "", "", ha1);
	printf("%s\n", ha1);
	
	return EXIT_SUCCESS;
}
