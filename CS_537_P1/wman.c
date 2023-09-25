/*
Author: Jie Wang
Course: CS537 Fall-2023
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
int main(int argc, char const *argv[])
{
    // checking arguments those are valiated
    if (argc != 2 && argc != 3)
    {
        printf("What manual page do you want?\nFor example, try 'wman wman'\n");
        exit(0);
    }
    // creating a ptr to FILE
    FILE *fptr;
    // saving the general path of the files
    const char *general_addr = "./man_pages/man";

    // case 1: single argument
    if (argc == 2)
    {
        const char *filename = argv[1];
        int current_section = 1;
        while (current_section <= 9)
        {
            char path[512]; // Adjust the buffer size as needed
            sprintf(path, "%s%d/%s.%d", general_addr, current_section, filename,
                    current_section);

            fptr = fopen(path, "r");
            if (fptr != NULL)
            {
                char content[1000];
                while (fgets(content, 1000, fptr))
                {
                    printf("%s", content);
                }
                fclose(fptr); // Close the file when done
                return 0;
            }
            else
            {
                if (errno == EISDIR || errno == EACCES || errno == EINVAL || errno == ENOMEM)
                {
                    printf("cannot open file\n");
                    exit(1);
                }
                current_section++;
            }
        }
        printf("No manual entry for %s\n", filename);
        exit(0);
    }
    else
    {
        int specific_section = atoi(argv[1]);
        if (specific_section < 1 || specific_section > 9)
        {
            printf("invalid section\n");
            exit(1);
        }
        const char *filename = argv[2];
        char path[521];
        sprintf(path, "%s%d/%s.%d", general_addr, specific_section, filename,
                specific_section);
        fptr = fopen(path, "r");
        if (fptr != NULL)
        {
            char content[1000];
            while (fgets(content, 1000, fptr))
            {
                printf("%s", content);
            }
            fclose(fptr);
            return 0;
        }
        else
        {
            printf("No manual entry for %s in section %d\n", filename, specific_section);
            exit(0);
        }
    }
}
