#include "types.h"
#include "stat.h"
#include "user.h"

int main(int argc, char *argv[])
{
	char filename[256];
    getlastcat(filename);
    printf(1, "XV6_TEST_OUTPUT Last catted filename: %s\n", filename);
    exit();
}
