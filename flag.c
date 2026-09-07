/* flag.c */

/*
 *   SklaffKOM, a simple conference system for UNIX.
 *
 *   Copyright (C) 1993-1994  Torbj|rn B}}th, Peter Forsberg, Peter Lindberg,
 *                            Odd Petersson, Carl Sundbom
 *
 *   Program dedicated to the memory of Staffan Bergstr|m.
 *
 *   For questions about this program, mail sklaff@sklaffkom.se
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2, or (at your option)
 *   any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "sklaff.h"
#include "ext_globals.h"

/*
 * set_flags - setup flags at logintime
 * args: pointer to flagbuffer (flags)
 */

void
set_flags(char *flags)
{
    char *p, *i;

    if (strlen(flags)) {
        p = strstr(flags, "shout");
        if (p) {
            i = strchr(p, '=');
            i++;
            Shout = atoi(i);
        } else
            Shout = 1;

        p = strstr(flags, "say");
        if (p) {
            i = strchr(p, '=');
            i++;
            Say = atoi(i);
        } else
            Say = 1;

        p = strstr(flags, "present");
        if (p) {
            i = strchr(p, '=');
            i++;
            Present = atoi(i);
        } else
            Present = 1;

        p = strstr(flags, "ibm");
        if (p) {
            i = strchr(p, '=');
            i++;
            Ibm = atoi(i);
        } else
            Ibm = 0;

        p = strstr(flags, "iso8859");
        if (p) {
            i = strchr(p, '=');
            i++;
            Iso8859 = atoi(i);
        } else
            Iso8859 = 0;

        p = strstr(flags, "mac");
        if (p) {
            i = strchr(p, '=');
            i++;
            Mac = atoi(i);
        } else
            Mac = 0;

        p = strstr(flags, "subject_change");
        if (p) {
            i = strchr(p, '=');
            i++;
            Subject_change = atoi(i);
        } else
            Subject_change = 1;

        p = strstr(flags, "end_default");
        if (p) {
            i = strchr(p, '=');
            i++;
            End_default = atoi(i);
        } else
            End_default = 0;

        p = strstr(flags, "space");
        if (p) {
            i = strchr(p, '=');
            i++;
            Space = atoi(i);
        } else
            Space = 0;

        p = strstr(flags, "copy");
        if (p) {
            i = strchr(p, '=');
            i++;
            Copy = atoi(i);
        } else
            Copy = 1;

        p = strstr(flags, "author");
        if (p) {
            i = strchr(p, '=');
            i++;
            Author = atoi(i);
        } else
            Author = 0;

        p = strstr(flags, "date");
        if (p) {
            i = strchr(p, '=');
            i++;
            Date = atoi(i);
        } else
            Date = 0;

        p = strstr(flags, "beep");
        if (p) {
            i = strchr(p, '=');
            i++;
            Beep = atoi(i);
        } else
            Beep = 1;

        p = strstr(flags, "clear");
        if (p) {
            i = strchr(p, '=');
            i++;
            Clear = atoi(i);
        } else
            Clear = 0;

        p = strstr(flags, "header");
        if (p) {
            i = strchr(p, '=');
            i++;
            Header = atoi(i);
        } else
            Header = 0; /* default off */

        p = strstr(flags, "presbeep");
        if (p) {
            i = strchr(p, '=');
            i++;
            Presbeep = atoi(i);
        } else
            Presbeep = 0;

        p = strstr(flags, "strip");
        if (p) {
            i = strchr(p, '=');
            i++;
            Special = atoi(i);
        } else
            Special = 0;

        p = strstr(flags, "oldwho");
        if (p) {
            i = strchr(p, '=');
            i++;
            Old_who = atoi(i);
        } else
            Old_who = 0;
        
        p = strstr(flags, "ansi");
        if (p) {
            i = strchr(p, '=');
            i++;
            Ansi_output = atoi(i);
        } else
            Ansi_output = 0;  /* default off */
        
        p = strstr(flags, "utf8");
        if (p) {
            i = strchr(p, '=');
            i++;
            Utf8 = atoi(i);
        } else
            Utf8 = 0;  /* default off */
        
        p = strstr(flags, "force_sf7");
        if (p) {
            i = strchr(p, '=');
            i++;
            Force_sf7 = atoi(i);
         } else
            Force_sf7 = 0;
        
        p = strstr(flags, "alternate_intro");
        if (p) {
            i = strchr(p, '=');
            i++;
            Alternate_intro = atoi(i);
        } else
            Alternate_intro = 0;  /* default off */

        p = strstr(flags, "rookie_mode");
        if (p) {
            i = strchr(p, '=');
            i++;
            Rookie_mode = atoi(i);
        } else
            Rookie_mode = 1;  /* default on */
        
        p = strstr(flags, "start_mailbox");
        if (p) {
            i = strchr(p, '=');
            i++;
            Start_mailbox = atoi(i);
        } else
            Start_mailbox = 0; /* default off */
        
        p = strstr(flags, "ibol_logout");
        if (p) {
            i = strchr(p, '=');
            i++;
            Ibol_logout = atoi(i);
        } else {
#if IBOL_LOGOUT_COUNT > 0
            Ibol_logout = 1;  /* default on */
#else
            Ibol_logout = 0;  /* default off */
#endif
        }

    } else {
        Shout = 1;
        Say = 1;
        Present = 1;
        Ibm = 0;
        Iso8859 = 0;
        Mac = 0;
        Subject_change = 1;
        End_default = 0;
        Space = 0;
        Copy = 1;
        Author = 0;
        Date = 0;
        Beep = 1;
        Clear = 0;
        Header = 0;
        Special = 0;
        Presbeep = 0;
        Old_who = 0;
        Ansi_output = 0;
        Utf8 = 0;
        Force_sf7 = 0;
        Alternate_intro = 0;
        Rookie_mode = 1;
        Start_mailbox = 0;
#if IBOL_LOGOUT_COUNT > 0
        Ibol_logout = 1;
#else
        Ibol_logout = 0;
#endif
    }
}

/*
 * save_flags - save the current global flag values to the user's sklaffrc
 * ret: success (0) or failure (-1)
 */

int
save_flags(void)
{
    struct SKLAFFRC *rc;
    int n;
    int ret;

    rc = read_sklaffrc(Uid);
    if (rc == NULL)
        return -1;

    n = snprintf(rc->flags, sizeof(rc->flags),
        "say = %d\n"
        "shout = %d\n"
        "present = %d\n"
        "ibm = %d\n"
        "iso8859 = %d\n"
        "mac = %d\n"
        "subject_change = %d\n"
        "end_default = %d\n"
        "space = %d\n"
        "copy = %d\n"
        "author = %d\n"
        "date = %d\n"
        "beep = %d\n"
        "clear = %d\n"
        "header = %d\n"
        "strip = %d\n"
        "presbeep = %d\n"
        "oldwho = %d\n"
        "ansi = %d\n"
        "utf8 = %d\n"
        "force_sf7 = %d\n"
        "alternate_intro = %d\n"
        "rookie_mode = %d\n"
        "start_mailbox = %d\n"
        "ibol_logout = %d\n",
        Say,
        Shout,
        Present,
        Ibm,
        Iso8859,
        Mac,
        Subject_change,
        End_default,
        Space,
        Copy,
        Author,
        Date,
        Beep,
        Clear,
        Header,
        Special,
        Presbeep,
        Old_who,
        Ansi_output,
        Utf8,
        Force_sf7,
        Alternate_intro,
        Rookie_mode,
        Start_mailbox,
        Ibol_logout);

    if (n < 0 || (size_t)n >= sizeof(rc->flags)) {
        debuglog("save_flags: flags buffer too small", 1);
        free(rc);
        return -1;
    }

    ret = write_sklaffrc(Uid, rc);

    /*
     * write_sklaffrc() currently frees rc on failure, but not on success.
     * Only free it here after a successful write.
     */
    if (ret == 0)
        free(rc);

    return ret;
}

/*
 * check_flag - check for value of flag
 * args: flagbuffer (flags), flag to check (flag)
 * ret: value of flag or failure (-1)
 */

int
check_flag(char *flags, char *flag)
{
    char *p, *i;

    p = strstr(flags, flag);
    if (p) {
        i = strchr(p, '=');
        i++;
        return atoi(i);
    }
    return -1;
}

/*
 * turn_flag - set flag on or off
 * args: new value of flag (mode), flagname (flag)
 * ret: success (0) or failure (-1)
 */

int
turn_flag(int mode, char *flag)
{
    int i;
    LINE flags[24], outline;

    strcpy(flags[0], MSG_FLAG0);
    strcpy(flags[1], MSG_FLAG1);
    strcpy(flags[2], MSG_FLAG2);
    strcpy(flags[3], MSG_FLAG3);
    strcpy(flags[4], MSG_FLAG4);
    strcpy(flags[5], MSG_FLAG5);
    strcpy(flags[6], MSG_FLAG6);
    strcpy(flags[7], MSG_FLAG7);
    strcpy(flags[8], MSG_FLAG8);
    strcpy(flags[9], MSG_FLAG9);
    strcpy(flags[10], MSG_FLAG10);
    strcpy(flags[11], MSG_FLAG11);
    strcpy(flags[12], MSG_FLAG12);
    strcpy(flags[13], MSG_FLAG13);
    strcpy(flags[14], MSG_FLAG14);
    strcpy(flags[15], MSG_FLAG15);
    strcpy(flags[16], MSG_FLAG16);
    strcpy(flags[17], MSG_FLAG17);
    strcpy(flags[18], MSG_FLAG18);
    strcpy(flags[19], MSG_FLAG19);
    strcpy(flags[20], MSG_FLAG20);
    strcpy(flags[21], MSG_FLAG21);
    strcpy(flags[22], MSG_FLAG22);
    strcpy(flags[23], MSG_FLAG23);
    if (!flag || (*flag == '\0')) {
        output("\n%s\n\n", MSG_NOFLAG);
        return 0;
    }
    down_string(flag);
    i = strlen(flag);
    if ((strstr(flags[0], flag) == flags[0]) && (i >= MSG_FLAG0N)) {
        if (mode && (Iso8859 || Mac || Utf8)) {
            if (Iso8859) {
                output("\n%s\n\n", MSG_ISOWARN);
                return 0;
	    } else if (Utf8) {
 	        output("\n%s\n\n", MSG_UTFWARN);
                return 0;
            } else if (Mac) {
                output("\n%s\n\n", MSG_MACWARN);
                return 0;
            }
        }
        Ibm = mode;
        if (mode)
            Force_sf7 = 0;
        strcpy(outline, MSG_FLAG0F);
    } else if ((strstr(flags[1], flag) == flags[1]) && (i >= MSG_FLAG1N)) {
        if (mode && (Ibm || Mac || Utf8)) {
            if (Ibm) {
                output("\n%s\n\n", MSG_PCWARN);
                return 0;
	    } else if (Utf8) {
                output("\n%s\n\n", MSG_UTFWARN);
                return 0;
            } else if (Mac) {
                output("\n%s\n\n", MSG_MACWARN);
                return 0;
            }
        }
        Iso8859 = mode;
        if (mode)
            Force_sf7 = 0;
        strcpy(outline, MSG_FLAG1F);
    } else if ((strstr(flags[2], flag) == flags[2]) && (i >= MSG_FLAG2N)) {
        if (mode && (Ibm || Iso8859 || Utf8)) {
            if (Ibm) {
                output("\n%s\n\n", MSG_PCWARN);
                return 0;
            } else if (Utf8) {
                output("\n%s\n\n", MSG_UTFWARN);
                return 0;
            } else if (Iso8859) {
                output("\n%s\n\n", MSG_ISOWARN);
                return 0;
            }
        }
        Mac = mode;
        if (mode)
            Force_sf7 = 0;
        strcpy(outline, MSG_FLAG2F);
    } else if ((strstr(flags[3], flag) == flags[3]) && (i > MSG_FLAG3N)) {
        Present = mode;
        strcpy(outline, MSG_FLAG3F);
    } else if ((strstr(flags[4], flag) == flags[4]) && (i >= MSG_FLAG4N)) {
        Shout = mode;
        strcpy(outline, MSG_FLAG4F);
    } else if ((strstr(flags[5], flag) == flags[5]) && (i >= MSG_FLAG5N)) {
        End_default = mode;
        strcpy(outline, MSG_FLAG5F);
    } else if ((strstr(flags[6], flag) == flags[6]) && (i >= MSG_FLAG6N)) {
        Say = mode;
        strcpy(outline, MSG_FLAG6F);
    } else if ((strstr(flags[7], flag) == flags[7]) && (i >= MSG_FLAG7N)) {
        Subject_change = mode;
        strcpy(outline, MSG_FLAG7F);
    } else if ((strstr(flags[8], flag) == flags[8]) && (i >= MSG_FLAG8N)) {
        Space = mode;
        strcpy(outline, MSG_FLAG8F);
    } else if ((strstr(flags[9], flag) == flags[9]) && (i >= MSG_FLAG9N)) {
        Copy = mode;
        strcpy(outline, MSG_FLAG9F);
    } else if ((strstr(flags[10], flag) == flags[10]) && (i >= MSG_FLAG10N)) {
        Author = mode;
        strcpy(outline, MSG_FLAG10F);
    } else if ((strstr(flags[11], flag) == flags[11]) && (i >= MSG_FLAG11N)) {
        Date = mode;
        strcpy(outline, MSG_FLAG11F);
    } else if ((strstr(flags[12], flag) == flags[12]) && (i >= MSG_FLAG12N)) {
        Beep = mode;
        strcpy(outline, MSG_FLAG12F);
    } else if ((strstr(flags[13], flag) == flags[13]) && (i >= MSG_FLAG13N)) {
        Clear = mode;
        strcpy(outline, MSG_FLAG13F);
    } else if ((strstr(flags[14], flag) == flags[14]) && (i >= MSG_FLAG14N)) {
        Header = mode;
        strcpy(outline, MSG_FLAG14F);
    } else if ((strstr(flags[15], flag) == flags[15]) && (i >= MSG_FLAG15N)) {
        Special = mode;
        strcpy(outline, MSG_FLAG15F);
    } else if ((strstr(flags[16], flag) == flags[16]) && (i >= MSG_FLAG16N)) {
        Presbeep = mode;
        strcpy(outline, MSG_FLAG16F);
    } else if ((strstr(flags[17], flag) == flags[17]) && (i >= MSG_FLAG17N)) {
        Old_who = mode;
        strcpy(outline, MSG_FLAG17F);
    } else if ((strstr(flags[18], flag) == flags[18]) && (i >= MSG_FLAG18N)) {
        Ansi_output = mode;
        strcpy(outline, MSG_FLAG18F);
    } else if ((strstr(flags[19], flag) == flags[19]) && (i >= MSG_FLAG19N)) {
        if (mode && (Ibm || Iso8859 || Mac)) {
            if (Ibm) {
                output("\n%s\n\n", MSG_PCWARN);
                return 0;
            } else if (Mac) {
                output("\n%s\n\n", MSG_MACWARN);
                return 0;
            } else if (Iso8859) {
                output("\n%s\n\n", MSG_ISOWARN);
                return 0;
            }
        }
        Utf8 = mode;
        if (mode)
            Force_sf7 = 0;
        strcpy(outline, MSG_FLAG19F);

    } else if ((strstr(flags[20], flag) == flags[20]) && (i >= MSG_FLAG20N)) {
        Alternate_intro = mode;	
        strcpy(outline, MSG_FLAG20F);
    } else if ((strstr(flags[21], flag) == flags[21]) && (i >= MSG_FLAG21N)) {
        Rookie_mode = mode;
        strcpy(outline, MSG_FLAG21F);
    } else if ((strstr(flags[22], flag) == flags[22]) && (i >= MSG_FLAG22N)) {
        Start_mailbox = mode;
        strcpy(outline, MSG_FLAG22F);
    } else if ((strstr(flags[23], flag) == flags[23]) && (i >= MSG_FLAG23N)) {
        Ibol_logout = mode;
        strcpy(outline, MSG_FLAG23F);
    } else {
        output("\n%s\n\n", MSG_BADFLAG);
        return 0;
    }

    if (save_flags() == -1)
        return -1;
    output("\n%s %s ", MSG_FLAG, outline);
    if (mode) {
        output("%s\n\n", MSG_FLON);
    } else {
        output("%s\n\n", MSG_FLOFF);
    }
    return 0;
}
