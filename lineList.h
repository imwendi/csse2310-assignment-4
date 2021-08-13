#ifndef LINELIST_H
#define LINELIST_H

#include <stdio.h>
#include <stdbool.h>

/* Struct used to store a list of lines.
 *
 * Contains an array of strings and an integer equal to the
 * number of strings in the string array.
 *
 * Used for functions which operate on string arrays where it is useful to
 * also know the number of strings in the array.
 * (i.e. the exact/prefix/anywhere search functions in searchMethods.c)
 *
 * (Adapted from the WordList struct in my A1 submission)
 */
typedef struct {
    /* Array of lines stored in the LineList */
    char **lines;
    /* Number of lines stored */
    int numLines;
} LineList;

LineList *init_line_list();
void add_to_lines(LineList *target, char *line);
void free_line_list(LineList *linesToFree);
char *read_file_line(FILE *doc, bool *isLineEmpty);
char *read_line_stdin(bool *isLineEmpty);
LineList *file_to_lines(FILE *doc);
void add_to_string(char **target, char *wordsToAdd);
int pattern_match_string(char *pattern, char *target);
char *get_printable(char *line);

#endif
