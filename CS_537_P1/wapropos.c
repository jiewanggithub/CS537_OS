/*
Author: Jie Wang
Course: CS537 Fall-2023
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <ctype.h>

#define MAX_PATH_LENGTH 512
#define MAX_LINE_LENGTH 1000
int countOfPages = 0;
// Function to trim leading and trailing whitespace from a string
char *trim(char *str)
{
    // two variables to track the positions of the fist and last now-white space
    // characters in the string
    int start = 0;
    int end = strlen(str) - 1;

    // iterate uitle ecounter the first non whitespce character in the string
    // if it is a white space start++
    while (isspace((unsigned char)str[start]))
    {
        start++;
    }
    // entrire string is whitespace
    if (start > end)
    {
        str[0] = '\0'; // Entire string is whitespace
        return str;
    }
    // iterate from end of the string
    while (end > start && isspace((unsigned char)str[end]))
    {
        end--;
    }

    int trimmed_length = end - start + 1;
    // trim the stre
    memmove(str, str + start, trimmed_length);
    str[trimmed_length] = '\0';

    return str;
}

// Function to remove all characters before the first hyphen '-' in a string
void removeBeforeHyphen(char *str)
{
    char *hyphen_pos = strchr(str, '-');
    if (hyphen_pos != NULL)
    {
        int hyphen_index = hyphen_pos - str;
        memmove(str, str + hyphen_index + 1, strlen(str) - hyphen_index);
    }
}

// Function to search for a keyword in the specified directory and print matching results
void searchAndPrintKeyword(const char *keyword, const char *directory, int section)
{
    DIR *dir = opendir(directory);
    if (dir == NULL)
    {
        // perror("Unable to open directory");
        // return 0;
    }

    struct dirent *entry;

    while ((entry = readdir(dir)))
    {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
        {
            continue;
        }

        char current_file[MAX_PATH_LENGTH];
        snprintf(current_file, sizeof(current_file), "%s/%s", directory, entry->d_name);

        FILE *file = fopen(current_file, "r");
        //  printf("%s\n",current_file);
        if (file == NULL)
        {
            continue; // Skip files that cannot be opened
        }

        char line[MAX_LINE_LENGTH];
        char description[MAX_LINE_LENGTH] = "";
        int foundKeyword = 0;

        while (fgets(line, sizeof(line), file))
        {
            if (strstr(line, "NAME") != NULL)
            {
                foundKeyword = 1;
                continue;
            }

            if (foundKeyword == 1)
            {
                strncpy(description, line, sizeof(description) - 1);
                description[sizeof(description) - 1] = '\0';
                trim(description);               // Trim the description on both sides
                removeBeforeHyphen(description); // Remove text before the hyphen
                break;
            }
        }

        int scan = 0;
        int count = 0;
        int end = 0;

        FILE *filetoscan = fopen(current_file, "r");
        while (fgets(line, sizeof(line), filetoscan))
        {
            if (strstr(line, "NAME") != NULL && count == 0)
            {
                scan = 1;
                count++;
                continue;
            }
            if (strstr(line, "SYNOPSIS") != NULL)
            {
                scan = 0;
                continue;
            }
            if (strstr(line, "DESCRIPTION") != NULL)
            {
                scan = 1;
                continue;
            }
            if (strstr(line, "FORMATTED INPUT FILES") != NULL)
            {
                scan = 0;
                end = 1;
                continue;
            }
            if ((strstr(line, keyword) != NULL && scan == 1) && end != 1)
            {
                entry->d_name[strlen(entry->d_name) - 2] = '\0';
                countOfPages++;
                printf("%s (%d) -%s\n", entry->d_name, section, description);
                break;
            }
        }
        fclose(file);
        fclose(filetoscan);
    }
    closedir(dir);
}

int main(int argc, char const *argv[])
{
    if (argc == 1)
    {
        printf("wapropos what?\n");
        exit(0);
    }

    const char *directory_path = "./man_pages/man";

    for (int section = 1; section <= 9; section++)
    {
        char current_section_path[MAX_PATH_LENGTH];
        snprintf(current_section_path, sizeof(current_section_path), "%s%d", directory_path, section);
        searchAndPrintKeyword(argv[1], current_section_path, section);
    }
    if (countOfPages == 0)
    {
        printf("nothing appropriate\n");
        exit(0);
    }
    exit(0);
}
