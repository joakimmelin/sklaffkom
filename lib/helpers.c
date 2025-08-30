#include <ctype.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <time.h>
#include "sklaff.h"
#include "ext_globals.h"


/* Two Little helpers to avoid extra '(') 2025-08-26 PL */
void clear_prompt(int num) 
{
    int x;
    output("\r");
    for (x = 0; x < num; x++)
        output(" ");
    output("\r");
}

void clear_prompt_cols(int cols)
{
    output_ansi_fmt("\r\033[0K", "\r");
        if (!Ansi_output) {
        int i;
        for (i = 0; i < cols; i++)
            output(" ");
        output("\r");
    }
}
