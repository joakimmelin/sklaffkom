/* sklaff_ui.h
 *
 * UI helpers, ANSI output and terminal presentation for SklaffKOM.
 */

#ifndef SKLAFF_UI_H
#define SKLAFF_UI_H

/* Conditional ANSI output helpers. */
#define ansi_output(...) \
    do { if (Ansi_output) output(__VA_ARGS__); } while (0) /* 2025-07-30 PL */

#define output_ansi_fallback(ansi_str, plain_str) \
    do { output(Ansi_output ? (ansi_str) : (plain_str)); } while (0) /* 2025-07-30 PL */

/* Prompt and terminal control. */
#define PROMPT      "-> " /* one whitespace at the end is a good idea PL 2025-07-30*/
#define ANSI_CLS    "\033[H\033[J"

/* ANSI colors */ 								/* 2025-07-30 PL */
#define RED         "\e[31m"
#define GREEN       "\e[32m"
#define YELLOW      "\e[33m"
#define BLUE        "\e[34m"
#define PURPLE      "\e[35m"
#define CYAN        "\e[36m"
#define WHITE       "\e[37m"

#define BR_WHITE    "\e[97m"
#define BR_RED      "\e[91m"
#define BR_GREEN    "\e[92m"
#define BR_YELLOW   "\e[93m"
#define BR_BLUE     "\e[94m"
#define BR_PURPLE   "\e[95m"
#define BR_CYAN     "\e[96m"

#define DOT         "\e[0m" /* Reset */
#define RESET       "\e[0m"

/* lib/ui.c */
int output_ansi_fmt(const char *ansi_fmt, const char *plain_fmt, ...); /* 2025-07-30 PL */
int output_ansi_raw_fmt(const char *ansi_fmt, const char *plain_fmt, ...); /* 2026-08-13 PL */
void clear_prompt(int num); /* Little helper to avoid extra '(') 2025-08-26 PL */
void clear_prompt_cols(int cols); /* 2025-08-30 PL */
void clear_screen(void); /* More sophisticated cls to use outside of commands.c */
int output_body_line(const char *line, const char *col); /* ANSI support fort text body in articles 2025-09-24 PL */

#endif /* SKLAFF_UI_H */
