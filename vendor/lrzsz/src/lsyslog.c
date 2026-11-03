/*
  lsyslog.c - wrapper for the syslog function
  Copyright (C) 1997 Uwe Ohse

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2, or (at your option)
  any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
  02111-1307, USA.

*/
#include "config.h"
#include "zglobal.h"
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void
lsyslog(int prio, const char *format, ...)
{
    static char *username=NULL;
    /* i'd really hate this function to fail! 21 is enough for \0 plus
     * uint64_t uids, which most likely will never exist... */
    static char uid_string[21]="";
    char s[1024];
    char scrub[1024];
    static int init_done=0;
    va_list ap;
    if (!enable_syslog)
            return;
    if (!init_done) {
            uid_t uid;
            struct passwd *pwd;
            init_done=1;
            uid=getuid();
            pwd=getpwuid(uid);
            if (pwd && pwd->pw_name && *pwd->pw_name) {
                    username=strdup(pwd->pw_name);
            }
            if (!username) {
		    snprintf(uid_string,sizeof(uid_string),"#%lu",(unsigned long) uid);
                    username = uid_string;
            }
    }

    va_start(ap, format);
    vsnprintf(s, sizeof(s), format, ap);
    va_end(ap);
    (void) sanitize_bytes(scrub, sizeof(scrub), s);
    syslog(prio,"[%s] %s",username, scrub);
}
