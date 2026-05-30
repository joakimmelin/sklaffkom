#include <stdio.h>
#include <ctype.h>
#include <strings.h>

#include "sklaff.h"

#ifdef LINUX
#include <bsd/string.h>
#endif

#define BBSLINK_TITLE_WIDTH 32

static void
trim_bbslink(char *s)
{
    char *p = s;
    char *e;

    while (*p && isspace((unsigned char)*p))
        p++;

    if (p != s)
        memmove(s, p, strlen(p) + 1);

    e = s + strlen(s);
    while (e > s && isspace((unsigned char)e[-1]))
        *--e = '\0';
}

static int
parse_bbslink_line(char *line, char **code, char **title)
{
    char *p;

    trim_bbslink(line);

    if (*line == '\0' || *line == '#')
        return 0;

    p = strchr(line, ':');
    if (!p)
        return 0;

    *p++ = '\0';

    trim_bbslink(line);
    trim_bbslink(p);

    if (*line == '\0' || *p == '\0')
        return 0;

    *code = line;
    *title = p;
    return 1;
}

int
find_bbslink_game(const char *wanted, char *out, size_t outsz)
{
    FILE *fp;
    char line[256];

    fp = fopen(BBSLINK_INTRO, "r");
    if (!fp)
        return 0;

    while (fgets(line, sizeof(line), fp)) {
        char *code, *title;

        if (!parse_bbslink_line(line, &code, &title))
            continue;

        if (strcasecmp(wanted, code) == 0) {
            strlcpy(out, code, outsz);
            fclose(fp);
            return 1;
        }
    }

    fclose(fp);
    return 0;
}

void
show_bbslink_games(void)
{
    FILE *fp;
    char line[256];

    output("\n" MSG_BBSLINK01 "\n");
    output(MSG_BBSLINK02 "\n");
    output(MSG_BBSLINK03 "\n\n");

    fp = fopen(BBSLINK_INTRO, "r");
    if (!fp) {
        output("Kunde inte läsa spellistan (%s)\n\n", BBSLINK_INTRO);
        return;
    }

    output("%-*s  %s\n", BBSLINK_TITLE_WIDTH, "Titel", "Spelkod");
    output("%-*s  %s\n", BBSLINK_TITLE_WIDTH, "=====", "========");

    while (fgets(line, sizeof(line), fp)) {
        char *code, *title;
        char short_title[BBSLINK_TITLE_WIDTH + 1];

        if (!parse_bbslink_line(line, &code, &title))
            continue;

        strlcpy(short_title, title, sizeof(short_title));

        if (strlen(title) > BBSLINK_TITLE_WIDTH) {
            short_title[BBSLINK_TITLE_WIDTH - 3] = '.';
            short_title[BBSLINK_TITLE_WIDTH - 2] = '.';
            short_title[BBSLINK_TITLE_WIDTH - 1] = '.';
            short_title[BBSLINK_TITLE_WIDTH] = '\0';
        }

        output("%-*s  %s\n", BBSLINK_TITLE_WIDTH, short_title, code);
    }

    fclose(fp);
    output("\n");
}
