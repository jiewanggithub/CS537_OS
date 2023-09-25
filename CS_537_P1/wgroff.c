/*
Author: Jie Wang
Course: CS537 Fall-2023
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

void removeNewlines(char *str)
{
    int len = strlen(str);
    int readIndex = 0, writeIndex = 0;

    while (readIndex < len)
    {
        if (str[readIndex] != '\n')
        {
            str[writeIndex] = str[readIndex];
            writeIndex++;
        }
        readIndex++;
    }

    // Null-terminate the modified string.
    str[writeIndex] = '\0';
}
void convertFormattingMarks(char *line, FILE *outputFile)
{
    char *formatted = line;
    while (*formatted)
    {
        if (strncmp(formatted, "/fB", 3) == 0)
        {
            fputs("\033[1m", outputFile); // ANSI bold
            formatted += 3;
        }
        else if (strncmp(formatted, "/fI", 3) == 0)
        {
            fputs("\033[3m", outputFile); // ANSI italic
            formatted += 3;
        }
        else if (strncmp(formatted, "/fU", 3) == 0)
        {
            fputs("\033[4m", outputFile); // ANSI underline
            formatted += 3;
        }
        else if (strncmp(formatted, "/fP", 3) == 0)
        {
            fputs("\033[0m", outputFile); // ANSI reset to normal
            formatted += 3;
        }
        else if (strncmp(formatted, "//", 2) == 0)
        {
            fputc('/', outputFile); // Output forward-slash
            formatted += 2;
        }
        else
        {
            fputc(*formatted, outputFile);
            formatted++;
        }
    }
}

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        printf("Improper number of arguments\nUsage: %s <file>\n", argv[0]);
        return 0;
    }

    char *inputFileName = argv[1];
    FILE *inputFile = fopen(inputFileName, "r");
    FILE *outputFile = NULL;
    char line[1024];
    int isSectionHeader = 0;
    int lineNumber = 0;
    // char* lastLine;
    if (inputFile == NULL)
    {
        printf("File doesn't exist\n");
        return 0;
    }
    char date[256];
    while (fgets(line, sizeof(line), inputFile) != NULL)
    {
        lineNumber++;

        if (lineNumber == 1 && (line[0] != '.' || line[1] != 'T' || line[2] != 'H'))
        {
            printf("Improper formatting on line %d\n", lineNumber);
            exit(0);
        }
        if (line[0] == '#')
        {
            continue;
        }
        else if (strncmp(line, ".TH", 3) == 0)
        {
            char command[256], section[256];
            if (sscanf(line, ".TH %s %s %s", command, section, date) == 3)
            {
                int year, month, day;
                if (sscanf(date, "%d-%d-%d", &year, &month, &day) != 3)
                {
                    printf("Improper formatting on line %d\n", lineNumber);
                    exit(0);
                }
                if (atoi(section) < 1 || atoi(section) > 9)
                {
                    printf("Improper formatting on line %d\n", lineNumber);
                    exit(0);
                }
                char outputFileName[512];
                sprintf(outputFileName, "%s.%s", command, section);
                outputFile = fopen(outputFileName, "w");
                if (outputFile == NULL)
                {
                    perror("Error creating output file");
                    return 0;
                }
                // first line
                // example(1) ----total 80 lines----- example(1)
                int padding = 80 - (strlen(command) + strlen(section) + 2) * 2;
                fprintf(outputFile, "%s(%s)%*s%s(%s)\n", command, section, padding, "", command, section);
            }
            else
            {
                printf("Improper formatting on line %d\n", lineNumber);
                exit(0);
            }
        }
        else if (strncmp(line, ".SH", 3) == 0)
        {
            isSectionHeader = 1;

            for (int i = 0; line[i]; i++)
            {
                line[i] = toupper(line[i]);
            }
            char sectionHeader[256];
            removeNewlines(line + 4);
            sprintf(sectionHeader, "\n\033[1m%s\033[0m\n", line + 4);
            fprintf(outputFile, "%s", sectionHeader);
        }
        else
        {
            if (isSectionHeader)
            {
                isSectionHeader = 0;
                // fprintf(outputFile,"%s","\n");
            }
            fprintf(outputFile, "%s", "       ");
            convertFormattingMarks(line, outputFile);
        }
    } // 80 characters in the line total
    // center the date in this line
    //

    int padding = 40 - strlen(date) / 2;
    fprintf(outputFile, "%*s%s%s%*s%s\n", padding, "", date, "", padding, "", "");

    if (inputFile)
    {
        fclose(inputFile);
    }
    if (outputFile)
    {
        fclose(outputFile);
    }
    return 0;
}