/* commands.c */

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
#include <pwd.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <math.h>   /* for floor() in Swatch Internet Time  2025-09-10 PL */
#include <ctype.h>  /* isspace */
/* a bunch of new includes below for zork  2025-08-xx PL*/
#include <limits.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>

/*
 * cmd_sendbatch - send all texts in database format, zipped
 * args: user arguments (args)
 * ret: ok (0) or error (-1)
 */

int
cmd_sendbatch(char *args)
{
    LINE tmpdir, cwd, cmdline, source, answer;
    LONG_LINE dest, cname;
    char tmpline[512];
    int fd, fd2;
    long num;
    char *oldbuf, *buf, *nbuf;
    struct CONFS_ENTRY cse;
    struct UR_STACK *start, *ptr, *saved;
    sigset_t sigmask, oldsigmask;

    /* QWK handling variables */

#define LOCAL
#define MAX_USR  512
#define MAX_CNF  1024

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>

#define CFG_FILE "sq.cfg"
#define CTL_FILE "control.dat"
#define MSG2_FILE "messages.dat"
#define USR_FILE "user"
#define CNF_FILE "conf"
#define INF_FILE "info"
#define COPY "Produced by SQ version 0.30, Copyright 1994 by Daniel Gr|njord"
#define MAX_NAME 128

    FILE *fp, *cof, *mep, *msf;
    struct stat stbuf;
    LINE package;
    char sbuf[256], pdate[256], *ptr2, ch = 0;
    int usrc, cnfc, ssc, c, l, usr_find, to_find, msg_count = 0, blocks;
    FILE *nep;
    char filler[1], msint[4];
    int offset;
    long login, msg_size;
    long rem;
    struct {
        int uid;
        char *name;
    } ulst[MAX_USR];
    struct {
        int cid;
        char *name;
    } clst[MAX_CNF];
    struct {
        long number;
        int uid;
        time_t wtime;
        long comnum;
        int comcnf;
        int comuid;
        int lines;
        char user[MAX_NAME];
        char to[MAX_NAME];
        char subject[MAX_NAME];
    } txt;
    struct {
        char bbs[64];
        char adress[64];
        char phone[128];
        char sysop[128];
        char user[64];
    } info;
    struct tm tim;

    Change_prompt = 1;
    Change_msg = 1;
    set_avail(Uid, 1);

    output("\n%s", MSG_CRPACK);
    output("\n");
    fflush(stdout);

    snprintf(tmpdir, sizeof(tmpdir), "/tmp/%d", getpid());
    mkdir(tmpdir, 0777);
    if (getcwd(cwd, LINE_LEN) == NULL) {
    perror("getcwd"); /* Modified by PL 2025-07-25 to keep linux compiler happy */
    return 0;
    }
    if (chdir(tmpdir) == -1) {
    perror("chdir"); /* Modified by PL 2025-07-25 to keep linux compiler happy */
    return 0;
    }

    /* copy userlist */

    snprintf(dest, sizeof(dest), "%s/user", tmpdir);
    copy_file(USER_FILE, dest);

    /* copy infofile */

    snprintf(dest, sizeof(dest), "%s/info", tmpdir);
    if ((fd = open_file(dest, OPEN_CREATE | OPEN_QUIET)) == -1) {
        set_avail(Uid, 0);
        return -1;
    }
    nbuf = (char *) malloc(100000);
    memset(nbuf, 0, 100000);
    snprintf(nbuf, 100000, "%s\n%s\n%s\n%s\n%s\n",
        SKLAFF_ID, SKLAFF_LOC, SKLAFF_NUM,
        SKLAFF_SYSOP, user_name(Uid, cname));
    critical();
    if (write_file(fd, nbuf) == -1) {
        set_avail(Uid, 0);
        return -1;
    }
    if (close_file(fd) == -1) {
        set_avail(Uid, 0);
        return -1;
    }
    non_critical();

    /* copy custom conffile */

    strcpy(source, Home);
    strcat(source, CONFS_FILE);

    if ((fd = open_file(source, 0)) == -1) {
        set_avail(Uid, 0);
        return -1;
    }
    if ((buf = read_file(fd)) == NULL) {
        set_avail(Uid, 0);
        return -1;
    }
    oldbuf = buf;
    if (close_file(fd) == -1) {
        set_avail(Uid, 0);
        return -1;
    }
    snprintf(dest, sizeof(dest), "%s/conf", tmpdir);
    if ((fd2 = open_file(dest, OPEN_CREATE | OPEN_QUIET)) == -1) {
        set_avail(Uid, 0);
        return -1;
    }
    nbuf = (char *) malloc(100000);
    memset(nbuf, 0, 100000);

    while ((buf = get_confs_entry(buf, &cse)) != NULL) {
        free_confs_entry(&cse);
        conf_name(cse.num, cname);
        snprintf(tmpline, sizeof(tmpline), "%d:%s\n", cse.num, cname);
        strcat(nbuf, tmpline);
    }

    critical();
    if (write_file(fd2, nbuf) == -1) {
        set_avail(Uid, 0);
        return -1;
    }
    if (close_file(fd2) == -1) {
        set_avail(Uid, 0);
        return -1;
    }
    non_critical();

    /* the texts */

    start = NULL;
    buf = oldbuf;
    while ((buf = get_confs_entry(buf, &cse)) != NULL) {
        free_confs_entry(&cse);
        if (next_text(cse.num)) {
            output(".");
            fflush(stdout);
            snprintf(dest, sizeof(dest), "%s/%d", tmpdir, cse.num);
            if ((fd = open_file(dest, OPEN_CREATE | OPEN_QUIET)) == -1) {
                set_avail(Uid, 0);
                return -1;
            }
            lseek(fd, 0L, 0);
            while ((num = next_text(cse.num)) != 0) {
                if (cse.num > 0)
                    snprintf(cname, sizeof(cname), "%s/%d/%ld", SKLAFF_DB, cse.num, num);
                else
                    snprintf(cname, sizeof(cname), "%s/%ld", Mbox, num);
                if ((fd2 = open_file(cname, 0)) != -1) {
                    if ((nbuf = read_file(fd2)) == NULL) {
                        set_avail(Uid, 0);
                        return -1;
                    }
                    close_file(fd2);
                    if (write(fd, nbuf, strlen(nbuf)) == -1) {
                        set_avail(Uid, 0);
                        return -1;
                    }
                    if (write(fd, "\004\n", 2) == -1) {
                        set_avail(Uid, 0);
                        return -1;
                    }
                    free(nbuf);
                }
                if (start) {
                    saved = ptr;
                }
                ptr = (struct UR_STACK *) malloc(sizeof(struct UR_STACK));
                if (!start) {
                    start = ptr;
                } else {
                    saved->next = ptr;
                }
                ptr->num = num;
                ptr->conf = cse.num;
                ptr->next = NULL;
                mark_as_read(num, cse.num);
            }
        }
        close_file(fd);
    }
    free(oldbuf);

    /* unread all texts again */

    ptr = start;
    while (ptr) {
        mark_as_unread(ptr->num, ptr->conf);
        ptr = ptr->next;
    }

    /* QWK package handling by Daniel Gr|njord */

    /* Test if QWK-file already exists */

    snprintf(package, sizeof(package), "%d", getpid());
    strcpy(sbuf, package);
    strcat(sbuf, ".qwk");
    if ((fp = fopen(sbuf, "r")) != NULL) {
        output("%s already exists\n", sbuf);
        exit(1);
    }
    /* Get package date and time */

    if (stat(INF_FILE, &stbuf) == -1) {
        output("Unable to determine package time and date - package not found?\n");
        set_avail(Uid, 0);
        return -1;
    }
    tim = *localtime(&stbuf.st_mtime);  /* Need to set TZ? */
    snprintf(pdate, sizeof(pdate), "%.2d-%.2d-%.2d,%.2d:%.2d:%.2d",
        tim.tm_mon + 1, tim.tm_mday, tim.tm_year + 1900,
        tim.tm_hour, tim.tm_min, tim.tm_sec);

    /* Parse info file */

    if ((fp = fopen(INF_FILE, "rt")) == NULL) {
        output("Can't open %s\n", INF_FILE);
        set_avail(Uid, 0);
        return -1;
    }
    memset(info.bbs, 0, 64);
    if (fgets(info.bbs, 63, fp) == NULL) {
    perror("fgets info.bbs"); 		/* Linux compiler complained here and below (untested due to other errors - work in progress / TODO 2025-07-25, PL */
    fclose(fp);
    return 0;
    }
    memset(info.adress, 0, 64);
    if (fgets(info.adress, 63, fp) == NULL) {
    perror("fgets info.adress");	/* Modified on 2025-07-25, PL */
    fclose(fp);
    return 0;
    }
    memset(info.phone, 0, 128);
    if (fgets(info.phone, 127, fp) == NULL) {
    perror("fgets info.phone");		/* Modified on 2025-07-25, PL */
    fclose(fp);
    return 0;
    }
    memset(info.sysop, 0, 128);
    if (fgets(info.sysop, 127, fp) == NULL) {
    perror("fgets info.sysop");		/* Modified on 2025-07-25, PL */
    fclose(fp);
    return 0;
    }
    memset(info.user, 0, 64);
    if (fgets(info.user, 63, fp) == NULL) {
    perror("fgets info.user");		/* Modified on 2025-07-25, PL */
    fclose(fp);
    return 0;
    }   
    cnvnat(info.bbs, ch);
    cnvnat(info.adress, ch);
    cnvnat(info.phone, ch);
    cnvnat(info.sysop, ch);
    cnvnat(info.user, ch);
    fclose(fp);

    /* Parse user file */

    if ((fp = fopen(USR_FILE, "rt")) == NULL) {
        output("Can't open %s\n", USR_FILE);
        set_avail(Uid, 0);
        return -1;
    }
    usrc = 0;
    while (fgets(sbuf, 127, fp) != NULL) {
        if ((ulst[usrc].name = (char *) malloc(MAX_NAME)) == NULL) {
            output("Out of memory\n");
            exit(1);
        }
        ssc = sscanf(sbuf, "%d:%ld:%[^#\n]", &ulst[usrc].uid, &login,
            ulst[usrc].name);
        if (ssc != 3 || ssc == EOF) {
            output("Error while parsing %s\n", USR_FILE);
            set_avail(Uid, 0);
            return -1;
        }
        usrc++;
    }
    fclose(fp);

    /* Parse conf file */

    if ((fp = fopen(CNF_FILE, "rt")) == NULL) {
        output("Can't open %s\n", CNF_FILE);
        set_avail(Uid, 0);
        return -1;
    }
    cnfc = 0;
    while (fgets(sbuf, 127, fp) != NULL) {
        if ((clst[cnfc].name = (char *) malloc(MAX_NAME)) == NULL) {
            output("Out of memory\n");
            exit(1);
        }
        ssc = sscanf(sbuf, "%d:%[^#\n]", &clst[cnfc].cid, clst[cnfc].name);
        if (ssc != 2 || ssc == EOF) {
            output("Error while parsing %s\n", CNF_FILE);
            set_avail(Uid, 0);
            return -1;
        }
        cnvnat(clst[cnfc].name, ch);
        cnfc++;
    }
    fclose(fp);

    /* Create MESSAGES.DAT file */

    if ((mep = fopen(MSG2_FILE, "wt+")) == NULL) {
        output("Can't create %s\n", MSG2_FILE);
        set_avail(Uid, 0);
        return -1;
    }
    strcpy(sbuf, COPY);
    cnvnat(sbuf, ch);
    fprintf(mep, "%-128s", sbuf);

    for (c = 0; c < cnfc; c++) {
        snprintf(sbuf, sizeof(sbuf), "%d", clst[c].cid);
        if ((msf = fopen(sbuf, "rt")) != NULL) {
            if (clst[c].cid < 1000)
                snprintf(sbuf, sizeof(sbuf), "%03d.ndx", clst[c].cid);
            else
                snprintf(sbuf, sizeof(sbuf), "%04d.ndx", clst[c].cid);
            if ((nep = fopen(sbuf, "wt+")) == NULL) {
                output("Can't create %s\n", sbuf);
                set_avail(Uid, 0);
                return -1;
            }
            while (fgets(sbuf, 127, msf) != NULL) {
                /* Convert and write offset record */
                offset = (int) (ftell(mep) / 128L) + 1;
                int2ms(offset, msint);
                fwrite(msint, 4, 1, nep);       /* Rec pointer */
                filler[0] = 0;
                fwrite(&filler, 1, 1, nep);     /* Unused */
                ssc = sscanf(sbuf, "%ld:%d:%lld:%ld:%d:%d:%d",
                    &txt.number, &txt.uid,
                    (long long *) &txt.wtime, &txt.comnum, &txt.comcnf, &txt.comuid,
                    &txt.lines);
                if (ssc != 7 || ssc == EOF) {
                    output("Error while parsing %s\n", sbuf);
                    set_avail(Uid, 0);
                    return -1;
                }
                msg_count++;
                if (fgets(txt.subject, 127, msf) == NULL) {
                perror("fgets txt.subject"); /* 2025-07-25, PL (compiler was unhappy) */
                return 0;
                }
                txt.subject[strlen(txt.subject) - 1] = '\0';

                /* Get name of writer */

                if (txt.uid > 0) {
                    for (usr_find = 0; usr_find < usrc; usr_find++)
                        if (ulst[usr_find].uid == txt.uid) {
                            strcpy(txt.user, ulst[usr_find].name);
                            break;
                        }
                } else
                    while (fgets(sbuf, 255, msf) != NULL) {
                        txt.lines--;    /* Decrease lines... */
                        if (strncmp("From: ", sbuf, 6) == 0) {  /* Writer found */
                            ptr2 = &sbuf[6];
                            sbuf[strlen(sbuf) - 1] = '\0';
                            strcpy(txt.user, ptr2);
                        }
                        if (sbuf[0] == 0x0a)    /* End of header found */
                            break;
                    }

                /* Get name of the commented */

                if (txt.comnum > 0) {
                    if (txt.comuid > 0) {
                        for (to_find = 0; to_find < usrc; to_find++)
                            if (ulst[to_find].uid == txt.comuid) {
                                strcpy(txt.to, ulst[to_find].name);
                                break;
                            }
                    } else
                        strcpy(txt.to, "Unknown");
                } else
                    strcpy(txt.to, "All");

                /* Convert time */

                tim = *localtime(&txt.wtime);   /* Need to set TZ? */

                /* Write to MESSAGES.DAT file */

                if (clst[c].cid == 0)
                    fprintf(mep, "+");  /* Private */
                else
                    fprintf(mep, " ");  /* Public */
                fprintf(mep, "%-7ld", txt.number);      /* Text number */
                fprintf(mep, "%.2d-%.2d-%.2d", tim.tm_mon + 1, tim.tm_mday,
                    tim.tm_year);
                fprintf(mep, "%.2d:%.2d", tim.tm_hour, tim.tm_min);
                cnvnat(txt.to, ch);
                cnvnat(txt.user, ch);
                cnvnat(txt.subject, ch);
                fprintf(mep, "%-25.25s", txt.to);
                fprintf(mep, "%-25.25s", txt.user);
                fprintf(mep, "%-25.25s", txt.subject);
                fprintf(mep, "            ");   /* Password */
                if (txt.comcnf == 0)    /* If 0 com in cur cnf? */
                    fprintf(mep, "%-8ld", txt.comnum);  /* Comment in this cnf? */
                else
                    fprintf(mep, "0       ");   /* No comment or other cnf */
                rem = ftell(mep);       /* Remember position */
                fprintf(mep, "FILLER"); /* BLOCKS WRITTEN LATER */
                fprintf(mep, "\341");   /* Active text */
                fwrite(&clst[c].cid, 2, 1, mep);        /* Converence number */
                fprintf(mep, "   ");    /* Msg number & no tagline */

                /* Write text and filler */

                msg_size = 0L;
                for (l = 0; l < txt.lines; l++) {
                    if (fgets(sbuf, 127, msf) == NULL) { /* Get text */
    		    perror("fgets sbuf"); /* error checking 2025-07-25, PL */
  		    return 0;
		    }     
                    cnvnat(sbuf, ch);
                    sbuf[strlen(sbuf) - 1] = (char) 227;        /* Replace nl */
                    msg_size += fprintf(mep, "%s", sbuf);       /* Write text */
                }
                blocks = (int) (msg_size / 128L + (msg_size % 128L > 0)) + 1;
                l = (int) ((blocks - 1) * 128 - msg_size);
                while (l-- > 0)
                    fprintf(mep, " ");

                /* Write number of blocks */

                fseek(mep, rem, SEEK_SET);      /* Restore position */
                fprintf(mep, "%-6d", blocks);   /* Write blocks */
                fseek(mep, 0L, SEEK_END);

                /* Skip comments and EOT mark */

                while (fgets(sbuf, 127, msf) != NULL) {
                    if (sbuf[0] == 4)
                        break;
                }
            }
            fclose(msf);
            fclose(nep);
        }
    }
    fclose(mep);

    /* Create CONTROL.DAT file */
	printf("\n");
	printf("[DEBUG] Skapar control.dat...");
    if ((cof = fopen(CTL_FILE, "wt")) == NULL) {
        output("Can't create %s\n", CTL_FILE);
        set_avail(Uid, 0);
        return -1;
    }
    /* Write info to CONTROL.DAT file */

    fprintf(cof, "%s", info.bbs);
    fprintf(cof, "%s", info.adress);
    fprintf(cof, "%s", info.phone);
    fprintf(cof, "%s", info.sysop);
    fprintf(cof, "4711,SQ\n");  /* Mail door */
    fprintf(cof, "%s\n", pdate);/* Package date */
    fprintf(cof, "%s", info.user);
    fprintf(cof, "\n");         /* Blank */
    fprintf(cof, "0\n");        /* Zero */
    fprintf(cof, "%d\n", msg_count);    /* Messages in package */
    fprintf(cof, "%d\n", cnfc - 1);     /* Conferences minus 1 */

    /* Temporary solution due to bug in TC 2.01 */

    for (c = 0; c < cnfc; c++) {
        snprintf(sbuf, sizeof(sbuf), "%d\n%.13s\n", clst[c].cid, clst[c].name);
        fputs(sbuf, cof);
    }

    fclose(cof);

    sigemptyset(&sigmask);
    sigaddset(&sigmask, SIGNAL_NEW_TEXT);
    sigaddset(&sigmask, SIGNAL_NEW_MSG);
    sigprocmask(SIG_BLOCK, &sigmask, &oldsigmask);

    signal(SIGNAL_NEW_TEXT, SIG_IGN);
    signal(SIGNAL_NEW_MSG, SIG_IGN);

    /* Pack QWK-file */

    strcpy(sbuf, ZIPPRGM);
    strcat(sbuf, " ");
    strcat(sbuf, package);
    strcat(sbuf, ".qwk control.dat messages.dat *.ndx > /dev/null");
    if (system(sbuf) == -1) {
    perror("system"); /* PL 2025-07-25 */
    }

    /* Cleanup */

    unlink(USR_FILE);
    unlink(CNF_FILE);
    unlink(INF_FILE);
    unlink(CTL_FILE);
    unlink(MSG2_FILE);
    for (c = 0; c < cnfc; c++) {
        snprintf(sbuf, sizeof(sbuf), "%d", clst[c].cid);
        unlink(sbuf);
        if (clst[c].cid < 1000)
            snprintf(sbuf, sizeof(sbuf), "%03d.ndx", clst[c].cid);
        else
            snprintf(sbuf, sizeof(sbuf), "%04d.ndx", clst[c].cid);
        unlink(sbuf);
    }

    output("%s\n", MSG_FINPACK);

    snprintf(cmdline, sizeof(cmdline), "%d.qwk", getpid());
    if (!fork()) {
        sig_reset();
        execl(DOWNLOADPRGM, DOWNLOADPRGM, DLOPT1, DLOPT2, DLOPT3, cmdline, NULL); /* TODO : POSSIBLY BROKEN */
    } else {
        wait(NULL);
    }

    snprintf(cmdline, sizeof(cmdline), "%d.qwk", getpid());
    unlink(cmdline);
    if (chdir(cwd) == -1) {
    perror("chdir"); /* 2025-07-25 PL */
    }
    rmdir(tmpdir);

    signal(SIGNAL_NEW_TEXT, baffo);
    signal(SIGNAL_NEW_MSG, newmsg);
    sigprocmask(SIG_UNBLOCK, &oldsigmask, NULL);

    do {
        output("\n%s", MSG_URQ);
        input("", answer, 4, 0, 0, 0);
        down_string(answer);
    } while ((answer[0] != MSG_YESANSWER) && (answer[0] != 'n'));

    if (*answer && (answer[0] == MSG_YESANSWER)) {
        ptr = start;
        while (ptr) {
            mark_as_read(ptr->num, ptr->conf);
            ptr = ptr->next;
        }
    }
    ptr = start;
    while (ptr) {
        saved = ptr;
        ptr = ptr->next;
        free(saved);
    }
    output("\n");
    set_avail(Uid, 0);
    return 0;
}

/*
 * safe_str - returns a safe printable string
 * We need this for file descriptions to show
 * correctly on FreeBSD. PL 2025-07-26
 */ 
const char *
safe_str(const char *s)
{
    return (s && *s) ? s : "";
}

/*
 * rc_set_scalar - sets a single value in sklaffrc (2025-08-10 PL)
 * args: 
 * ret:
 */
int rc_set_scalar(int uid, const char *key, const char *value)
{
    char path[512], tmp[512], *buf = NULL, *newbuf = NULL;
    size_t blen = 0, klen, vlen;
    int fd = -1, rc = -1;

    if (!key || !*key || !value) return -1;
    klen = strlen(key);
    vlen = strlen(value);

    if (user_dir(uid, path) == NULL) return -1;          
    strncat(path, "/sklaffrc", sizeof(path)-strlen(path)-1); /* Bug fixed September 2025 PL */
    snprintf(tmp, sizeof(tmp), "%s.tmp", path);

    /* read existing */
    fd = open_file(path, OPEN_QUIET);
    if (fd != -1) {
        buf = read_file(fd);
        close_file(fd);
    }
    if (!buf) {
        buf = strdup("");                                
        if (!buf) return -1;
    }
    blen = strlen(buf);

    /* find heading at start-of-line: "![Key]" */
    char heading[128];
    snprintf(heading, sizeof(heading), "![%s]", key);

    char *p = buf;
    char *hit = NULL;
    while ((p = strstr(p, heading))) {
        if (p == buf || p[-1] == '\n') { hit = p; break; }
        p += 2; 
    }

    if (hit) {
        /* replace the single value line following the heading */
        char *val_start = strchr(hit, '\n');            
        if (!val_start) val_start = buf + blen; else val_start++;
        char *val_end = val_start;
   
        while (*val_end && *val_end != '\n') val_end++;
   
        size_t prefix_len = (size_t)(val_start - buf);
        size_t suffix_len = blen - (size_t)(val_end - buf);
        size_t need = prefix_len + vlen + 1 /* \n */ + suffix_len + 1;

        newbuf = malloc(need);
        if (!newbuf) goto out;

        memcpy(newbuf, buf, prefix_len);
        memcpy(newbuf + prefix_len, value, vlen);
        newbuf[prefix_len + vlen] = '\n';
        memcpy(newbuf + prefix_len + vlen + 1, val_end, suffix_len);
        newbuf[need - 1] = '\0';
    } else {
        /* append at end; ensure file ends with a newline */
        int need_nl = (blen > 0 && buf[blen-1] != '\n') ? 1 : 0;
        size_t add_len = (need_nl ? 1 : 0) + 2 + 1 + klen + 1 + 1 + vlen + 1;
        /* "\n" + "![" + key + "]" + "\n" + value + "\n" */
        size_t need = blen + add_len + 1;

        newbuf = malloc(need);
        if (!newbuf) goto out;

        memcpy(newbuf, buf, blen);
        size_t o = blen;
        if (need_nl) newbuf[o++] = '\n';
        newbuf[o++] = '!';
        newbuf[o++] = '[';
        memcpy(newbuf + o, key, klen); o += klen;
        newbuf[o++] = ']';
        newbuf[o++] = '\n';
        memcpy(newbuf + o, value, vlen); o += vlen;
        newbuf[o++] = '\n';
        newbuf[o] = '\0';
    }
    {
        int tfd = create_file(tmp);
        if (tfd == -1) goto out;
        critical();
        if (write_file(tfd, newbuf) == -1) { close_file(tfd); non_critical(); newbuf = NULL; goto out; }
        close_file(tfd);
	newbuf = NULL; 
        if (rename(tmp, path) == -1) { /* POSIX-safe; Linux/FreeBSD */
            unlink(tmp);
            non_critical();
            goto out;
        }
        non_critical();
    }
    rc = 0;

out:
    if (buf) free(buf);
    if (newbuf) free(newbuf);
    return rc;
}



/*
 * cmd_help - list all commands
 * args: user arguments (args)
 * ret: ok (0) or error (-1)
 */

int
cmd_help(char *args)
{
    int i;

    i = 0;
    output("\n%s\n", MSG_LISTCOM);
    while (Par_ent[i].func[0] != '\0') {
        if (output("%-20s %s\n", Par_ent[i].cmd, Par_ent[i].help) == -1)
            break;
        i++;
    }
    output("\n");
    return 0;
}

/*
 * cmd_where - show current conf and number of unread texts
 * args: user arguments (args)
 * ret: ok (0) or error (-1)
 */

int
cmd_where(char *args)
{
    LINE confname;
    int left;

    conf_name(Current_conf, confname);
    //output("\n%s %s\n", MSG_WHERE2, confname);
    output_ansi_fmt("\n%s "BR_RED "%s\n"DOT, "\n%s %s\n", MSG_WHERE2, confname);
    left = num_unread(Uid, Current_conf, last_text(Current_conf, Uid));
    if (left == 0)
        output("%s\n\n", MSG_NOUNREAD);
    else if (left == 1)
        output_ansi_fmt("%s\n\n","%s\n\n", MSG_ONEUNREAD);
    else
        output_ansi_fmt(CYAN"%d"DOT" %s\n\n", "%d %s\n\n", left, MSG_UNREADTEXTS);
    return 0;
}

/*
 * cmd_change_conf - change conference
 * args: user arguments (args)
 * ret: ok (0) or error (-1)
 */

int
cmd_change_conf(char *args)
{
    char *newname;
    int conf;
    LINE answer, arg2;

    if (*args) {
        newname = expand_name(args, CONF, 0, NULL);

        if (newname) {
            conf = conf_num(newname);
            if (!member_of(Uid, conf)) {
                output("\n%s %s? ", MSG_JOIN, newname);
                input(MSG_YES, answer, 4, 0, 0, 0);
                down_string(answer);
                if (*answer && (answer[0] == MSG_YESANSWER)) {
                    strcpy(arg2, newname);
                    cmd_subscribe(arg2);
                } else {
                    output("\n");
                    return 0;
                }
            }
            if (member_of(Uid, conf)) {
                set_conf(conf);
                cmd_where(args);
                clear_comment();
                Current_text = last_text(Current_conf, Uid);
                Last_text = Current_text;
            }
        }
    } else
        output("\n%s\n\n", MSG_NOCONFNAME);
    return 0;
}

/*
 * cmd_display_time - display current time
 * args: user arguments (args)
 * ret: ok (0) or error (-1)
 *
 * Upgraded on 2025-08-15 by PL:
 * - Friendly Swedish timestamp and login duration
 * - Rotating nerdy @beat messages (Swatch Internet Time)
 * - Random tip from SKLAFFDIR "/etc/strings"
 */
 
 
 /*
 * HERREJÖSSES VILKEN RÖRA NEDAN - DET ÄR INTE FÄRDIGT ÄN, SKALL FIXAS! :D
 */
int
cmd_display_time(char *args)
{
    char *tip = NULL;
    char linebuf[512];
    FILE *fp = NULL;
    int linecount = 0, chosen = 0, current = 0;
    int beatstyle;
    long at;
    time_t now;
    struct tm ltm = {0};       /* <- zero-init to silence -Wmaybe-uninitialized */
    struct tm *gmt = NULL;

    now = time(0);
    get_wallclock_localtime(&now, &ltm);   /* fills ltm using /etc/localtime */
    gmt = gmtime(&now);                   /* for @beat */
    at = active_time(Uid);

        /* === Swatch Internet Time (@beats) and Sweden local-day percent === */
    /* fixed on 2025-09-10, PL: correct label + compute Sweden % from localtime */

    int beats = 0;
    double se_pct = 0.0;

    if (gmt) {
        /* BMT = UTC+1, no DST */
        long bmt_secs =
            (((gmt->tm_hour + 1) % 24) * 3600L) +
            (gmt->tm_min * 60L) +
            gmt->tm_sec;

        /* 1 beat = 86.4 seconds */
        double bmt_beats = bmt_secs / 86.4;
        beats = (int)floor(bmt_beats);
        if (beats >= 1000) beats = 0; /* clamp edge */
    } else {
        beats = 0;
    }

    /* Sweden (Europe/Stockholm) local day progress */
    {
        long se_secs = ltm.tm_hour * 3600L + ltm.tm_min * 60L + ltm.tm_sec;
        se_pct = (se_secs / 86400.0) * 100.0;
    }

	/* The actual output starts here, finally */ 
	output("\n%s %02d:%02d %s %d %s %d,\n%s ",
           MSG_DISPTIME,
           ltm.tm_hour, ltm.tm_min,
           MSG_IT,
           ltm.tm_mday, month_name(ltm.tm_mon),
           1900 + ltm.tm_year,
           MSG_DISPTIME2);

    if (at == 1)
        output("%s", MSG_ONEMIN);
    else
        output("%ld %s", at, MSG_MINUTES);

    output("\n");

    beatstyle = rand() % 5;
    switch (beatstyle) {
        case 0:
        //  output("(@%03d) = Swatch Internet Time — %.1f%% through the UTC day\n", tbeat, tbeat / 10.0);
      //    output("(@%03d) = Swatch Internet Time — %.1f%% through the UTC day\n", tbeat, tbeat / 10.0);
      //      output("Tiden på internet är (@%03d), och i Sverige har %.1f%% av dygnet hunnit gå.\n", beats, se_pct);
	   output("%s(@%03d), %s %.1f%% %s\n",
           MSG_BEATS1,   // "Tiden på internet {r"
           beats,
           MSG_BEATS2,   // "och i Sverige har"
           se_pct,
           MSG_BEATS3);  // "av dygnet hunnit g}.  
	break;
        case 1:
     //       output("System Clock (Internet Time): @%03d. Transmission window optimal.\n", tbeat);
     //       output("Tiden på internet är (@%03d), och i Sverige har %.1f%% av dygnet hunnit gå.\n", beats, se_pct);
   output("%s(@%03d), %s %.1f%% %s\n",
           MSG_BEATS1,   // "Tiden på internet {r"
           beats,
           MSG_BEATS2,   // "och i Sverige har"
           se_pct,
           MSG_BEATS3);  // "av dygnet hunnit g}.
            break;
        case 2:
     //       output("UTC Sync: @%03d beats — %.1f%% of the day has elapsed.\n", tbeat, tbeat / 10.0);
     //       output("Tiden på internet är (@%03d), och i Sverige har %.1f%% av dygnet hunnit gå.\n", beats, se_pct);
     output("%s(@%03d), %s %.1f%% %s\n",
           MSG_BEATS1,   // "Tiden på internet {r"
           beats,
           MSG_BEATS2,   // "och i Sverige har"
           se_pct,
           MSG_BEATS3);  // "av dygnet hunnit g}.
            break;
        case 3:
       //     output("(@%03d)  // Internet Time: one beat = 86.4 seconds (UTC)\n", tbeat);
      //        output("Tiden på internet är (@%03d), och i Sverige har %.1f%% av dygnet hunnit gå.\n", beats, se_pct);
   output("%s(@%03d), %s %.1f%% %s\n",
           MSG_BEATS1,   // "Tiden på internet {r"
           beats,
           MSG_BEATS2,   // "och i Sverige har"
           se_pct,
           MSG_BEATS3);  // "av dygnet hunnit g}.

            break;
        case 4:
       //      output("Tiden på internet är (@%03d), och i Sverige har %.1f%% av dygnet hunnit gå.\n", beats, se_pct);
      //      output(">>> Timecode @%03d // Swatch Internet Time engaged\n", tbeat);
   output("%s(@%03d), %s %.1f%% %s\n",
           MSG_BEATS1,   // "Tiden på internet {r"
           beats,
           MSG_BEATS2,   // "och i Sverige har"
           se_pct,
           MSG_BEATS3);  // "av dygnet hunnit g}.
            break;
    }

    fp = fopen(SKLAFFDIR "/etc/strings", "r");
    if (fp) {
        while (fgets(linebuf, sizeof(linebuf), fp)) {
            if (linebuf[0] == '#' || linebuf[0] == '\n' || linebuf[0] == '\r')
                continue;
            linecount++;
        }

        if (linecount > 0) {
            rewind(fp);
            chosen = rand() % linecount;
            current = 0;
            while (fgets(linebuf, sizeof(linebuf), fp)) {
                if (linebuf[0] == '#' || linebuf[0] == '\n' || linebuf[0] == '\r')
                    continue;
                if (current == chosen) {
                    chomp(linebuf);
                    tip = strdup(linebuf);
                    break;
                }
                current++;
            }
        }
        fclose(fp);
    }

    if (tip) {
        output("\n%s\n", tip);
        free(tip);
    }

    output("\n");
    return 0;
}


/*
int
cmd_display_time(char *args)
{
    LINE tmp_str;
    long at;
    time_t now;
    struct tm *gmt;
    int tbeat;

    now = time(0);
    at = active_time(Uid);
*/
    /* Time beats */
/*
    gmt = gmtime(&now);
    tbeat = ((gmt->tm_hour + 1) % 24) * 3600 / 86.4 + gmt->tm_min * 60 / 86.4 + gmt->tm_sec / 86.4;

    output("\n%s (@%03d)\n%s ", time_string(now, tmp_str, 1), tbeat, MSG_ACTIVE);
    if (at == 1)
        output("%s", MSG_ONEMIN);
    else
        output("%ld %s", at, MSG_MINUTES);
    output("\n\n");

    return 0;
}

*/
/*
 * cmd_end_session - end session for user
 * args: user arguments (args)
 * ret: ok (0) or error (-1)
 */

int
cmd_end_session(char *args)
{
    if (args && *args) {
        output("\n%s\n\n", MSG_NOARG);
    } else {
        End_sklaff = 1;
    }
    return 0;
}

/*
 * cmd_restart - restarts session
 * args: user arguments (args)
 * ret: ok (0)
 */

int
cmd_restart(char *args)
{
    if (args && *args) {
        output("\n%s\n\n", MSG_NOARG);
    } else {
        debuglog("Restart command issued by user", 1);  /* modified on 2025-09-25, PL */
        End_sklaff = 1;
        restart = 1;
    }
    return 0;
}

/*
 * cmd_list_users - list all users
 * args: user arguments (args)
 * ret: ok (0) or error (-1)
 */

int
cmd_list_users(char *args)
{
    list_user(1, 0, 0);
    return 0;
}

/*
 * cmd_list_last - list last logined users
 * args: user arguments (args)
 * ret: ok (0) or error (-1)
 */

int
cmd_list_last(char *args)
{
    list_user(2, 0, 0);
    return 0;
}

static int
active_entry_cmp(const void *a, const void *b)
{
    const struct ACTIVE_ENTRY *ae1 = a;
    const struct ACTIVE_ENTRY *ae2 = b;

    int r = idle_time(ae1->user) - idle_time(ae2->user);

    if (r == 0)
        r = active_time(ae2->user) - active_time(ae1->user);

    return (r);
}

/*
 * cmd_who - list users online
 * args: user arguments (args)
 * ret: ok (0) or error (-1)
 */

int
cmd_who(char *args)
{
    long itime;
    LINE tid, idle, namn;
    char *buf, *oldbuf;
    int nactive, nidle, i;


    struct ACTIVE_ENTRY
    *ae, ea;

    return (list_who(WHO_INTERNAL));

    if ((ActiveFD = open_file(ACTIVE_FILE, 0)) == -1) {
        return -1;
    }
    if ((buf = read_file(ActiveFD)) == NULL) {
        return -1;
    }
    oldbuf = buf;

    if (close_file(ActiveFD) == -1) {
        return -1;
    }
    ActiveFD = -1;

    /* Old vilka-lista char *ptr;
     * 
     * output("\n%-25s %7s  %-12s %-8s  %s\n\n", MSG_NAME, MSG_TIME, MSG_WHEN,
     * MSG_ACT, MSG_FROM); while ((buf = get_active_entry(buf, &ae))) { if
     * (ae.avail) ptr = MSG_YES; else ptr = ""; if(output("%-25s %7ld  %-12s
     * %-6s  %s\n", user_name(ae.user, namn), active_time(ae.user),
     * time_string(ae.login_time, tid, 0), ptr, ae.from) == -1) break; }
     * output("\n"); */


    if (Old_who) {

        /* New vilka-lista (98-04-15, OR) */

        output("\n%-25s %-11s %4s %4s   %s\n\n", MSG_NAME, MSG_WHEN,
            MSG_TIME, MSG_ACT, MSG_FROM);
        while ((buf = get_active_entry(buf, &ea))) {
            if (!ea.avail) {
                user_name(ea.user, namn);
                namn[25] = 0;
            } else {
                namn[0] = '(';
                user_name(ea.user, namn + 1);
                namn[24] = 0;
                strcat(namn, ")");
            }
            itime = idle_time(ea.user);
            if (itime == 0)
                strcpy(idle, "    ");
            else
                snprintf(idle, sizeof(idle), "%4ld", itime);
            if (output("%-25s %-11s %4ld %s  %s\n",
                    namn,
                    time_string(ea.login_time, tid, 0),
                    active_time(ea.user),
                    idle,
                    ea.from) == -1)
                break;
        }
        output("\n");
    } else {
        nidle = 0;
        nactive = 0;
        while ((buf = get_active_entry(buf, &ea)))
            nactive++;

        ae = (struct ACTIVE_ENTRY *) malloc(nactive * sizeof(struct ACTIVE_ENTRY));

        buf = oldbuf;

        i = 0;
        while ((buf = get_active_entry(buf, &(ae[i]))))
            i++;

        qsort(ae, nactive, sizeof(struct ACTIVE_ENTRY), active_entry_cmp);

        output("\n%-25s %-11s %4s %4s   %s\n\n", MSG_NAME, MSG_WHEN,
            MSG_TIME, MSG_ACT, MSG_FROM);

        for (i = 0; i < nactive; i++) {
            if (!ae[i].avail) {
                user_name(ae[i].user, namn);
                namn[25] = 0;
            } else {
                namn[0] = '(';
                user_name(ae[i].user, namn + 1);
                namn[24] = 0;
                strcat(namn, ")");
            }
            itime = idle_time(ae[i].user);
            if (itime < IDLE_LIMIT)
                strcpy(idle, "    ");
            else {
                snprintf(idle, sizeof(idle), "%4ld", itime);
                nidle++;
            }
            if (output("%-25s %-11s %4ld %s  %s\n",
                    namn,
                    time_string(ae[i].login_time, tid, 0),
                    active_time(ae[i].user),
                    idle,
                    ae[i].from) == -1)
                break;
        }
        output("\n");
        free(ae);

        /* Added 99-01-28/ OR */

        output("Totalt %d inloggade, varav %d aktiva.\n", nactive, nactive - nidle);
        output("\n");
    }

    free(oldbuf);
    return 0;
}

/*
 * cmd_list_confs - list all conferences
 * args: user arguments (args)
 * ret: ok (0) or error (-1)
 */

int
cmd_list_confs(char *args)
{
    return (list_confs(Uid, 1));
}

/*
 * cmd_list_news - list unread texts for user
 * args: user arguments (args)
 * ret: ok (0) or error (-1)
 */

int
cmd_list_news(char *args)
{
    int uid;
    char *username;

    if (strlen(args) == 0) {
        uid = Uid;
    } else {
        username = expand_name(args, USER, 0, NULL);
        if (!username) {
            return 0;
        }
        uid = user_uid(username);
    }
    return (list_news(uid));
}


/*
 * cmd_list_rights - list rights for conference
 * args: user arguments (args)
 * ret: ok (0) or error (-1)
 */

int
cmd_list_rights(char *args)
{
    int count, x, confnum;
    struct UEL *ue_list, *top;
    struct UEN *namelist, *topname;
    struct USER_LIST *ul, *utop;
    char *confname;
    LINE username;
    struct CONF_ENTRY *ce;

    if (*args && args) {
        confname = expand_name(args, CONF, 0, NULL);
        if (!confname) {
            return 0;
        } else {
            confnum = conf_num(confname);
        }
    } else {
        confnum = Current_conf;
    }

    if (!confnum) {
        output("\n%s\n\n", MSG_RIGHTMAIL);
        return 0;
    }
    ul = get_confrc_struct(confnum);
    ce = get_conf_struct(confnum);

    if ((ce->type == OPEN_CONF) || (ce->type == NEWS_CONF)) {
        output("\n%s %s:\n\n", MSG_NORIGHTS, ce->name);
    } else {
        output("\n%s %s:\n\n", MSG_YESRIGHTS, ce->name);
        user_name(ce->creator, username);
        output("%s%s\n", username, MSG_CREATOR);
    }

    utop = ul;
    ue_list = NULL;
    top = NULL;
    count = 0;
    while (ul) {
        if (ue_list) {
            ue_list->next = (struct UEL *) malloc
                (sizeof(struct UEL) + 1);
            if (ue_list->next == NULL) {
                sys_error("list_user", 1, "malloc");
                return -1;
            }
            ue_list = ue_list->next;
            user_name(ul->num, ue_list->ue.name);
            ue_list->next = NULL;
        } else {
            ue_list = (struct UEL *) malloc
                (sizeof(struct UEL) + 1);
            if (ue_list == NULL) {
                sys_error("list_user", 1, "malloc");
                return -1;
            }
            top = ue_list;
            user_name(ul->num, ue_list->ue.name);
            ue_list->next = NULL;
        }
        ul = ul->next;
        count++;
    }
    free_userlist(utop);
    namelist = sort_user(top, count);
    if (namelist) {
        topname = namelist;
        for (x = 0; x < count; x++) {
            output("%s\n", namelist->name);
            namelist++;
        }
        free(topname);
    }
    output("\n");
    return 0;
}


/*
 * cmd_create_conf - create new conference
 * args: user arguments (args)
 * ret: ok (0) or error (-1)
 */

int
cmd_create_conf(char *args)
{
    int fd, fd2, conf_type, confnum, i;
    char *buf, *oldbuf, *nbuf, *tmp;
    LINE confname, interact, newname;
    LONG_LINE tmpbuf;
    struct CONF_ENTRY ce;

    conf_type = OPEN_CONF;
    strcpy(confname, args);

    output("\n");
    if (*confname == '\0') {
        output(MSG_CNAMEASK);
        input("", confname, LINE_LEN, 0, 0, 0);
    }
    ltrim(confname);

    if (expand_name(confname, CONF, 1, NULL)) {
        output("%s\n\n", MSG_ERRCNAME);
        return 0;
    }
    if (*confname != '\0') {

        output(MSG_CONFTPROMPT);
        input(MSG_CONFDEFAULT, interact, LINE_LEN, 0, 0, 0);
        down_string(interact);
        if (*interact) {
            if (*interact == MSG_CONFCLOSED)
                conf_type = 1;
            else if (*interact == MSG_CONFSECRET)
                conf_type = 2;
            else if (*interact == MSG_CONFNEWS)
                conf_type = 3;
            else
                conf_type = 0;
        } else
            conf_type = 0;

        if ((fd = open_file(CONF_FILE, 0)) == -1) {
            return -1;
        }
        if ((buf = read_file(fd)) == NULL) {
            return -1;
        }
        oldbuf = buf;
        ce.num = 0;

        for (;;) {
            confnum = ce.num;
            buf = get_conf_entry(buf, &ce);
            if (buf == NULL)
                break;
        }

        if (conf_type == 3)
            ce.life = EXP_DEF_NEWS;
        else
            ce.life = EXP_DEF;
        ce.num = confnum + 1;
        ce.last_text = 0;
        ce.creator = Uid;
        ce.type = conf_type;
        ce.time = time(0);
        ce.comconf = 0;
        strcpy(ce.name, confname);
        tmp = stringify_conf_struct(&ce, tmpbuf);

        i = strlen(oldbuf) + strlen(tmp) + 1;
        nbuf = (char *) malloc(i);
        memset(nbuf, 0, i);

        strcpy(nbuf, oldbuf);
        strcat(nbuf, tmp);

        critical();
        if (write_file(fd, nbuf) == -1) {
            return -1;
        }
        if (close_file(fd) == -1) {
            return -1;
        }
        snprintf(newname, sizeof(newname), "%s/%d", SKLAFF_DB, ce.num);
        mkdir(newname, NEW_DIR_MODE);
        snprintf(newname, sizeof(newname), "%s/%d", FILE_DB, ce.num);
        mkdir(newname, NEW_DIR_MODE);
        snprintf(newname, sizeof(newname), "%s/%d", SKLAFF_DB, ce.num);
        strcat(newname, CONFRC_FILE);

        if ((fd2 = open_file(newname, OPEN_CREATE | OPEN_QUIET)) == -1) {
            return -1;
        }
        if (close_file(fd2) == -1) {
            return -1;
        }
        non_critical();

        free(oldbuf);
        output("\n%s %s %s\n", MSG_CONFNAME, confname, MSG_CREATED2);
        cmd_subscribe(confname);
    } else
        output("\n%s\n\n", MSG_NOCONFNAME);
    return 0;
}


/*
 * cmd_subscribe - subscribe to a conference
 * args: user arguments (args)
 * ret: ok (0) or error (-1)
 */

int
cmd_subscribe(char *args)
{
    LINE confname, userstr, confsname;
    char *exp_confname, *buf, *nbuf;
    int fd2, conf, right, i;
    long first;
    struct USER_LIST *ul;
    struct CONF_ENTRY *ce;

    Change_prompt = 1;
    if (args && *args) {
        strcpy(confname, args);
    } else {
        output("\n%s\n\n", MSG_NOCONFNAME);
        return 0;
    }
    exp_confname = expand_name(confname, UNSUBSCRIBED, 0, NULL);
    if (exp_confname) {
        conf = conf_num(exp_confname);
        if (!member_of(Uid, conf)) {
            ul = get_confrc_struct(conf);
            ce = get_conf_struct(conf);
            right = conf_right(ul, Uid, ce->type, ce->creator);
            free_userlist(ul);
            if (right == 0) {
                strcpy(confsname, Home);
                strcat(confsname, CONFS_FILE);
                if ((fd2 = open_file(confsname, 0)) == -1)
                    return -1;
                if ((buf = read_file(fd2)) == NULL)
                    return -1;
                i = strlen(buf) + LINE_LEN + 1;
                nbuf = (char *) malloc(i);
                if (!nbuf) {
                    sys_error("cmd_subscribe", 1, "malloc");
                    return -1;
                }
                memset(nbuf, 0, i);
                strcpy(nbuf, buf);
                free(buf);
                first = first_text(conf, Uid);
                if (first > 1) {
                    snprintf(userstr, sizeof(userstr), "%d:1-%ld\n", conf,
                        (first - 1));
                } else {
                    snprintf(userstr, sizeof(userstr), "%d:\n", conf);
                }
                strcat(nbuf, userstr);
                critical();
                if (write_file(fd2, nbuf) == -1)
                    return -1;
                if (close_file(fd2) == -1)
                    return -1;
                non_critical();
                output("\n%s %s.\n\n", MSG_SUBOK, exp_confname);
            } else
                output("\n%s\n\n", MSG_NOTALLOW);
        } else
            output("\n%s\n\n", MSG_ALREADYSUB);
    }
    return 0;
}

/*
 * cmd_unsubscribe - unsubscribe to a conference
 * args: user arguments (args)
 * ret: ok (0) or error (-1)
 */

int
cmd_unsubscribe(char *args)
{
    LINE confsname, confname;
    char *exp_confname, *buf, *nbuf, *oldbuf, *tmpbuf;
    int fd, conf, i;
    struct CONFS_ENTRY ce;

    if (args && *args) {
        strcpy(confname, args);
    } else {
        output("\n%s\n\n", MSG_NOCONFNAME);
        return 0;
    }
    exp_confname = expand_name(confname, SUBSCRIBED | ALSOERASED, 0, NULL);
    if (exp_confname) {
        conf = conf_num(exp_confname);
        if (conf) {
            if (member_of(Uid, conf)) {
                strcpy(confsname, Home);
                strcat(confsname, CONFS_FILE);

                if ((fd = open_file(confsname, 0)) == -1)
                    return -1;

                if ((buf = read_file(fd)) == NULL)
                    return -1;

                oldbuf = buf;

                i = strlen(buf) + 1;
                nbuf = (char *) malloc(i);
                if (!nbuf) {
                    sys_error("cmd_unsubscribe", 1, "malloc");
                    return -1;
                }
                memset(nbuf, 0, i);

                while (buf != NULL) {
                    buf = get_confs_entry(buf, &ce);
                    free_confs_entry(&ce);
                    if (ce.num == conf)
                        break;
                }

                if (ce.num == conf) {
                    tmpbuf = buf;

                    tmpbuf--;
                    while ((tmpbuf > oldbuf) && (*tmpbuf == '\n'))
                        tmpbuf--;

                    while ((tmpbuf > oldbuf) && (*tmpbuf != '\n'))
                        tmpbuf--;

                    if (tmpbuf > oldbuf)
                        tmpbuf++;
                    *tmpbuf = '\0';
                    strcpy(nbuf, oldbuf);
                    strcat(nbuf, buf);
                    critical();
                    if (write_file(fd, nbuf) == -1) {
                        return -1;
                    }
                }
                free(oldbuf);
                if (close_file(fd) == -1)
                    return -1;
                non_critical();
                output("\n%s %s.\n", MSG_USUBOK, exp_confname);
                if (conf == Current_conf) {
                    set_conf(0);
                    clear_comment();
                    cmd_where(args);
                } else
                    output("\n");
            } else
                output("\n%s\n\n", MSG_NOSUB);
        } else
            output("\n%s\n\n", MSG_NOUSUBMAIL);
    }
    return 0;
}

/*
 * cmd_add_rights - add rights for user in conference
 * args: user arguments (args)
 * ret: ok (0) or error (-1)
 */

int
cmd_add_rights(char *args)
{
    char *user, *buf, *nbuf;
    LINE confrcfile, newuid;
    int unum, fd, i;
    struct USER_LIST *saved, *ul;
    struct CONF_ENTRY *ce;

    if (Current_conf == 0) {
        output("\n%s\n\n", MSG_NOADDMAIL);
        return 0;
    }
    if (args && (*args != '\0')) {
        user = expand_name(args, USER, 0, NULL);
        if (user && (*user != '\0')) {
            unum = user_uid(user);
            ce = get_conf_struct(Current_conf);
            if (ce->creator == Uid) {
                ul = get_confrc_struct(Current_conf);
                saved = ul;
                snprintf(confrcfile, sizeof(confrcfile), "%s/%d%s", SKLAFF_DB,
                    Current_conf, CONFRC_FILE);
                if ((fd = open_file(confrcfile, 0)) == -1)
                    return -1;
                if ((buf = read_file(fd)) == NULL)
                    return -1;
                if (conf_right(ul, unum, ce->type, ce->creator) > 0) {
                    i = strlen(buf) + 20;
                    if ((nbuf = (char *) malloc(i)) == NULL)
                        return -1;
                    memset(nbuf, 0, i);
                    if ((ce->type > 0) && (ce->type < 3)) {
                        snprintf(newuid, sizeof(newuid), "%d\n", unum);
                        strcpy(nbuf, buf);
                        strcat(nbuf, newuid);
                    } else {
                        while (ul) {
                            if (ul->num != unum) {
                                snprintf(newuid, sizeof(newuid), "%d\n", ul->num);
                                strcat(nbuf, newuid);
                            }
                            ul = ul->next;
                        }
                    }
                    critical();
                    if (write_file(fd, nbuf) == -1)
                        return -1;
                    free(buf);
                    output("\n%s %s.\n\n", MSG_ADDRIGHTOK, user);
                } else
                    output("\n%s\n\n", MSG_ALREADYRIG);
                if (close_file(fd) == -1)
                    return -1;
                free_userlist(saved);
                non_critical();
            } else
                output("\n%s\n\n", MSG_RIGHTCREATE);
        }
    } else
        output("\n%s\n\n", MSG_NONAME);
    return 0;
}

/*
 * cmd_sub_rights - remove rights for user in conference
 * args: user arguments (args)
 * ret: ok (0) or error (-1)
 */

int
cmd_sub_rights(char *args)
{
    char *user, *buf, *nbuf;
    struct USER_LIST *saved;
    LINE confrcfile, newuid;
    int unum, fd, i;
    struct USER_LIST *ul;
    struct CONF_ENTRY *ce;

    if (Current_conf == 0) {
        output("\n%s\n\n", MSG_NOSUBMAIL);
        return 0;
    }
    if (args && (*args != '\0')) {
        user = expand_name(args, USER, 0, NULL);
        if (user && (*user != '\0')) {
            unum = user_uid(user);
            ce = get_conf_struct(Current_conf);
            if (ce->creator == Uid) {
                if (ce->creator != unum) {
                    ul = get_confrc_struct(Current_conf);
                    saved = ul;
                    snprintf(confrcfile, sizeof(confrcfile), "%s/%d%s", SKLAFF_DB,
                        Current_conf, CONFRC_FILE);
                    if ((fd = open_file(confrcfile, 0)) == -1)
                        return -1;
                    if ((buf = read_file(fd)) == NULL)
                        return -1;
                    if (conf_right(ul, unum, ce->type, ce->creator) == 0) {
                        if ((buf = read_file(fd)) == NULL)
                            return -1;
                        i = strlen(buf) + 20;
                        if ((nbuf = (char *) malloc(i)) == NULL)
                            return -1;
                        memset(nbuf, 0, i);
                        if ((ce->type > 0) && (ce->type < 3)) {
                            while (ul) {
                                if (ul->num != unum) {
                                    snprintf(newuid, sizeof(newuid), "%d\n", ul->num);
                                    strcat(nbuf, newuid);
                                }
                                ul = ul->next;
                            }
                        } else {
                            snprintf(newuid, sizeof(newuid), "%d\n", unum);
                            strcpy(nbuf, buf);
                            strcat(nbuf, newuid);
                        }
                        critical();
                        if (write_file(fd, nbuf) == -1)
                            return -1;
                        free(buf);
                        output("\n%s %s.\n\n", MSG_SUBRIGHTOK, user);
                    } else
                        output("\n%s\n\n", MSG_NORIG);
                    if (close_file(fd) == -1)
                        return -1;
                    free_userlist(saved);
                    non_critical();
                } else
                    output("\n%s\n\n", MSG_NOSUBSELF);
            } else
                output("\n%s\n\n", MSG_SUBCREATE);
        }
    } else
        output("\n%s\n\n", MSG_NONAME);
    return 0;
}

/*
 * cmd_read_text - read text in conference
 * args: user arguments (args)
 * ret: ok (0) or error (-1)
 */

int
cmd_read_text(char *args)
{
    long textnum;

    textnum = parse_text(args);
    if (textnum) {
        if (!display_text(Last_conf, textnum, 0, 0))
            Last_text = textnum;
    }
    Rot13 = 0;
    return 0;
}

/*
 * cmd_whole_text - read text with absolute date
 * args: user arguments (args)
 * ret: ok (0) or error (-1)
 */

int
cmd_whole_text(char *args)
{
    long textnum;

    if (!args || (*args == '\0'))
        sprintf(args, "%ld", Last_text);
    textnum = parse_text(args);
    if (textnum) {
        Cont = 1;
        if (!display_text(Last_conf, textnum, 0, 1))
            Last_text = textnum;
        Cont = 0;
    }
    return 0;
}

/*
 * cmd_next_comment - read next comment in conference
 * args: user arguments (args)
 * ret: ok (0) or error (-1)
 */

int
cmd_next_comment(char *args)
{
    long textnum;

    Last_conf = Current_conf;
    textnum = pop_comment();
    if (textnum == -1) {
        output("\n%s\n\n", MSG_NOMORECOM);
        return 0;
    }
    display_text(Current_conf, textnum, 1, 0);
    mark_as_read(textnum, Current_conf);
    push_read(Current_conf, textnum);
    Current_text = textnum;
    Last_text = textnum;
    return 0;
}

/*
 * cmd_next_text - read next text in conference
 * args: user arguments (args)
 * ret: ok (0) or error (-1)
 */

int
cmd_next_text(char *args)
{
    long textnum;

    Last_conf = Current_conf;
    clear_comment();
    if (Nexttext != -1)
        textnum = Nexttext;
    else
        textnum = next_text(Current_conf);
    Nexttext = -1;
    if (textnum == 0) {
        output("\n%s\n\n", MSG_NOTEXTLEFT);
        return 0;
    }
    display_text(Current_conf, textnum, 1, 0);
    mark_as_read(textnum, Current_conf);
    push_read(Current_conf, textnum);
    Current_text = textnum;
    Last_text = textnum;
    return 0;
}

/*
 * cmd_next_conf - enter next conference with unread texts
 * args: user arguments (args)
 * ret: ok (0) or error (-1)
 */

int
cmd_next_conf(char *args)
{
    int conf;

    if (Nextconf != -1)
        conf = Nextconf;
    else
        conf = more_conf();
    Nextconf = -1;
    if (conf == -1) {
        output("\n%s\n\n", MSG_NOCONFLEFT);
        return 0;
    }
    set_conf(conf);
    cmd_where(args);
    clear_comment();
    Current_text = last_text(Current_conf, Uid);
    Last_text = Current_text;

    return 0;
}

/*
 * cmd_jump_over - skip rest of branch
 * args: user arguments (args)
 * ret: ok (0) or error (-1)
 */

int
cmd_jump_over(char *args)
{
    long textnum;
    int count;

    count = 0;
    clear_comment();
    textnum = Last_text;
    while (textnum > 0) {
        count += mark_as_read(textnum, Current_conf);
        stack_text(textnum);
        textnum = pop_comment();
    }

    if (count == 1) {
        output("\n%s\n\n", MSG_JUMPONE);
    } else if (count == 0) {
        output("\n%s\n\n", MSG_JUMPNONE);
    } else {
        output("\n%d %s\n\n", count, MSG_JUMPTEXT);
    }

    return 0;
}

/*
 * cmd_jump_tree - skip tree
 * args: user arguments (args)
 * ret: ok (0) or error (-1)
 */

int
cmd_jump_tree(char *args)
{
    long textnum;
    int count;

    if (!Current_conf) {
        output("\n%s\n\n", MSG_NOTINMBOX);
        return 0;
    }
    count = 0;
    if (!Last_text) {
        output("\n%s\n\n", MSG_READONE);
        return 0;
    }
    clear_comment();
    textnum = tree_top(Last_text);
    while (textnum > 0) {
        count += mark_as_read(textnum, Current_conf);
        stack_text(textnum);
        textnum = pop_comment();
    }

    if (count == 1) {
        output("\n%s\n\n", MSG_JUMPONE);
    } else if (count == 0) {
        output("\n%s\n\n", MSG_JUMPNONE);
    } else {
        output("\n%d %s\n\n", count, MSG_JUMPTEXT);
    }
    return 0;
}

/*
 * cmd_goto_text - skip all texts under argument
 * args: user arguments (args)
 * ret: ok (0) or error (-1)
 */

int
cmd_goto_text(char *args)
{
    LINE fname;
    long textnum;
    int count;

    Change_prompt = 1;
    if (!Current_conf) {
        output("\n%s\n\n", MSG_NOTINMBOX);
        return 0;
    }
    count = 0;
    clear_comment();
    textnum = parse_text(args);
    snprintf(fname, sizeof(fname), "%s/%d/%ld", SKLAFF_DB, Current_conf, textnum);
    if (file_exists(fname) != -1) {
        while (textnum > 0) {
            count += mark_as_unread(textnum, Current_conf);
            stack_text(textnum);
            textnum = pop_comment();
        }
    } else {
        if (textnum)
            output("\n%s\n\n", MSG_ERRTNUM);
        return 0;
    }

    if (count == 1) {
        output("\n%s\n\n", MSG_URONE);
    } else if (count == 0) {
        output("\n%s\n\n", MSG_URNONE);
    } else {
        output("\n%d %s\n\n", count, MSG_URTEXT);
    }
    return 0;
}

/*
 * cmd_unread_tree - unreads text tree
 * args: user arguments (args)
 * ret: ok (0) or error (-1)
 */

int
cmd_unread_tree(char *args)
{
    long textnum;
    int count;
    LINE fn;

    if (!Current_conf) {
        output("\n%s\n\n", MSG_NOTINMBOX);
        return 0;
    }
    if (args && *args)
        textnum = atol(args);
    else
        textnum = 0L;
    if (!textnum && Last_text)
        textnum = Last_text;
    if (!textnum) {
        output("\n%s\n\n", MSG_ERRTNUM);
        return 0;
    }
    snprintf(fn, sizeof(fn), "%s/%d/%ld", SKLAFF_DB, Current_conf, textnum);
    if (file_exists(fn) == -1) {
        output("\n%s\n\n", MSG_ERRTNUM);
        return 0;
    }
    count = 0;
    Change_prompt = 1;
    clear_comment();
    textnum = tree_top(textnum);
    while (textnum > 0) {
        count += mark_as_unread(textnum, Current_conf);
        stack_text(textnum);
        textnum = pop_comment();
    }

    if (count == 1) {
        output("\n%s\n\n", MSG_URONE);
    } else if (count == 0) {
        output("\n%s\n\n", MSG_URNONE);
    } else {
        output("\n%d %s\n\n", count, MSG_URTEXT);
    }
    return 0;
}

/*
 * cmd_only - set unread texts in conference
 * args: user arguments (args)
 * ret: ok (0) or error (-1)
 */

int
cmd_only(char *args)
{
    int fd;
    long textnum, last, first, age;
    struct CONFS_ENTRY cse, tcse;
    char *buf, *oldbuf, *nbuf, *ptr;
    LINE fname;

    Change_prompt = 1;
    if (args && *args) {
        ptr = fname;
        *ptr = 0;
        textnum = strtol(args, &ptr, 10);
        if (ptr == args) {
            textnum = -1;
        } else {
            if (*ptr == ' ') {
                while (*ptr == ' ')
                    ptr++;
                switch (*ptr) {
                case 'm':
                    age = textnum * 60;
                    break;
                case 't':
                    age = textnum * 3600;
                    break;
                case 'd':
                    age = textnum * 24 * 3600;
                    break;
                default:
                    age = -1;
                }
                if (age >= 0)
                    textnum = age_to_textno(age);
                else
                    textnum = -1;
            }
        }

        first = first_text(Current_conf, Uid);
        last = last_text(Current_conf, Uid);
        if (textnum > (last - first + 1))
            textnum = (last - first + 1);
        if (textnum >= 0) {
            strcpy(fname, Home);
            strcat(fname, CONFS_FILE);

            if ((fd = open_file(fname, 0)) == -1) {
                return -1;
            }
            if ((buf = read_file(fd)) == NULL) {
                return -1;
            }
            oldbuf = buf;

            while (buf) {
                buf = get_confs_entry(buf, &cse);
                free_confs_entry(&cse);
                if (cse.num == Current_conf)
                    break;
            }

            if (cse.num == Current_conf) {
                tcse.num = cse.num;
                if (textnum == last) {
                    tcse.il = NULL;
                } else {
                    tcse.il = (struct INT_LIST *) malloc
                        (sizeof(struct INT_LIST));
                    if (tcse.il) {
                        tcse.il->next = NULL;
                        tcse.il->from = 1;
                        tcse.il->to = last - textnum;
                    } else {
                        sys_error("cmd_only", 1, "malloc");
                        return -1;
                    }
                }
                buf = oldbuf;
                nbuf = replace_confs(&tcse, buf);
                free_confs_entry(&tcse);
                critical();
                if (write_file(fd, nbuf) == -1) {
                    return -1;
                }
            } else
                output("\n%s\n\n", MSG_CONFMISSING);

            if (close_file(fd) == -1) {
                return -1;
            }
            non_critical();
            clear_comment();
            cmd_where(args);
        } else
            output("\n%s\n\n", MSG_ERRNUMT);
    } else
        output("\n%s\n\n", MSG_ERRNUMT);
    return 0;
}

/*
 * cmd_mail - send a mail to user
 * args: user arguments (args)
 * ret: ok (0) or error (-1)
 */

int
cmd_mail(char *args)
{
    LINE fname, tmpstr;
    LONG_LINE cmdline, tmp;
    char *username, *mailrec, *inbuf, *finbuf, *ptr, *inptr;
    struct TEXT_HEADER th;
    int conf, mailuid, uid, fd, msize;
    FILE *pipe;
    struct passwd *pw;

    Change_prompt = 1;
    mailrec = NULL;
    if (args && *args) {
        /* if (strchr(args, '@') || strchr(args, '!')) { */

        /* I remove the !-possbility now. That seems like some obsolete
         * news-thing which only causes potential security problems. OR
         * 00-01-02 */

        if (strchr(args, '@')) {

            strcpy(tmp, args);
            /* Only allow a minimum of nonalphanum chars in mail address */
            if (strip_string(tmp, "@+.-_%") != 0 || tmp[0] == '-') {
                output("\n%s\n\n", MSG_BADAD);
                return 0;
            }
            mailrec = args;
            mailuid = 0;
        } else {
            username = expand_name(args, USER, 0, NULL);
            if (!username) {
                return 0;
            }
            mailuid = user_uid(username);
            conf = mailuid - (mailuid * 2);
        }
        th.num = 0L;
        th.author = Uid;
        th.comment_num = 0;
        th.comment_conf = 0;
        th.comment_author = 0;
        th.time = 0;
        th.size = 0;
        th.type = TYPE_TEXT;
        strcpy(th.subject, "");
        strcpy(fname, Home);
        strcat(fname, EDIT_FILE);
        if (mailuid) {
            disp_note(mailuid);
            uid = mailuid - (mailuid * 2);
        } else {
            output("\n");
            uid = 0;
        }
        display_header(&th, 1, uid, 0, mailrec);
        if (strlen(th.subject) == 0) {
            output("\n");
            return 0;
        }
        if (line_ed(fname, &th, uid, 1, 0, NULL, mailrec) == NULL) {        /* Article deleted */
            output("\n%s\n\n", MSG_TEXTREM);
            return 0;
        }
        if (mailuid) {
            if (save_text(fname, &th, conf) == -1) {
                output("\n%s\n\n", MSG_CONFMISSING);          /* Panic, conference missing, shouldn't happen */
                return -1;
            }
            output("%s\n\n", MSG_SAVED);
        } else {
            snprintf(cmdline, sizeof(cmdline), "%s %s", MAILPRGM, mailrec);
            if ((pipe = (FILE *) popen(cmdline, "w")) == NULL) {
                output("%s\n\n", MSG_NOMAIL);                     /* Cannot exec mailprgm. */
                return -1;
            }
            if ((fd = open_file(fname, 0)) == -1) {
                return -1;
            }
            if ((inbuf = read_file(fd)) == NULL) {
                return -1;
            }
            if (close_file(fd) == -1) {
                return -1;
            }
            /*
 	    msize = (strlen(inbuf) * 3) + 1;
            finbuf = (char *) malloc (msize);
            memset(finbuf, 0, msize);
            ptr = finbuf;
            inptr = inbuf;
            */

            /* Modified to allow mails with initial line containing a
             * header-like syntax. The extra blank line added to all mails
             * will be stripped by sendmail but ensures that sendmail does not
             * consider the first real line a part of the header. / OR
             * 99-04-28 */

            msize = (strlen(inbuf) * 3) + 1 + 1;
            finbuf = (char *) malloc(msize);
            memset(finbuf, 0, msize);
            finbuf[0] = 10;
            ptr = finbuf + 1;
            inptr = inbuf;
            while (*inptr) {
                if (*inptr == '}') {
                    *ptr++ = '=';
                    *ptr++ = 'E';
                    *ptr++ = '5';
                } else if (*inptr == '{') {
                    *ptr++ = '=';
                    *ptr++ = 'E';
                    *ptr++ = '4';
                } else if (*inptr == '|') {
                    *ptr++ = '=';
                    *ptr++ = 'F';
                    *ptr++ = '6';
                } else if (*inptr == ']') {
                    *ptr++ = '=';
                    *ptr++ = 'C';
                    *ptr++ = '5';
                } else if (*inptr == '[') {
                    *ptr++ = '=';
                    *ptr++ = 'C';
                    *ptr++ = '4';
                } else if (*inptr == '\\') {
                    *ptr++ = '=';
                    *ptr++ = 'D';
                    *ptr++ = '6';
                } else if (*inptr == '=') {
                    *ptr++ = '=';
                    *ptr++ = '3';
                    *ptr++ = 'D';
                } else
                    *ptr++ = *inptr;
                inptr++;
            }
            free(inbuf);
            pw = getpwuid(Uid);
            snprintf(tmpstr, sizeof(tmpstr), "<%s@%s>", pw->pw_name, MACHINE_NAME);
            fprintf(pipe, "%s%s\n%s\n%s\n%s\n%s%s\n%s", MSG_EMRETURN, tmpstr,
                MSG_MIMETYPE, MSG_MIMECONT, MSG_MIMEENC, MSG_EMSUB,
                th.subject, finbuf);
            pclose(pipe);
            if (Copy)
                (void) save_mailcopy(mailrec, th.subject, finbuf);
            free(finbuf);
            unlink(fname);
            output("%s\n\n", MSG_MAILED);
        }
    } else
        output("\n%s\n\n", MSG_NORECIP);
    Last_text = 0;
    return 0;
}

/*
 * cmd_personal - make a personal comment to a text
 * args: user arguments (args)
 * ret: ok (0) or error (-1)
 */

int
cmd_personal(char *args)
{
    LINE fname, mr, cmdline, tmpstr;
    LONG_LINE tmp;
    char *buf, *oldbuf, *mailrec, *ptr2, *ptr3, *inbuf, *ptr4;
    char sav, *ptr, *inptr, *finbuf;
    int conf, fd, commentuid, uid, msize;
    long textnum, last, commenttext;
    struct TEXT_HEADER th, *thtmp;
    struct TEXT_BODY *tb;
    struct TEXT_ENTRY te;
    struct passwd *pw;
    FILE *pipe;

    Change_prompt = 1;
    if (!Current_conf) {
        output("\n%s\n\n", MSG_NOPERMBOX);
        return 0;
    }
    if (!args || (*args == '\0')) {
        if (Last_conf == Current_conf)
            textnum = Last_text;
        else {
            output("\n%s\n\n", MSG_NOTINCONF);
            return 0;
        }
    } else {
        textnum = parse_text(args);
    }

    last = last_text(Current_conf, Uid);

    if ((textnum <= 0) || (textnum > last)) {
        if (textnum)
            output("\n%s\n\n", MSG_ERRTNUM);
        else
            output("\n%s\n\n", MSG_NOLASTTEXT);
        return 0;
    }
    snprintf(fname, sizeof(fname), "%s/%d/%ld", SKLAFF_DB, Current_conf, textnum);

    if ((fd = open_file(fname, OPEN_QUIET)) == -1) {
        output("\n%s\n\n", MSG_NOTEXT);
        return 0;
    }
    if ((buf = read_file(fd)) == NULL) {
        output("\n%s\n\n", MSG_NOREAD);
        return 0;
    }
    oldbuf = buf;

    if (close_file(fd) == -1) {
        return 0;
    }
    buf = get_text_entry(buf, &te);

    free(oldbuf);

    thtmp = &te.th;
    commenttext = thtmp->num;
    commentuid = thtmp->author;
    mailrec = NULL;
    if (!commentuid) {
        tb = te.body;
        while (tb) {
            if ((ptr2 = strstr(tb->line, MSG_EMFROM)) != NULL)
                break;
            else if ((ptr2 = strstr(tb->line, MSG_EMFROM2)) != NULL)
                break;
            else if ((ptr2 = strstr(tb->line, MSG_EMFROM3)) != NULL)
                break;
            tb = tb->next;
        }
        ptr2 = ptr2 + strlen(MSG_EMFROM);
        ptr3 = strchr(ptr2, '@');
        if (!ptr3)
            ptr3 = strchr(ptr2, '!');
        if (ptr3) {
            while ((*ptr3 != ' ') && (*ptr3 != '<'))
                ptr3--;
            ptr3++;
            ptr4 = strchr(ptr3, '>');
            if (!ptr4)
                ptr4 = strchr(ptr3, ' ');
            if (ptr4) {
                sav = *ptr4;
                *ptr4 = '\0';
            }
            strcpy(mr, ptr3);
            if (ptr4)
                *ptr4 = sav;
        } else {
            ptr3 = strchr(ptr2, '(');
            if (!ptr3)
                ptr3 = strchr(ptr2, '<');
            if (ptr3) {
                ptr3--;
                sav = *ptr3;
                *ptr3 = '\0';
                strcpy(mr, ptr2);
                *ptr3 = sav;
            } else
                strcpy(mr, ptr2);
        }
        strcpy(tmp, mr);
        /* Only allow a minimum of nonalphanum chars in mail address */
        if (strip_string(tmp, "@+.-_%") != 0 || tmp[0] == '-') {
            output("\n%s\n\n", MSG_BADAD);
            return 0;
        }
        mailrec = mr;
    }
    free_text_entry(&te);

    conf = commentuid - (commentuid * 2);
    th.num = 0L;
    th.author = Uid;
    th.comment_num = commenttext;
    th.comment_conf = Current_conf;
    th.comment_author = commentuid;
    th.time = 0;
    th.size = 0;
    th.type = TYPE_TEXT;
    strcpy(th.subject, thtmp->subject);
    strcpy(fname, Home);
    strcat(fname, EDIT_FILE);
    if (!commentuid) {
        output("\n");
    } else {
        disp_note(commentuid);
    }
    uid = commentuid - (commentuid * 2);
    display_header(&th, Subject_change, uid, 0, mailrec);
    if (line_ed(fname, &th, uid, 1, 0, NULL, mailrec) == NULL) {
        output("\n%s\n\n", MSG_TEXTREM);
        return 0;
    }
    if (commentuid || (th.author && conf)) {
        if (save_text(fname, &th, conf) == -1) {
            output("\n%s\n\n", MSG_CONFMISSING);
            return -1;
        }
    } else {
        snprintf(cmdline, sizeof(cmdline), "%s %s", MAILPRGM, mailrec);
        if ((pipe = (FILE *) popen(cmdline, "w")) == NULL) {
            output("%s\n\n", MSG_NOMAIL);
            return -1;
        }
        if ((fd = open_file(fname, 0)) == -1) {
            return -1;
        }
        if ((inbuf = read_file(fd)) == NULL) {
            return -1;
        }
        if (close_file(fd) == -1) {
            return -1;
        }
        msize = (strlen(inbuf) * 3) + 1;
        finbuf = (char *) malloc(msize);
        memset(finbuf, 0, msize);
        ptr = finbuf;
        inptr = inbuf;
        while (*inptr) {
            if (*inptr == '}') {
                *ptr++ = '=';
                *ptr++ = 'E';
                *ptr++ = '5';
            } else if (*inptr == '{') {
                *ptr++ = '=';
                *ptr++ = 'E';
                *ptr++ = '4';
            } else if (*inptr == '|') {
                *ptr++ = '=';
                *ptr++ = 'F';
                *ptr++ = '6';
            } else if (*inptr == ']') {
                *ptr++ = '=';
                *ptr++ = 'C';
                *ptr++ = '5';
            } else if (*inptr == '[') {
                *ptr++ = '=';
                *ptr++ = 'C';
                *ptr++ = '4';
            } else if (*inptr == '\\') {
                *ptr++ = '=';
                *ptr++ = 'D';
                *ptr++ = '6';
            } else if (*inptr == '=') {
                *ptr++ = '=';
                *ptr++ = '3';
                *ptr++ = 'D';
            } else
                *ptr++ = *inptr;
            inptr++;
        }
        free(inbuf);
        pw = getpwuid(Uid);
        snprintf(tmpstr, sizeof(tmpstr), "<%s@%s>", pw->pw_name, MACHINE_NAME);
        fprintf(pipe, "%s%s\n%s\n%s\n%s\n%s%s\n%s", MSG_EMRETURN, tmpstr,
            MSG_MIMETYPE, MSG_MIMECONT, MSG_MIMEENC, MSG_EMSUB,
            th.subject, finbuf);
        pclose(pipe);
        if (Copy)
            (void) save_mailcopy(mailrec, th.subject, finbuf);
        free(finbuf);
        unlink(fname);
        output("%s\n\n", MSG_MAILED);
        return 0;
    }
    output("%s\n\n", MSG_SAVED);
    Last_text = 0;
    return 0;
}

/*
 * cmd_post_text - post text in conference
 * args: user arguments (args)
 * ret: ok (0) or error (-1)
 */

int
cmd_post_text(char *args)
{
    LINE fname, cname;
    char *confname, *un, *inbuf;
    struct TEXT_HEADER th;
    int confid, fd;
    long textnum;
    struct CONF_ENTRY *ce;
    LONG_LINE uname, cmdline; /* We don't use group anymore */
    LONG_LINE tmp; /* moved here for inews header generation (2025-08-07, PL) */
    FILE *pipe;
    struct SKLAFFRC *rc;

    Change_prompt = 1;
    un = NULL;
    if (!args || (*args == '\0')) {
        strcpy(args, conf_name(Current_conf, cname));
        confid = Current_conf;
    } else {
        confname = expand_name(args, CONF, 0, NULL);
        if (!confname) {
            return 0;
        }
        confid = conf_num(confname);
    }

    if (!confid) {
        output("\n%s\n\n", MSG_NOPOSTMBOX);
        return 0;
    }
    if (!member_of(Uid, confid)) {
        output("\n%s\n\n", MSG_NOSUB);
        return 0;
    }
    th.author = Uid;
    ce = get_conf_struct(confid);
#ifndef POSTING_OK
    if (ce->type == NEWS_CONF) {
        output("\n%s\n\n", MSG_NONEWS);
        return 0;
    }
#endif
#ifdef POSTING_OK
    if (ce->type == NEWS_CONF) {
        struct passwd *pw = getpwuid(Uid);

        snprintf(uname, sizeof(uname), "%s@%s (%s)", pw->pw_name, MACHINE_NAME,
            user_name(Uid, tmp));
        un = uname;
 //       snprintf(group, sizeof(group), "%s %s", MSG_NGROUP, conf_name(confid, tmp));
        th.author = 0;
    }
#endif
    th.num = 0L;
    th.comment_num = 0;
    th.comment_conf = 0;
    th.comment_author = 0;
    th.size = 0;
    th.time = 0;
    strcpy(th.subject, "");
    th.type = TYPE_TEXT;
    strcpy(fname, Home);
    strcat(fname, EDIT_FILE);
    output("\n");
    if (un) {
        if ((fd = open_file(POST_INFO, 0)) == -1)
            return -1;
        if ((inbuf = read_file(fd)) == NULL)
            return -1;
        if (close_file(fd) == -1)
            return -1;
        output(inbuf);
        free(inbuf);
        output("\n");
    }
    display_header(&th, 1, confid, 0, un);
    if (strlen(th.subject) == 0) {
        output("\n");
        return 0;
    }
    if (line_ed(fname, &th, confid, 1, 1, NULL, un) == NULL) {
        output("\n%s\n\n", MSG_TEXTREM);
        return 0;
    }
    if (un) {
        snprintf(cmdline, sizeof(cmdline), "%s", NEWSPRGM);
        if ((pipe = (FILE *) popen(cmdline, "w")) == NULL) {
            output("%s\n\n", MSG_NOINEWS);
            return -1;
        }
        if ((fd = open_file(fname, 0)) == -1)
            return -1;
        if ((inbuf = read_file(fd)) == NULL)
            return -1;
        if (close_file(fd) == -1)
            return -1;
        rc = read_sklaffrc(Uid);
        if (rc != NULL) {
            if (strlen(rc->sig)) {
             /*fprintf(pipe, "%s%s\n%s\n%s%s\n\n%s--\n%s", MSG_EMFROM,
                    uname, group, MSG_EMSUB, th.subject, inbuf, rc->sig);*/
/*
		fprintf(pipe, "From: %s\n", uname);
		fprintf(pipe, "Newsgroups: %s\n", conf_name(confid, tmp));
		fprintf(pipe, "Subject: %s\n", th.subject);
		fprintf(pipe, "\n%s\n--\n%s\n", inbuf, rc->sig);
*/

fprintf(pipe, "From: %s\n", uname);
fprintf(pipe, "Newsgroups: %s\n", conf_name(confid, tmp));
fprintf(pipe, "Subject: %s\n", th.subject);
fprintf(pipe, "Content-Type: text/plain; charset=UTF-8\n");
fprintf(pipe, "MIME-Version: 1.0\n");
fprintf(pipe, "\n");
fprintf(pipe, "%s\n", inbuf);
fprintf(pipe, "--\n%s\n", rc->sig);


            } else {
                /*fprintf(pipe, "%s%s\n%s\n%s%s\n\n%s", MSG_EMFROM, uname,
                    group, MSG_EMSUB, th.subject, inbuf);*/

/*

		fprintf(pipe, "From: %s\n", uname);
		fprintf(pipe, "Newsgroups: %s\n", conf_name(confid, tmp));
		fprintf(pipe, "Subject: %s\n", th.subject);
		fprintf(pipe, "\n%s\n", inbuf);
*/
		fprintf(pipe, "From: %s\n", uname);
		fprintf(pipe, "Newsgroups: %s\n", conf_name(confid, tmp));
		fprintf(pipe, "Subject: %s\n", th.subject);
		fprintf(pipe, "Content-Type: text/plain; charset=UTF-8\n");
		fprintf(pipe, "MIME-Version: 1.0\n");
		fprintf(pipe, "\n");  // REQUIRED blank line between headers and body
		fprintf(pipe, "%s\n", inbuf);
        } 
}
	fflush(pipe);        /* Added for good measure 2025-08-07 PL */
        fputs("\004", pipe); /* Writes ^D (not sure if really needed?) */
        if (rc != NULL)
            free(rc);
        free(inbuf);
        pclose(pipe);
        unlink(fname);
        output("%s\n\n", MSG_POSTED);
        return 0;
    } else if ((textnum = save_text(fname, &th, confid)) == -1) {
        output("\n%s\n\n", MSG_CONFMISSING);
        return -1;
    }
    output("%s %ld %s\n\n", MSG_TEXTNAME, textnum, MSG_SAVED2);
    if (confid != Current_conf) {
        Last_text = 0;
    } else {
        Last_text = textnum;
    }
    mark_as_read(textnum, confid);
    return 0;
}

/*
 * cmd_comment - post a comment to a text/mail
 * args: user arguments (args)
 * ret: ok (0) or error (-1)
 */

int
cmd_comment(char *args)
{
    LINE fname, newline, mr, cmdline, uname, refname, reference, tmpstr;
    LONG_LINE group, tmp, cname;
    char *buf, *oldbuf, *nbuf, *ptr2, *mailrec, *inbuf, *ptr3, *ptr4, sav;
    char *finbuf, *inptr, *bptr;
    int conf, fd, commentuid, allow, nc, *ptr, i, msize, right;
    long textnum, last, commenttext, savednum;
    struct TEXT_HEADER th, *thtmp;
    struct TEXT_BODY *tb;
    struct TEXT_ENTRY te;
    struct CONF_ENTRY *ce, *ce2;
    struct passwd *pw;
    struct SKLAFFRC *rc;
    struct USER_LIST *ul;
    FILE *pipe;

    Change_prompt = 1;
    if (!args || (*args == '\0')) {
        if (Last_conf == Current_conf)
            textnum = Last_text;
        else {
            output("\n%s\n\n", MSG_NOTINCONF);
            return 0;
        }
    } else {
        textnum = parse_text(args);
    }

    last = last_text(Current_conf, Uid);
    if ((textnum <= 0) || (textnum > last)) {
        if (textnum)
            output("\n%s\n\n", MSG_ERRTNUM);
        return 0;
    }
    if (Current_conf > 0) {
        snprintf(cname, sizeof(cname), "%s/%d/%ld", SKLAFF_DB, Current_conf, textnum);
        allow = 1;
    } else {
        snprintf(cname, sizeof(cname), "%s/%ld", Mbox, textnum);
        allow = 0;
    }

    if ((fd = open_file(cname, OPEN_QUIET)) == -1) {
        output("\n%s\n\n", MSG_NOTEXT);
        return 0;
    }
    if ((buf = read_file(fd)) == NULL) {
        output("\n%s\n\n", MSG_NOREAD);
        return 0;
    }
    if (close_file(fd) == -1) {
        return 0;
    }
    oldbuf = buf;
    buf = get_text_entry(buf, &te);

    free(oldbuf);

    thtmp = &te.th;
    commenttext = thtmp->num;
    commentuid = thtmp->author;
    mailrec = NULL;
    if (!commentuid) {
        tb = te.body;
        while (tb) {
            if ((ptr2 = strstr(tb->line, MSG_EMFROM)) != NULL)
                break;
            else if ((ptr2 = strstr(tb->line, MSG_EMFROM2)) != NULL)
                break;
            else if ((ptr2 = strstr(tb->line, MSG_EMFROM3)) != NULL)
                break;
            tb = tb->next;
        }
        ptr2 = ptr2 + strlen(MSG_EMFROM);
        ptr3 = strchr(ptr2, '@');
        if (!ptr3)
            ptr3 = strchr(ptr2, '!');
        if (ptr3) {
            while ((*ptr3 != ' ') && (*ptr3 != '<'))
                ptr3--;
            ptr3++;
            ptr4 = strchr(ptr3, '>');
            if (!ptr4)
                ptr4 = strchr(ptr3, ' ');
            if (ptr4) {
                sav = *ptr4;
                *ptr4 = '\0';
            }
            strcpy(mr, ptr3);
            if (ptr4)
                *ptr4 = sav;
        } else {
            ptr3 = strchr(ptr2, '(');
            if (!ptr3)
                ptr3 = strchr(ptr2, '<');
            if (ptr3) {
                ptr3--;
                sav = *ptr3;
                *ptr3 = '\0';
                strcpy(mr, ptr2);
                *ptr3 = sav;
            } else
                strcpy(mr, ptr2);
        }
        strcpy(tmp, mr);
        /* Only allow a minimum of nonalphanum chars in mail address */
        if (strip_string(tmp, "@+.-_%") != 0 || tmp[0] == '-') {
            output("\n%s\n\n", MSG_BADAD);
            return 0;
        }
        mailrec = mr;
    }
    free_text_entry(&te);

    if (Current_conf)
        conf = Current_conf;
    else
        conf = commentuid - (commentuid * 2);
    th.num = 0L;
    th.author = Uid;
    th.comment_num = commenttext;
    th.comment_conf = 0;
    th.comment_author = commentuid;
    th.size = 0;
    th.time = 0;
    th.type = TYPE_TEXT;
    strcpy(th.subject, thtmp->subject);
    strcpy(fname, Home);
    strcat(fname, EDIT_FILE);
    if (Current_conf || !commentuid) {
        output("\n");
#ifdef POSTING_OK
        if (Current_conf && !commentuid) {
            if ((fd = open_file(POST_INFO, 0)) == -1)
                return -1;
            if ((buf = read_file(fd)) == NULL)
                return -1;
            if (close_file(fd) == -1)
                return -1;
            output(buf);
            free(buf);
            output("\n");
        }
#endif
    } else {
        disp_note(commentuid);
    }

    if (Current_conf) {
        ce = get_conf_struct(conf);
        if (ce->comconf) {
            ul = get_confrc_struct(conf);
            right = conf_right(ul, Uid, ce->type, ce->creator);
            free_userlist(ul);
            if (right == 0) {
                nc = ce->comconf;
                th.comment_conf = conf;
                ce2 = get_conf_struct(ce->comconf);
                output(MSG_CMNTMOVED);
                output(ce2->name);
                output(".\n\n");
            } else {
                nc = Current_conf;
            }
        } else
            nc = Current_conf;
        ptr = &nc;
    } else {
        nc = conf;
        ptr = NULL;
    }

    display_header(&th, Subject_change, nc, 0, mailrec);

    if (line_ed(fname, &th, conf, 1, allow, ptr, mailrec) == NULL) {
        output("\n%s\n\n", MSG_TEXTREM);
        return 0;
    }
    if (nc > 0) {
        ce = get_conf_struct(nc);
        if (ce->type == NEWS_CONF) {
            th.author = 0;
        }
    }
    if (commentuid || (th.author && nc)) {
        if ((savednum = save_text(fname, &th, nc)) == -1) {
            output("\n%s\n\n", MSG_CONFMISSING);
            return -1;
        }
    } else if (!nc) {
        snprintf(cmdline, sizeof(cmdline), "%s %s", MAILPRGM, mailrec);
        if ((pipe = (FILE *) popen(cmdline, "w")) == NULL) {
            output("%s\n\n", MSG_NOMAIL);
            return -1;
        }
        if ((fd = open_file(fname, 0)) == -1)
            return -1;
        if ((inbuf = read_file(fd)) == NULL)
            return -1;
        if (close_file(fd) == -1)
            return -1;

        msize = (strlen(inbuf) * 3) + 1;
        finbuf = (char *) malloc(msize);
        memset(finbuf, 0, msize);
        bptr = finbuf;
        inptr = inbuf;
        while (*inptr) {
            if (*inptr == '}') {
                *bptr++ = '=';
                *bptr++ = 'E';
                *bptr++ = '5';
            } else if (*inptr == '{') {
                *bptr++ = '=';
                *bptr++ = 'E';
                *bptr++ = '4';
            } else if (*inptr == '|') {
                *bptr++ = '=';
                *bptr++ = 'F';
                *bptr++ = '6';
            } else if (*inptr == ']') {
                *bptr++ = '=';
                *bptr++ = 'C';
                *bptr++ = '5';
            } else if (*inptr == '[') {
                *bptr++ = '=';
                *bptr++ = 'C';
                *bptr++ = '4';
            } else if (*inptr == '\\') {
                *bptr++ = '=';
                *bptr++ = 'D';
                *bptr++ = '6';
            } else if (*inptr == '=') {
                *bptr++ = '=';
                *bptr++ = '3';
                *bptr++ = 'D';
            } else
                *bptr++ = *inptr;
            inptr++;
        }
        free(inbuf);
        pw = getpwuid(Uid);
        snprintf(tmpstr, sizeof(tmpstr), "<%s@%s>", pw->pw_name, MACHINE_NAME);
        fprintf(pipe, "%s%s\n%s\n%s\n%s\n%s%s\n%s", MSG_EMRETURN, tmpstr,
            MSG_MIMETYPE, MSG_MIMECONT, MSG_MIMEENC, MSG_EMSUB,
            th.subject, finbuf);
        pclose(pipe);
        if (Copy)
            (void) save_mailcopy(mailrec, th.subject, finbuf);
        free(finbuf);
        unlink(fname);
        output("%s\n\n", MSG_MAILED);
        return 0;
    } else {
        snprintf(refname, sizeof(refname), "%s/%d/%ld", SKLAFF_DB, Current_conf, textnum);
        if ((fd = open_file(refname, 0)) == -1)
            return -1;
        if ((buf = read_file(fd)) == NULL)
            return -1;
        oldbuf = buf;
        if (close(fd) == -1)
            return -1;
        strcpy(reference, "");
        ptr4 = strstr(buf, MSG_MSGID);
        if (ptr4) {
            ptr4 = strchr(ptr4, '<');
            ptr2 = strchr(ptr4, '>');
            if (ptr4 && ptr2 && (ptr2 > ptr4)) {
                *ptr2 = '\0';
                strcpy(reference, (ptr4 + 1));
            }
        }
        free(oldbuf);
        snprintf(cmdline, sizeof(cmdline), "%s", NEWSPRGM);
	if ((pipe = (FILE *) popen(cmdline, "w")) == NULL) {
            output("%s\n\n", MSG_NOINEWS);
            return -1;
        }
        pw = getpwuid(Uid);
        snprintf(uname, sizeof(uname), "%s@%s (%s)", pw->pw_name, MACHINE_NAME,
            user_name(Uid, tmp));
        snprintf(group, sizeof(group), "%s %s", MSG_NGROUP, conf_name(nc, tmp));
        if ((fd = open_file(fname, 0)) == -1)
            return -1;
        if ((inbuf = read_file(fd)) == NULL)
            return -1;
        if (close_file(fd) == -1)
            return -1;
        rc = read_sklaffrc(Uid);
        if (rc != NULL) {
            if (strlen(rc->sig)) {
                fprintf(pipe, "%s%s\n%s\n%s <%s>\n%s%s\n\n%s--\n%s",
                    MSG_EMFROM, uname, group, MSG_REFID, reference,
                    MSG_EMSUB, th.subject, inbuf, rc->sig);
            } else
                fprintf(pipe, "%s%s\n%s\n%s <%s>\n%s%s\n\n%s", MSG_EMFROM,
                    uname, group, MSG_REFID, reference, MSG_EMSUB,
                    th.subject, inbuf);
        }
        fputs("\004", pipe);
        free(inbuf);
        if (rc != NULL)
            free(rc);
        pclose(pipe);
        unlink(fname);
        output("%s\n\n", MSG_POSTED);
        return 0;
    }

    if (Current_conf && (nc == conf)) {

        if ((fd = open_file(cname, OPEN_QUIET)) == -1) {
            output("\n%s\n\n", MSG_NOTEXT);
            return 0;
        }
        if ((buf = read_file(fd)) == NULL) {
            output("\n%s\n\n", MSG_NOREAD);
            return 0;
        }
        i = strlen(buf) + LINE_LEN;
        nbuf = (char *) malloc(i);
        if (!nbuf) {
            sys_error("cmd_comment", 1, "malloc");
            return -1;
        }
        memset(nbuf, 0, i);

        snprintf(newline, sizeof(newline), "%ld:%d\n", savednum, Uid);
        strcpy(nbuf, buf);
        strcat(nbuf, newline);
        free(buf);

        critical();
        if (write_file(fd, nbuf) == -1) {
            output("\n%s\n\n", MSG_NOREPPTR);
            return 0;
        }
        if (close_file(fd) == -1) {
            return 0;
        }
        non_critical();
    }
    if (Current_conf) {
        mark_as_read(savednum, nc);
        output("%s %ld %s\n\n", MSG_TEXTNAME, savednum, MSG_SAVED2);
        if (nc == Current_conf)
            Last_text = savednum;
    } else {
        output("%s\n\n", MSG_SAVED);
    }
    return 0;
}

/*
 * cmd_display_commented - display commented text
 * args: user arguments (args)
 * ret: ok (0) or error (-1)
 */

int
cmd_display_commented(char *args)
{
    strcpy(args, MSG_REPLIED);
    cmd_read_text(args);
    return 0;
}

/*
 * cmd_display_last - display last read text in conference
 * args: user arguments (args)
 * ret: ok (0) or error (-1)
 */

int
cmd_display_last(char *args)
{
    sprintf(args, "%ld", Last_text);
    cmd_read_text(args);
    return 0;
}

/*
 * cmd_display_rot13 - display last read text in conference (rot13)
 * args: user arguments (args)
 * ret: ok (0) or error (-1)
 */

int
cmd_display_rot13(char *args)
{
    sprintf(args, "%ld", Last_text);
    Rot13 = 1;
    cmd_read_text(args);
    return 0;
}

/*
 * cmd_mod_pinfo - modify personal information
 * args: user arguments (args)
 * ret: ok (0) or error (-1)
 */

int
cmd_mod_pinfo(char *args)
{
    char u_name[255];
    struct SKLAFFRC *rc;
    char str2[255];

    user_name(Uid, u_name);

    rc = read_sklaffrc(Uid);
    if (rc != NULL) {
        output("\n%s %s.\n\n", MSG_MODPINFO, u_name);
//        output(MSG_MODPINFO2); /* upcoming feature */
/*
        output(MSG_INFOADDR);
        input(rc->user.adress, str2, 60, 0, 0, 0);
        strcpy(rc->user.adress, str2);

        output(MSG_INFOZIP);
        input(rc->user.postnr, str2, 60, 0, 0, 0);
        strcpy(rc->user.postnr, str2);

        output(MSG_INFOTELE1);
        input(rc->user.tele1, str2, 60, 0, 0, 0);
        strcpy(rc->user.tele1, str2);

        output(MSG_INFOTELE2);
        input(rc->user.tele2, str2, 60, 0, 0, 0);
        strcpy(rc->user.tele2, str2);

        output(MSG_INFOTELE3);
        input(rc->user.tele3, str2, 60, 0, 0, 0);
        strcpy(rc->user.tele3, str2);
*/
        output(MSG_INFOMAIL1);
        input(rc->user.email1, str2, 60, 0, 0, 0);
        strcpy(rc->user.email1, str2);
/*
        output(MSG_INFOMAIL2);
        input(rc->user.email2, str2, 60, 0, 0, 0);
        strcpy(rc->user.email2, str2);
*/
        output(MSG_INFOURL);
        input(rc->user.url, str2, 60, 0, 0, 0);
        strcpy(rc->user.url, str2);

        output(MSG_INFOTOWN);
        input(rc->user.ort, str2, 60, 0, 0, 0);
        strcpy(rc->user.ort, str2);
/*
        output(MSG_INFOORG);
        input(rc->user.org, str2, 60, 0, 0, 0);
        strcpy(rc->user.org, str2);
*/
        output("\n%s", MSG_SAVEINFO);
        input(MSG_YES, str2, 4, 0, 0, 0);
        down_string(str2);

        if (*str2 == MSG_YESANSWER) {
            write_sklaffrc(Uid, rc);
            output("\n%s\n\n", MSG_INFOSAVED);
        } else {
            free(rc);
            output("\n%s\n\n", MSG_INFONOSAVE);
        }
    }
    return 0;
}

/*
 * cmd_show_status - show status for an object
 * args: user arguments (args)
 * ret: ok (0) or error (-1)
 */

int
cmd_show_status(char *args)
{
    int i_tmp, flag, num;
    char *u_name, *c_name, *tmpname;
    static LINE cname, uname;

    flag = USER;
    num = -1;

    rtrim(args);
    c_name = cname;
    u_name = uname;
    if (strlen(args) == 0) {
        num = Uid;
    } else {
        tmpname = expand_name(args, CONF | USER, 0, &i_tmp);
        if (!tmpname) {
            return 0;
        }
        switch (i_tmp) {
        case USER:
            strcpy(u_name, tmpname);
            num = user_uid(u_name);
            break;
        case CONF:
            strcpy(c_name, tmpname);
            num = conf_num(c_name);
            flag = CONF;
        }
    }

    return show_status(num, flag, STATUS_INTERNAL);
}

/*
 * cmd_say - tell a user something
 * args: user arguments (args)
 * ret: ok (0) or error (-1)
 */

int
cmd_say(char *args)
{
    char tmpname[81], *name;
    LINE msg;
    int xit, uid, w;

    w = 0;

    while ((args[w]) && (args[w] != ',')) {
        w++;
    }

    strncpy(tmpname, args, w);
    tmpname[w] = 0;
    rtrim(tmpname);
    if (!strlen(tmpname)) {
        output
            ("\n%s\n\n", MSG_NOUNAME);
        return 0;
    }
    if ((name = expand_name(tmpname, ACTIVE, 0, NULL)) == NULL) {
        return 0;
    }
    if ((uid = user_uid(name)) == -1) {
        return 0;
    }
    output("\n");

    user_name(Uid, tmpname);

    if (args[w] == ',') {
        send_msg(uid, MSG_SAY, &args[w + 1], 1);
        output("\n");
    } else {
        xit = 0;
        do {
            display_msg(0);
            output(MSG_MSGPROMPT);
            Interrupt_input = 2;
            input("", msg, 77 - strlen(tmpname), 0, 1, 0);
            Interrupt_input = 0;
            rtrim(msg);
            if (strlen(msg)) {
                if (send_msg(uid, MSG_SAY, msg, 2) == -1) {
                    xit = 1;
                }
                output("\n");
            } else {
                xit = 1;
            }
        } while (!xit);
        output("\n");
    }
    strcpy(Overflow, "");
    return 0;
}

/*
 * cmd_yell - tell all users something
 * args: user arguments (args)
 * ret: ok (0) or error (-1)
 */

int
cmd_yell(char *args)
{
    int xit, nl;
    LINE msg;

    rtrim(args);
    output("\n");
    if (strlen(args)) {
        send_msg_to_all(MSG_YELL, args);
        output("\n");
    } else {
        xit = 0;
        nl = 1;
        do {
            display_msg(0);
            output(MSG_MSGPROMPT);
            Interrupt_input = 2;
            input("", msg, 65, 0, 1, 0);
            Interrupt_input = 0;
            rtrim(msg);
            if (strlen(msg)) {
                output("\n");
                if (send_msg_to_all(MSG_YELL, msg) == -1) {
                    nl = 0;
                    xit = 1;
                } else {
                    output("\n");
                }
            } else {
                xit = 1;
            }
        } while (!xit);
        if (nl)
            output("\n");
    }
    return 0;
}

/*
 * cmd_mod_note - modify users note
 * args: user arguments (args)
 * ret: ok (0) or error (-1)
 */

int
cmd_mod_note(char *args)
{
    int u_num, j;
    char u_name[255];
    struct SKLAFFRC *rc;
    char user_home[255];
    char *ptr, *nbuf;
    struct TEXT_HEADER th;
    int fd;
    char *func_name = "cmd_mod_note";

    u_num = Uid;
    user_name(u_num, u_name);

    rc = read_sklaffrc(u_num);
    if (rc != NULL) {
        (void) user_dir(u_num, user_home);
        strcat(user_home, TMP_NOTE);
        if ((fd = create_file(user_home)) == -1) {
            sys_error(func_name, 3, "open_file (LOCAL_SKLAFFRC)");
        }
        j = strlen(rc->note) + LINE_LEN;
        if ((nbuf = malloc(j)) == NULL) {
            sys_error("cmd_mod_note", 1, "malloc");
            return -1;
        }
        memset(nbuf, 0, j);
        strcpy(nbuf, rc->note);
        critical();
        write_file(fd, nbuf);
        close_file(fd);
        non_critical();

        output("\n");
        if (line_ed(user_home, &th, 0, 1, 0, NULL, NULL) == 0) {        /* FIX */
            unlink(user_home);
            output("\n");
        } else {
            if ((fd = open_file(user_home, 0)) == -1) {
                sys_error(func_name, 4, "haffo");
            }
            if ((ptr = read_file(fd)) == NULL) {
                sys_error("haffo", 3, "baffo");
                return 0;
            }
            close_file(fd);
            unlink(user_home);
            strcpy(rc->note, ptr);
            free(ptr);
        }
        write_sklaffrc(Uid, rc);

    }
    return 0;
}

/*
 * cmd_mod_sig - modify users news-signature
 * args: user arguments (args)
 * ret: ok (0) or error (-1)
 */

int
cmd_mod_sig(char *args)
{
    int u_num, j;
    char u_name[255];
    struct SKLAFFRC *rc;
    char user_home[255];
    char *ptr, *nbuf;
    struct TEXT_HEADER th;
    int fd;
    char *func_name = "cmd_mod_sig";

    u_num = Uid;
    user_name(u_num, u_name);

    rc = read_sklaffrc(u_num);
    if (rc != NULL) {
        (void) user_dir(u_num, user_home);
        strcat(user_home, TMP_NOTE);
        if ((fd = create_file(user_home)) == -1) {
            sys_error(func_name, 3, "open_file (LOCAL_SKLAFFRC)");
        }
        j = strlen(rc->sig) + LINE_LEN;
        if ((nbuf = malloc(j)) == NULL) {
            sys_error("cmd_mod_sig", 1, "malloc");
            return -1;
        }
        memset(nbuf, 0, j);
        strcpy(nbuf, rc->sig);
        critical();
        write_file(fd, nbuf);
        close_file(fd);
        non_critical();

        output("\n");
        if (line_ed(user_home, &th, 0, 1, 0, NULL, NULL) == 0) {
            unlink(user_home);
            output("\n");
        } else {
            if ((fd = open_file(user_home, 0)) == -1) {
                sys_error(func_name, 4, "haffo");
            }
            if ((ptr = read_file(fd)) == NULL) {
                sys_error("haffo", 3, "baffo");
                return 0;
            }
            close_file(fd);
            unlink(user_home);
            strcpy(rc->sig, ptr);
            free(ptr);
        }
        write_sklaffrc(Uid, rc);
    }
    return 0;
}

 /*
 * cmd_mod_timeout - modify users timeout period
 * args: user arguments (args)
 * ret: ok (0) or error (-1)
 */

int
cmd_mod_timeout(char *args)
{
    struct SKLAFFRC *rc;
    LINE subba;
    int intime;

    rc = read_sklaffrc(Uid);
    if (rc) {
        output("\n%s", MSG_ASKTIMEOUT);
        strcpy(subba, "");
        input(subba, subba, LINE_LEN, 0, 0, 0);
        if (*subba == '\0') {
            output("\n");
            return 0;
        }
        intime = atoi(subba);
        if (intime < 0)
            intime = 0;
        snprintf(subba, sizeof(subba), "%d", intime);
        output("\n%s %d %s\n\n", MSG_TIMESET, intime, MSG_MINUTES);
        strcpy(rc->timeout, subba);
        if (rc->timeout[0] != '\0') {
            Timeout = atoi(rc->timeout);
            if (Timeout) {
                alarm(60 * Timeout);
            }
        } else {
            Timeout = 0;
        }
        write_sklaffrc(Uid, rc);
    }
    return 0;
}

 /*
 * cmd_mod_login - modify users login commands
 * args: user arguments (args)
 * ret: ok (0) or error (-1)
 */

int
cmd_mod_login(char *args)
{
    int u_num, j;
    char u_name[255];
    struct SKLAFFRC *rc;
    char user_home[255];
    char *ptr, *nbuf;
    struct TEXT_HEADER th;
    int fd;
    char *func_name = "cmd_mod_login";

    u_num = Uid;
    user_name(u_num, u_name);

    rc = read_sklaffrc(u_num);
    if (rc != NULL) {
        (void) user_dir(u_num, user_home);
        strcat(user_home, TMP_NOTE);
        if ((fd = create_file(user_home)) == -1) {
            sys_error(func_name, 3, "open_file (LOCAL_SKLAFFRC)");
        }
        j = strlen(rc->login) + LINE_LEN;
        if ((nbuf = malloc(j)) == NULL) {
            sys_error("cmd_mod_login", 1, "malloc");
            return -1;
        }
        memset(nbuf, 0, j);
        strcpy(nbuf, rc->login);
        critical();
        write_file(fd, nbuf);;
        close_file(fd);
        non_critical();

        output("\n");
        if (line_ed(user_home, &th, 0, 1, 0, NULL, NULL) == 0) {        /* FIX */
            unlink(user_home);
            output("\n");
        } else {
            if ((fd = open_file(user_home, 0)) == -1) {
                sys_error(func_name, 4, "haffo");
            }
            if ((ptr = read_file(fd)) == NULL) {
                sys_error("haffo", 3, "baffo");
                return 0;
            }
            close_file(fd);
            unlink(user_home);
            strcpy(rc->login, ptr);
            free(ptr);
        }

        write_sklaffrc(Uid, rc);

    }
    return 0;
}

/*
 * cmd_delete_text - delete text from database
 * args: user arguments (args)
 * ret: ok (0) or error (-1)
 */

int
cmd_delete_text(char *args)
{
    LONG_LINE fname;
    int fd;
    char *buf, *oldbuf;
    struct TEXT_ENTRY te;
    struct TEXT_HEADER *th;
    long num;

    num = parse_text(args);

    if (!num) {
        return 0;
    }
    if (Current_conf > 0) {
        snprintf(fname, sizeof(fname), "%s/%d/%ld", SKLAFF_DB, Current_conf, num);
    } else {
        snprintf(fname, sizeof(fname), "%s/%ld", Mbox, num);
    }

    if ((fd = open_file(fname, OPEN_QUIET)) == -1) {
        output("\n%s\n\n", MSG_NOTEXT);
        return 0;
    }
    if ((buf = read_file(fd)) == NULL) {
        output("\n%s\n\n", MSG_NOREAD);
        return 0;
    }
    oldbuf = buf;

    if (close_file(fd) == -1) {
        return 0;
    }
    buf = get_text_entry(buf, &te);
    free_text_entry(&te);

    th = &te.th;

    if (th->author != Uid) {
        output("\n%s\n\n", MSG_NOTALLOW);
        return 0;
    }
    unlink(fname);
    free(oldbuf);
    output("\n%s %ld %s\n\n", MSG_TEXTNAME, num, MSG_DELETED);
    return 0;
}

/*
 * cmd_unread_text - mark text as unread when user logs out
 * args: user arguments (args)
 * ret: ok (0) or error (-1)
 */

int
cmd_unread_text(char *args)
{
    long textnum;
    LONG_LINE cname;

    Change_prompt = 1;
    if (*args == '\0') {
        if (!Last_text) {
            output("\n%s\n\n", MSG_ERRTNUM);
            return 0;
        } else {
            textnum = Last_text;
        }

    } else {
        textnum = parse_text(args);
    }

    if (Current_conf > 0) {
        snprintf(cname, sizeof(cname), "%s/%d/%ld", SKLAFF_DB, Current_conf, textnum);
    } else {
        snprintf(cname, sizeof(cname), "%s/%ld", Mbox, textnum);
    }

    if (file_exists(cname) == -1) {
        output("\n%s\n\n", MSG_NOTEXT);
        return 0;
    }
    if (textnum > 0) {
        push_unread(Current_conf, textnum);
        output("\n%s %ld %s\n\n", MSG_TEXTNAME, textnum, MSG_EXITUNREAD);
    }
    return 0;
}

/*
 * cmd_list_member - list members of conference
 * args: user arguments (args)
 * ret: ok (0) or error (-1)
 */

int
cmd_list_member(char *args)
{
    int fd, count, x, confnum;
    struct UEL *ue_list, *top;
    struct UEN *namelist, *topname;
    struct USER_ENTRY ue2;
    struct CONFS_ENTRY ce;
    LINE name, confsfile;
    char *buf, *oldbuf, *buf2, *oldbuf2, *confname;

    if (*args && args) {
        confname = expand_name(args, CONF, 0, NULL);
        if (!confname) {
            return 0;
        } else {
            confnum = conf_num(confname);
        }
    } else {
        confnum = Current_conf;
    }

    if (!confnum) {
        output("\n%s\n\n", MSG_ONLYMEMBER);
        return 0;
    }
    if ((fd = open_file(USER_FILE, 0)) == -1) {
        return -1;
    }
    if ((buf = read_file(fd)) == NULL) {
        return -1;
    }
    if (close_file(fd) == -1) {
        return -1;
    }
    oldbuf = buf;

    conf_name(confnum, name);
    output("\n%s %s:\n\n", MSG_FOLLMEM, name);

    ue_list = NULL;
    top = NULL;
    count = 0;
    while (buf) {
        buf = get_user_entry(buf, &ue2);
        if (buf) {
            user_dir(ue2.num, confsfile);
            strcat(confsfile, CONFS_FILE);

            if (file_exists(confsfile) != -1) {

                if ((fd = open_file(confsfile, 0)) == -1) {
                    return -1;
                }
                if ((buf2 = read_file(fd)) == NULL) {
                    return -1;
                }
                oldbuf2 = buf2;

                if (close_file(fd) == -1) {
                    return -1;
                }
                while (buf2) {
                    buf2 = get_confs_entry(buf2, &ce);
                    free_confs_entry(&ce);
                    if (ce.num == confnum)
                        break;
                }

                free(oldbuf2);
            } else {
                ce.num = -1;
            }

            if (ce.num == confnum) {

                if (ue_list) {
                    ue_list->next = (struct UEL *) malloc
                        (sizeof(struct UEL) + 1);
                    if (ue_list->next == NULL) {
                        sys_error("list_user", 1, "malloc");
                        return -1;
                    }
                    ue_list = ue_list->next;
                    user_name(ue2.num, ue_list->ue.name);
                    ue_list->next = NULL;
                } else {
                    ue_list = (struct UEL *) malloc
                        (sizeof(struct UEL) + 1);
                    if (ue_list == NULL) {
                        sys_error("list_user", 1, "malloc");
                        return -1;
                    }
                    top = ue_list;
                    user_name(ue2.num, ue_list->ue.name);
                    ue_list->next = NULL;
                }
                count++;
            }
        }
    }

    free(oldbuf);
    namelist = sort_user(top, count);
    if (namelist) {
        topname = namelist;
        for (x = 0; x < count; x++) {
            if (output("%s\n", namelist->name) == -1)
                break;
            namelist++;
        }
        free(topname);
    }
    output("\n");
    return 0;
}

/*
 * cmd_list_subj - list subjects in conference
 * args: user arguments (args)
 * ret: ok (0) or error (-1)
 */

int
cmd_list_subj(char *args)
{
    return (list_subj(args));
}

/*
 * cmd_change_cname - change the name of a conference
 * args: user arguments (args)
 * ret: ok (0) or error (-1)
 */

int
cmd_change_cname(char *args)
{
    int fd, c_num;
    char *buf, *tmpbuf, *origbuf, *expanded_name;
    struct CONF_ENTRY ce;
    LINE confname, oldname, newname;

    if ((args == NULL) || (!strlen(args))) {
        c_num = Current_conf;
        strcpy(args, conf_name(c_num, confname));
    } else {
        args = expand_name(args, CONF, 0, NULL);
        c_num = conf_num(args);
        if (c_num == -1) {
            return 0;
        }
    }
    if (!c_num) {
        output("\n%s\n\n", MSG_NOCHMBOX);
        return 0;
    }
    if (!is_conf_creator(Uid, c_num)) {
        output("\n%s %s.\n\n", MSG_NOTCREATOR, args);
        return 0;
    }
    conf_name(c_num, oldname);
    output("\n%s", MSG_NEWNAME);
    input(oldname, newname, LINE_LEN, 0, 0, 0);
    ltrim(newname);
    if (!strlen(newname)) {
        output("\n%s\n\n", MSG_NOCHNAME);
        return 0;
    }
    if (strcmp(oldname, newname) == 0) {
        output("\n%s\n\n", MSG_NOCHNAME);
        return 0;
    }
    if ((expanded_name = expand_name(newname, CONF, 1, NULL)) &&
        (strcmp(oldname, expanded_name) != 0)) {
        output("\n%s\n\n", MSG_ERRCNAME);
        return 0;
    }
    if ((fd = open_file(CONF_FILE, 0)) == -1) {
        return -1;
    }
    if ((buf = read_file(fd)) == NULL) {
        return -1;
    }
    origbuf = buf;
    ce.num = 0;
    while ((buf = get_conf_entry(buf, &ce)) != NULL) {
        if (ce.num == c_num) {
            break;
        }
    }
    if (ce.num != c_num) {
        output("\n%s\n\n", MSG_CONFMISSING);
        free(origbuf);
        close_file(fd);
        return -1;
    }
    strcpy(ce.name, newname);
    if ((tmpbuf = replace_conf(&ce, origbuf)) == NULL) {
        output("\n%s\n\n", MSG_CONFMISSING);
        return -1;
    }
    if (write_file(fd, tmpbuf) == -1) {
        return -1;
    }
    if (close_file(fd) == -1) {
        return -1;
    }
    output("\n%s %s.\n\n", MSG_PRESNAME, newname);
    return 0;
}

/*
 * cmd_change_comc - change the reply conference of a conference
 * args: user arguments (args)
 * ret: ok (0) or error (-1)
 */

int
cmd_change_comc(char *args)
{
    int fd, c_num, n_num;
    char *buf, *tmpbuf, *origbuf, *expanded_name;
    struct CONF_ENTRY ce, *ce2;
    LINE confname, oldname, newname;

    if ((args == NULL) || (!strlen(args))) {
        c_num = Current_conf;
        strcpy(args, conf_name(c_num, confname));
    } else {
        args = expand_name(args, CONF, 0, NULL);
        c_num = conf_num(args);
        if (c_num == -1) {
            return 0;
        }
    }
    if (!c_num) {
        output("\n%s\n\n", MSG_WRONGCONF);
        return 0;
    }
    if (!is_conf_creator(Uid, c_num)) {
        output("\n%s %s.\n\n", MSG_NOTCREATOR, args);
        return 0;
    }
    ce2 = get_conf_struct(c_num);
    if (ce2->comconf)
        conf_name(ce2->comconf, oldname);
    else
        conf_name(c_num, oldname);

    output("\n%s", MSG_NEWCOM);
    input(oldname, newname, LINE_LEN, 0, 0, 0);

    if (!strlen(newname)) {
        output("\n%s\n\n", MSG_NOCHNAME);
        return 0;
    }
    if (strcmp(oldname, newname) == 0) {
        output("\n%s\n\n", MSG_NOCHNAME);
        return 0;
    }
    if ((expanded_name = expand_name(newname, CONF, 1, NULL)) == NULL) {
        output("\n%s\n\n", MSG_WRONGCONF);
        return 0;
    }
    n_num = conf_num(expanded_name);
    if (n_num <= 0) {
        output("\n%s\n\n", MSG_WRONGCONF);
        return 0;
    }
    if (!is_conf_creator(Uid, c_num)) {
        output("\n%s %s.\n\n", MSG_NOTCREATOR, oldname);
        return 0;
    }
    if ((fd = open_file(CONF_FILE, 0)) == -1) {
        return -1;
    }
    if ((buf = read_file(fd)) == NULL) {
        return -1;
    }
    origbuf = buf;
    ce.num = 0;
    while ((buf = get_conf_entry(buf, &ce)) != NULL) {
        if (ce.num == c_num) {
            break;
        }
    }
    if (ce.num != c_num) {
        output("\n%s\n\n", MSG_CONFMISSING);
        free(origbuf);
        close_file(fd);
        return -1;
    }
    if (n_num == c_num)
        n_num = 0;
    ce.comconf = n_num;
    if ((tmpbuf = replace_conf(&ce, origbuf)) == NULL) {
        output("\n%s\n\n", MSG_CONFMISSING);
        return -1;
    }
    if (write_file(fd, tmpbuf) == -1) {
        return -1;
    }
    if (close_file(fd) == -1) {
        return -1;
    }
    output("\n%s %s.\n\n", MSG_COMCNAME, expanded_name);
    return 0;
}

/*
 * cmd_delete_conf - delete conference
 * args: user arguments (args)
 * ret: ok (0) or error (-1)
 */

int
cmd_delete_conf(char *args)
{
    int fd, c_num;
    char *buf, *origbuf, *tmpbuf, *mbuf, *oldmbuf;
    LINE confname, answer;
    LONG_LINE newname;
    struct CONF_ENTRY ce;

    if ((args == NULL) || (!strlen(args))) {
        c_num = Current_conf;
        strcpy(args, conf_name(c_num, confname));
    } else {
        args = expand_name(args, CONF, 0, NULL);
        c_num = conf_num(args);
        if (c_num == -1) {
            return 0;
        }
    }
    if (!c_num) {
        output("\n%s\n\n", MSG_NODELMBOX);
        return 0;
    }
    if (!is_conf_creator(Uid, c_num)) {
        output("\n%s %s.\n\n", MSG_NOTCREATOR, args);
        return 0;
    }
    if ((fd = open_file(CONF_FILE, 0)) == -1) {
        return -1;
    }
    if ((mbuf = read_file(fd)) == NULL) {
        return -1;
    }
    oldmbuf = mbuf;

    if (close_file(fd) == -1) {
        return -1;
    }
    while ((mbuf = get_conf_entry(mbuf, &ce)) != NULL) {
        if (ce.comconf == c_num)
            break;
    }

    free(oldmbuf);

    if (ce.comconf == c_num) {
        output("\n%s\n\n", MSG_COMEXT);
        return 0;
    }
    output("\n%s %s? ", MSG_DELCONF, args);
    input("", answer, 4, 0, 0, 0);
    down_string(answer);
    if (*answer && (answer[0] == MSG_YESANSWER)) {
        strcpy(confname, args);
        cmd_unsubscribe(confname);
        if ((fd = open_file(CONF_FILE, 0)) == -1) {
            return -1;
        }
        if ((buf = read_file(fd)) == NULL) {
            return -1;
        }
        origbuf = buf;
        ce.num = 0;
        while ((buf = get_conf_entry(buf, &ce)) != NULL) {
            if (ce.num == c_num) {
                break;
            }
        }
        if (ce.num != c_num) {
            output("\n%s\n\n", MSG_CONFMISSING);
            free(origbuf);
            close_file(fd);
            return -1;
        }
        snprintf(newname, sizeof(newname), "%s %d (%s) (%s)", MSG_CONFCALL, c_num,
            ce.name, MSG_DELETED2);
        strncpy(ce.name, newname, (LINE_LEN - 1));
        ce.name[LINE_LEN - 1] = 0;
        ce.time = 0;
        ce.type = 2;
        ce.creator = -1;
        if ((tmpbuf = replace_conf(&ce, origbuf)) == NULL) {
            output("\n%s\n\n", MSG_CONFMISSING);
            return -1;
        }
        critical();
        if (write_file(fd, tmpbuf) == -1) {
            return -1;
        }
        if (close_file(fd) == -1) {
            return -1;
        }
        snprintf(newname, sizeof(newname), "%s/%d%s", SKLAFF_DB, ce.num, CONFRC_FILE);
        if ((fd = open_file(newname, 0)) == -1)
            return -1;
        if ((buf = read_file(fd)) == NULL)
            return -1;
        strcpy(buf, "");
        if (write_file(fd, buf) == -1)
            return -1;
        if (close_file(fd) == -1)
            return -1;
        non_critical();
        output("%s %s %s\n\n", MSG_CONFNAME, confname, MSG_PURGED);
    } else {
        output("\n");
    }
    return 0;
}

/*
 * cmd_list_flags - list current flags and status
 * args: user arguments (args)
 * ret: ok (0) or error (-1)
 */

int
cmd_list_flags(char *args)
{
    if (args && *args) {
        output("\n%s\n\n", MSG_NOARG);
        return 0;
    }
    output("\n");
    out_onoff(Utf8);
    output("%s\n", MSG_FLAG19F); /* Catching up with modern times ;) - utf8 is now fully supported 2025-08-11 PL */
    out_onoff(Ibm);
    output("%s\n", MSG_FLAG0F);
    out_onoff(Iso8859);
    output("%s\n", MSG_FLAG1F);
    out_onoff(Mac);
    output("%s\n", MSG_FLAG2F);
    out_onoff(Present);
    output("%s\n", MSG_FLAG3F);
    out_onoff(Shout);
    output("%s\n", MSG_FLAG4F);
    out_onoff(End_default);
    output("%s\n", MSG_FLAG5F);
    out_onoff(Say);
    output("%s\n", MSG_FLAG6F);
    out_onoff(Subject_change);
    output("%s\n", MSG_FLAG7F);
    out_onoff(Space);
    output("%s\n", MSG_FLAG8F);
    out_onoff(Copy);
    output("%s\n", MSG_FLAG9F);
    out_onoff(Author);
    output("%s\n", MSG_FLAG10F);
    out_onoff(Date);
    output("%s\n", MSG_FLAG11F);
    out_onoff(Beep);
    output("%s\n", MSG_FLAG12F);
    out_onoff(Clear);
    output("%s\n", MSG_FLAG13F);
    out_onoff(Header);
    output("%s\n", MSG_FLAG14F);
    out_onoff(Special);
    output("%s\n", MSG_FLAG15F);
    out_onoff(Presbeep);
    output("%s\n", MSG_FLAG16F);
    out_onoff(Old_who);
    output("%s\n", MSG_FLAG17F);
    out_onoff(Ansi_output); /* We now have an ANSI flag PL 2025 */
    output("%s\n\n", MSG_FLAG18F);
    return 0;
}

/*
 * cmd_on_flag - turn on flag
 * args: user arguments (args)
 * ret: ok (0) or error (-1)
 */

int
cmd_on_flag(char *args)
{
    turn_flag(1, args);
    return 0;
}

/*
 * cmd_off_flag - turn off flag
 * args: user arguments (args)
 * ret: ok (0) or error (-1)
 */

int
cmd_off_flag(char *args)
{
    turn_flag(0, args);
    return 0;
}

/*
 * cmd_info - show system information file
 * args: user arguments (args)
 * ret: ok (0) or error (-1)
 */

int
cmd_info(char *args)
{
    int fd;
    char *buf;

    output("\n");
    if (file_exists(INFO_FILE)) {
        output("%s\n", MSG_NOINFO);
    } else {
        if ((fd = open_file(INFO_FILE, 0)) == -1) {
            sys_error("cmd_info", 1, "open_file");
            return -1;
        }
        if ((buf = read_file(fd)) == NULL) {
            sys_error("cmd_info", 2, "read_file");
            return -1;
        }
        if (close_file(fd) == -1) {
            sys_error("cmd_info", 3, "close_file");
            return -1;
        }
        output(buf);
    }
    output("\n");
    return 0;
}

/* TEMP: verbose help debugging; comment out to silence */
//#define HELP_DEBUG 1

/*
 * cmd_long_help - show help about a specific command
 * args: user arguments (args)
 * ret: ok (0) or error (-1)
 */

/* (hopefully repaired 2025-08-21 to work with commands with more than one underscore) /PL */
int
cmd_long_help(char *args)
{
    static LINE tmp;         /* derived basename (e.g., "mod_numlines") */
    static LONG_LINE fname;  /* full path */
    char *buf;
    int (*fcn) (), i, fd;

    if (args && *args) {
        fcn = parse(args, tmp);  /* tmp will hold normalized command text for parse */
        if (fcn) {
            for (i = 0; Par_ent[i].func[0]; i++) {
                if (fcn == Par_ent[i].addr) {
                    /* Derive help filename = function name without "cmd_" prefix as per docs. */
                    const char *sym = Par_ent[i].func;
                    const char *p = strstr(sym, "cmd_");
                    if (p) {
                        p += 4; /* skip "cmd_" */
                        strcpy(tmp, p);
                    } else {
                        /* fallback: if there is an underscore, use everything after the first '_' */
                        const char *us = strchr(sym, '_');
                        if (us && us[1]) strcpy(tmp, us + 1);
                        else strcpy(tmp, sym);
                    }
                    snprintf(fname, sizeof(fname), "%s/%s", HELP_DIR, tmp);

#ifdef HELP_DEBUG
                    output("[HELP-DEBUG] func=\"%s\" args=\"%s\" base=\"%s\" path=\"%s\"\n",
                           sym, (args ? args : ""), tmp, fname);
#endif

                    /* file_exists() returns -1 when NOT found in rest of codebase */
                    if (file_exists(fname) == -1) {
#ifdef HELP_DEBUG
                        output("[HELP-DEBUG] not found: %s\n", fname);
#endif
                        output("\n%s\n\n", MSG_NOHELP);
                    } else {
                        if ((fd = open_file(fname, 0)) == -1) {
#ifdef HELP_DEBUG
                            output("[HELP-DEBUG] open_file() failed for %s\n", fname);
#endif
                            sys_error("cmd_long_help", 1, "open_file");
                            return -1;
                        }
                        if ((buf = read_file(fd)) == NULL) {
#ifdef HELP_DEBUG
                            output("[HELP-DEBUG] read_file() returned NULL for %s\n", fname);
#endif
                            sys_error("cmd_long_help", 2, "read_file");
                            /* try to close before bailing */
                            (void) close_file(fd);
                            return -1;
                        }
                        if (close_file(fd) == -1) {
#ifdef HELP_DEBUG
                            output("[HELP-DEBUG] close_file() failed for %s\n", fname);
#endif
                            sys_error("cmd_long_help", 3, "close_file");
                            /* continue; we still have buf */
                        }

                        output("\n%s\n", MSG_COMMAND);
                        for (i = 0; Par_ent[i].func[0]; i++) {
                            if (fcn == Par_ent[i].addr)
                                output("  %s\n", Par_ent[i].cmd);
                        }
                        output("\n%s\n", buf);
                        free(buf);
                    }
                    break;
                }
            }
        } else {
#ifdef HELP_DEBUG
            output("[HELP-DEBUG] parse() did not resolve command: \"%s\"\n", args);
#endif
            output("\n%s\n\n", MSG_NOHELP);
        }
    } else {
        /* No args: general help */
#ifdef HELP_DEBUG
        output("[HELP-DEBUG] no args, checking HELP_FILE: %s\n", HELP_FILE);
#endif
        if (file_exists(HELP_FILE) == -1) {
#ifdef HELP_DEBUG
            output("[HELP-DEBUG] HELP_FILE missing, falling back to cmd_help()\n");
#endif
            cmd_help(args);
        } else {
            int fd;
            if ((fd = open_file(HELP_FILE, 0)) == -1) {
#ifdef HELP_DEBUG
                output("[HELP-DEBUG] open_file() failed for HELP_FILE\n");
#endif
                sys_error("cmd_long_help", 1, "open_file");
                return -1;
            }
            if ((buf = read_file(fd)) == NULL) {
#ifdef HELP_DEBUG
                output("[HELP-DEBUG] read_file() returned NULL for HELP_FILE\n");
#endif
                sys_error("cmd_long_help", 2, "read_file");
                (void) close_file(fd);
                return -1;
            }
            if (close_file(fd) == -1) {
#ifdef HELP_DEBUG
                output("[HELP-DEBUG] close_file() failed for HELP_FILE\n");
#endif
                sys_error("cmd_long_help", 3, "close_file");
                /* continue; we still have buf */
            }
            output("\n%s\n", buf);
            free(buf);
        }
    }
    return 0;
}

/*
 * cmd_change_passwd - change password for user
 * args: user arguments (args)
 * ret: ok (0) or error (-1)
 */

int
cmd_change_passwd(char *args)
{
    sigset_t sigmask, oldsigmask;

    Change_prompt = 1;
    if (strlen(args)) {
        output("\n%s\n\n", MSG_NOARG);
        return 0;
    }
    output("\n");
    tty_reset();
    sigemptyset(&sigmask);
    sigaddset(&sigmask, SIGNAL_NEW_TEXT);
    sigaddset(&sigmask, SIGNAL_NEW_MSG);
    sigprocmask(SIG_BLOCK, &sigmask, &oldsigmask);
    if (fork()) {
        (void) wait(NULL);
    } else {
        execl(SKLAFFPASSWD, SKLAFFPASSWD, (char *) 0);
    }
    tty_raw();
    sigprocmask(SIG_UNBLOCK, &oldsigmask, NULL);
    output("\n");
    return 0;
}

/*
 * cmd_cls - clear screen
 * args: user arguments (args)
 * ret: ok (0) or error (-1)
 */

int
cmd_cls(char *args)
{
    printf("%c[H%c[J", 27, 27);
    fflush(stdout);
    Lines = 1;
    return 0;
}

/*
 * cmd_grep - search for a string in conference
 * args: user arguments (args)
 * ret: ok (0) or error (-1)
 */

int
cmd_grep(char *args)
{
    LINE search;

    Change_prompt = 1;
    if (!args || (*args == '\0')) {
        output("\n%s", MSG_GREPPROMPT);
        input("", search, 70, 0, 0, 0);
    } else {
        strcpy(search, args);
    }

    if (*search == '\0') {
        output("\n%s\n\n", MSG_BADGREP);
        return 0;
    }
    if (!grep(Current_conf, search)) {
        output("\n%s\n", MSG_GREPNFOUND);
    }
    output("\n");
    return 0;
}

/*
 * cmd_global_grep - search for a string in all conferences
 * args: user arguments (args)
 * ret: ok (0) or error (-1)
 */

int
cmd_global_grep(char *args)
{
    LINE search, confname, confsname;
    int fd, length, res, foundany, cnum;
    struct CONFS_ENTRY cse;
    char *buf, *oldbuf;

    Change_prompt = 1;
    if (!args || (*args == '\0')) {
        output("\n%s", MSG_GREPPROMPT);
        input("", search, 70, 0, 0, 0);
    } else {
        strcpy(search, args);
    }

    if (*search == '\0') {
        output("\n%s\n\n", MSG_BADGREP);
        return 0;
    }
    strcpy(confsname, Home);
    strcat(confsname, CONFS_FILE);

    if ((fd = open_file(confsname, 0)) == -1) {
        return -1;
    }
    if ((buf = read_file(fd)) == NULL) {
        return -1;
    }
    oldbuf = buf;

    if (close_file(fd) == -1) {
        free(oldbuf);
        return -1;
    }
    res = 0;
    length = 0;
    foundany = 0;
    output("\n");
    while ((buf = get_confs_entry(buf, &cse)) != NULL) {
        conf_name(cse.num, confname);
        cnum = cse.num;         /* Added. OR 12/8/99 */
        free_confs_entry(&cse);
        if (buf) {
            while (length != 0) {
                output("\b \b");
                length--;
            }
/* conf_name(cse.num, confname); Moved up before the free. OR / 12/8/99 */
            output("%s", confname);
            fflush(stdout);
            /* res = grep(cse.num, search); Called after free. No good. OR */
            res = grep(cnum, search);
            if (!res) {
                length = strlen(confname);
            } else {
                foundany = 1;
                output("\n");
                if (res == 2) {
                    break;
                }
            }
        }
    }

    if (foundany) {
        if (res == 0) {
            while (length != 0) {
                output("\b \b");
                length--;
            }
        }
    } else {
        while (length != 0) {
            output("\b \b");
            length--;
        }
        output("%s\n\n", MSG_GGREPNFOUND);
    }
    free(oldbuf);
    return 0;
}

/*
 * cmd_licens - display GNU license
 * args: user arguments (args)
 * ret: ok (0) or error (-1)
 */

int
cmd_licens(char *args)
{
    int fd;
    char *buf;

    if ((fd = open_file(LICENS_FILE, OPEN_QUIET)) == -1) {
        return 0;
    }
    if ((buf = read_file(fd)) == NULL) {
        return 0;
    }
    if (close_file(fd) == -1) {
        return 0;
    }
    output("\n%s", buf);
    output("\n");
    free(buf);
    return 0;
}

/*
 * cmd_upload - uploads file(s)
 * args: user arguments (args)
 * ret: ok (0) or error (-1)
 */

int
cmd_upload(char *args)
{
    LINE cwd;
    LONG_LINE filed;
    int fd;

    Change_msg = 1;
    Change_prompt = 1;
    if (!Current_conf) {
        output("\n%s\n\n", MSG_NOTINMBOX);
        return 0;
    }
    set_avail(Uid, 1);
    output("\n");
    if (getcwd(cwd, LINE_LEN) == NULL) {
    perror("getcwd"); /* Keeps grumpy compiler happy on Linux PL 2025-07-25 */
    return 0;
    }
    signal(SIGNAL_NEW_TEXT, SIG_IGN);
    signal(SIGNAL_NEW_MSG, SIG_IGN);
    snprintf(filed, sizeof(filed), "%s/%d", FILE_DB, Current_conf);
    /* Ensure upload dir exists (PL 2025-09-15) */
    if (access(filed, F_OK) == -1) {
     /* fprintf(stderr, "Directory %s does not exist, creating it...\n", filed);*/ 	/* debug output */
        if (mkdir(filed, 0775) == -1) {
            perror("mkdir failed"); /* mkdir may fail if permission denied */
            return -1;
        }
        if (chown(filed, getuid(), getgid()) == -1) {
            perror("chown failed"); /* optional but helpful if ownership matters */
            /* not fatal, so we don't return */
        }
    }
	if (chdir(filed) == -1) {
    perror("chdir"); /* Keeps grumpy compiler happy on Linux PL 2025-07-25 */
    return 0;
    }
    if (fork()) {
        (void) wait(&fd);
    } else {
        sig_reset();

       

/* fprintf(stderr, "Executing: %s %s\n\n\n", UPLOADPRGM, ULOPT1);*/   	/* debugging */
/* fprintf(stderr, "EUID: %d\n", getuid());*/							/* Debug: Check the effective user ID */

    char *args[] = {UPLOADPRGM, ULOPT1, NULL};  			/* Added by PL 2025-07-17 */
    execvp(UPLOADPRGM, args); 						/* Replaces execl() that was broken. PL 2025-07-17 */
    



    }
    if (chdir(cwd) == -1) {
    perror("chdir"); /* Keeps grumpy compiler happy on Linux PL 2025-07-25 */
    return 0;
    }
    signal(SIGNAL_NEW_TEXT, baffo);
    signal(SIGNAL_NEW_MSG, newmsg);
 /* rebuild_index_file();*/ 	/* Replaced with new code with error checking below, PL 2025-07-17 */
    if (rebuild_index_file() == -1) {
    fprintf(stderr, "Failed to rebuild index file.\n");
    return -1;
}
    /* Temporary "fix", soon we will instead call cmd_describe here */
    output("Tackar f|r det! \nBeskriv nu g{rna filen genom att ge kommandot 'Beskriv <filnamn>'.\n\n");
    set_avail(Uid, 0);
    return 0;
}

/*
 * cmd_download - downloads file(s)
 * args: user arguments (args)
 * ret: ok (0) or error (-1)
 */

int
cmd_download(char *args)
{
    LINE fname, cwd;
    LONG_LINE filed;
    sigset_t sigmask, oldsigmask;

    Change_msg = 1;
    Change_prompt = 1;
    if (!Current_conf) {
        output("\n%s\n\n", MSG_NOTINMBOX);
        return 0;
    }
    output("\n");

    if (*args == '\0') {
        output(MSG_FILENAME);
        input("", fname, LINE_LEN, 0, 0, 0);
        output("\n");
    } else
        strcpy(fname, args);

    if (*fname == '\0') {
        return 0;
    }
    if (strchr(fname, '/') || strchr(fname, ';') || strchr(fname, ':')
        || strchr(fname, '<') || strchr(fname, '>') || strchr(fname, '?')
        || strchr(fname, '*') || strchr(fname, '|') || strchr(fname, '&')) {
        output("%s\n\n", MSG_BADFNAME);
        return 0;
    }
    if (getcwd(cwd, LINE_LEN) == NULL) {
    perror("getcwd"); /* Keeps grumpy compiler happy on Linux PL 2025-07-25 */
    return 0;
    }

    sigemptyset(&sigmask);
    sigaddset(&sigmask, SIGNAL_NEW_TEXT);
    sigaddset(&sigmask, SIGNAL_NEW_MSG);
    sigprocmask(SIG_BLOCK, &sigmask, &oldsigmask);
    signal(SIGNAL_NEW_TEXT, SIG_IGN);
    signal(SIGNAL_NEW_MSG, SIG_IGN);
    set_avail(Uid, 1);
    snprintf(filed, sizeof(filed), "%s/%d", FILE_DB, Current_conf);
    if (chdir(filed) == -1) {
    perror("chdir"); /* Keeps grumpy compiler happy on Linux PL 2025-07-25 */
    return 0;
    }
    if (!fork()) {
        sig_reset();        
 /* fprintf(stderr, "Executing: %s %s\n\n\n", DOWNLOADPRGM, DLOPT1, DLOPT2, DLOPT3); */ 	/* debugging */
 /* execl(DOWNLOADPRGM, DOWNLOADPRGM, DLOPT1, DLOPT2, DLOPT3, cmdline, NULL); */ 		/* DID NOT WORK IN 2025 */      
 /* fprintf(stderr, "EUID: %d\n", getuid()); */                         			/* Debug: Check the effective user ID */      
 /* Build argument list dynamically, omitting empty args */ 	/* 2025-07-24 PL */
    char *args_exec[10];					/* 2025-07-24 PL */
    int i = 0;							/* 2025-07-24 PL */
    args_exec[i++] = DOWNLOADPRGM;				/* 2025-07-24 PL */
    if (DLOPT1[0]) args_exec[i++] = DLOPT1;			/* 2025-07-24 PL */
    if (DLOPT2[0]) args_exec[i++] = DLOPT2;			/* 2025-07-24 PL */
    if (DLOPT3[0]) args_exec[i++] = DLOPT3;			/* 2025-07-24 PL */
    args_exec[i++] = fname;					/* 2025-07-24 PL */
    args_exec[i]   = NULL;					/* 2025-07-24 PL */
    execvp(DOWNLOADPRGM, args_exec); /* execl() replacement. PL 2025-07-24 */
    exit(1);  // if exec fails  
 } else {
        wait(NULL);
    }
    tty_raw();
    signal(SIGNAL_NEW_TEXT, baffo);
    signal(SIGNAL_NEW_MSG, newmsg);
    sigprocmask(SIG_UNBLOCK, &oldsigmask, NULL);
    if (chdir(cwd) == -1) {
    perror("chdir"); /* Keeps grumpy compiler happy on Linux PL 2025-07-25 */
    return 0;
    }
    tty_raw();
    output("\n");
    set_avail(Uid, 0);
    return 0;
}

/*
 * cmd_list_files - lists files in current conference
 * args: user arguments (args)
 * ret: ok (0) or error (-1)
 */

int
cmd_list_files(char *args)
{
    LONG_LINE fn;
    struct FILE_ENTRY fe;
    char *buf, *oldbuf;
    struct stat fs;
    int fd;

    if (!Current_conf) {
        output("\n%s\n\n", MSG_NOTINMBOX);
        return 0;
    }
     /* New code 2025-07-26 to help sklaffkom find the .index file */
	snprintf(fn, sizeof(fn), "%s/%d/.index", FILE_DB, Current_conf);	/* Better way to ensure we have the correct path */
     /* fprintf(stderr, "[DEBUG] Checking .index file at path: %s\n", fn);*/ 	/* Directory debugging */
    if ((fd = open_file(fn, OPEN_QUIET)) == -1) {
        output("\n%s\n\n", MSG_NOFILES);
        return 0;
    }
    if ((buf = read_file(fd)) == NULL) {
        return -1;
    }
    oldbuf = buf;

    close_file(fd);

    if (strlen(buf) < 2) {
        output("\n%s\n\n", MSG_NOFILES);
        return 0;
    }
    output("\n%s\n%s\n", MSG_FILEHEAD, MSG_LINEROW);

    while (buf != NULL) {
        buf = get_file_entry(buf, &fe);
        if (buf) {
            snprintf(fn, sizeof(fn), "%s/%d/%s", FILE_DB, Current_conf, fe.name);
            if (stat(fn, &fs) == -1) {
                fs.st_size = 0;
            }
            if (output("%-20s  %7d  %s\n", fe.name, (int)fs.st_size, safe_str(fe.desc)) == -1)
	       break;
        }
    }

    free(oldbuf);
    output("\n");
    return 0;

}

/*
 * cmd_describe - describes a file in current conference
 * args: user arguments (args)
 * ret: ok (0) or error (-1)
 */

int
cmd_describe(char *args)
{
    LINE fname;
    char fn[512];
    int fd, i;
    char *buf, *oldbuf, *nbuf, *ptr;
    struct FILE_ENTRY fe;

    if (!Current_conf) {
        output("\n%s\n\n", MSG_NOTINMBOX);
        return 0;
    }
    output("\n");

    if (*args == '\0') {
        output(MSG_FILENAME);
        input("", fname, LINE_LEN, 0, 0, 0);
        output("\n");
    } else
        strcpy(fname, args);

    if (*fname == '\0') {
        return 0;
    }
    if (strchr(fname, '/') || strchr(fname, '*') || strchr(fname, '[')
        || strchr(fname, ']') || strchr(fname, '?')) {
        output("%s\n\n", MSG_BADFNAME);
        return 0;
    }
    snprintf(fn, sizeof(fn), "%s/%d/%s", FILE_DB, Current_conf, fname);
    if (file_exists(fn) == -1) {
        output("%s\n\n", MSG_BADFILE);
        return 0;
    }
    snprintf(fn, sizeof(fn), "%s/%d%s", FILE_DB, Current_conf, INDEX_FILE);
    if ((fd = open_file(fn, 0)) == -1) {
        return -1;
    }
    if ((buf = read_file(fd)) == NULL) {
        return -1;
    }
    oldbuf = buf;

    close_file(fd);

    while (buf != 0) {
        buf = get_file_entry(buf, &fe);
        if (buf) {
            if (!strcmp(fname, fe.name)) {
                break;
            }
        }
    }

    if (!buf) {                 /* Should never happen! */
        output("%s\n\n", MSG_BADFILE);
        free(oldbuf);
        return 0;
    }
    free(oldbuf);

    output("%s", MSG_DESCRIBE);
    input(fe.desc, fe.desc, 47, 0, 0, 0);
    output("\n");
    if (!strlen(fe.desc)) {
        return 0;
    }
    strcat(fname, ":");

    if ((fd = open_file(fn, 0)) == -1) {
        return -1;
    }
    if ((buf = read_file(fd)) == NULL) {
        return -1;
    }
    oldbuf = buf;

    i = strlen(oldbuf) + LINE_LEN;
    if ((nbuf = malloc(i)) == NULL) {
        sys_error("cmd_describe", 1, "malloc");
        return -1;
    }
    memset(nbuf, 0, i);

    ptr = strstr(buf, fname);
    buf = ptr;
    *ptr = 0;
    strcpy(nbuf, oldbuf);
    snprintf(fn, sizeof(fn), "%s%s\n", fname, fe.desc);
    strcat(nbuf, fn);
    buf++;
    ptr = strchr(buf, '\n');
    ptr++;
    strcat(nbuf, ptr);
    if (write_file(fd, nbuf) == -1) {
        return -1;
    }
    close_file(fd);
    free(oldbuf);
    return 0;
}

/*
 * cmd_unlink - unlinks a file from current conference
 * args: user arguments (args)
 * ret: ok (0) or error (-1)
 */

int
cmd_unlink(char *args)
{
    LINE fname;
    char fn[512];

    if (!Current_conf) {
        output("\n%s\n\n", MSG_NOTINMBOX);
        return 0;
    }
    output("\n");

    if (*args == '\0') {
        output(MSG_FILENAME);
        input("", fname, LINE_LEN, 0, 0, 0);
        output("\n");
    } else
        strcpy(fname, args);

    if (*fname == '\0') {
        output("\n");
        return 0;
    }
    if (strchr(fname, '/') || strchr(fname, '*') || strchr(fname, '[')
        || strchr(fname, ']') || strchr(fname, '?')) {
        output("%s\n\n", MSG_BADFNAME);
        return 0;
    }
    snprintf(fn, sizeof(fn), "%s/%d/%s", FILE_DB, Current_conf, fname);
    if (file_exists(fn) == -1) {
        output("%s\n\n", MSG_BADFILE);
        return 0;
    }
    unlink(fn);
    rebuild_index_file();
    output("%s %s\n\n", fname, MSG_DELETED);
    return 0;
}

/*
 * cmd_prio - puts conference first in conference list
 * args: user arguments (args)
 * ret: ok (0) or error (-1)
 */

int
cmd_prio(char *args)
{
    LINE confname;
    LONG_LINE confsname;
    struct CONFS_ENTRY ce;
    char *exp_confname, *buf, *oldbuf, *nbuf, *tmpbuf, saved;
    int conf, fd, i;

    if (args && *args) {
        strcpy(confname, args);
    } else {
        output("\n%s\n\n", MSG_NOCONFNAME);
        return 0;
    }
    exp_confname = expand_name(confname, SUBSCRIBED, 0, NULL);
    if (exp_confname) {
        conf = conf_num(exp_confname);
        if (!conf) {
            output("\n%s\n\n", MSG_NOPRIOMBOX);
            return 0;
        }
        strcpy(confsname, Home);
        strcat(confsname, CONFS_FILE);
        if ((fd = open_file(confsname, 0)) == -1)
            return -1;
        if ((buf = read_file(fd)) == NULL)
            return -1;
        oldbuf = buf;
        i = strlen(buf) + 1;
        nbuf = (char *) malloc(i);
        if (!nbuf) {
            sys_error("cmd_prio", 1, "malloc");
            return -1;
        }
        memset(nbuf, 0, i);
        buf = get_confs_entry(buf, &ce);
        free_confs_entry(&ce);
        saved = *buf;
        *buf = '\0';
        strcpy(nbuf, oldbuf);
        *buf = saved;
        buf = get_confs_entry(buf, &ce);
        free_confs_entry(&ce);
        while (buf != NULL) {
            if (ce.num == conf) {
                tmpbuf = buf;
                saved = *buf;
                *buf = '\0';
                tmpbuf--;
                while ((tmpbuf > oldbuf) && (*tmpbuf == '\n'))
                    tmpbuf--;
                while ((tmpbuf > oldbuf) && (*tmpbuf != '\n'))
                    tmpbuf--;
                if (tmpbuf > oldbuf)
                    tmpbuf++;
                strcat(nbuf, tmpbuf);
                *buf = saved;
            }
            buf = get_confs_entry(buf, &ce);
        }
        buf = oldbuf;
        buf = get_confs_entry(buf, &ce);
        free_confs_entry(&ce);
        while (buf != NULL) {
            if (ce.num != conf && ce.num) {
                tmpbuf = buf;
                saved = *buf;
                *buf = '\0';
                tmpbuf--;
                while ((tmpbuf > oldbuf) && (*tmpbuf == '\n'))
                    tmpbuf--;
                while ((tmpbuf > oldbuf) && (*tmpbuf != '\n'))
                    tmpbuf--;
                if (tmpbuf > oldbuf)
                    tmpbuf++;
                strcat(nbuf, tmpbuf);
                *buf = saved;
            }
            buf = get_confs_entry(buf, &ce);
            free_confs_entry(&ce);
        }
        free(oldbuf);
        critical();
        if (write_file(fd, nbuf) == -1)
            return -1;
        if (close_file(fd) == -1)
            return -1;
        non_critical();
        output("\n%s %s %s\n\n", MSG_CONFNAME, exp_confname, MSG_PRIO);
    }
    return 0;
}

/*
 * cmd_deprio - puts conference last in conference list
 * args: user arguments (args)
 * ret: ok (0) or error (-1)
 */

int
cmd_deprio(char *args)
{
    LINE confname;
    LONG_LINE confsname;
    struct CONFS_ENTRY ce;
    char *exp_confname, *buf, *oldbuf, *nbuf, *tmpbuf, saved;
    int conf, fd, i;

    if (args && *args) {
        strcpy(confname, args);
    } else {
        output("\n%s\n\n", MSG_NOCONFNAME);
        return 0;
    }
    exp_confname = expand_name(confname, SUBSCRIBED, 0, NULL);
    if (exp_confname) {
        conf = conf_num(exp_confname);
        if (!conf) {
            output("\n%s\n\n", MSG_NOPRIOMBOX);
            return 0;
        }
        strcpy(confsname, Home);
        strcat(confsname, CONFS_FILE);
        if ((fd = open_file(confsname, 0)) == -1)
            return -1;
        if ((buf = read_file(fd)) == NULL)
            return -1;
        oldbuf = buf;
        i = strlen(buf) + 1;
        nbuf = (char *) malloc(i);
        if (!nbuf) {
            sys_error("cmd_prio", 1, "malloc");
            return -1;
        }
        memset(nbuf, 0, i);
        buf = get_confs_entry(buf, &ce);
        free_confs_entry(&ce);
        saved = *buf;
        *buf = '\0';
        strcpy(nbuf, oldbuf);
        *buf = saved;
        buf = get_confs_entry(buf, &ce);
        free_confs_entry(&ce);
        while (buf != NULL) {
            if (ce.num != conf && ce.num) {
                tmpbuf = buf;
                saved = *buf;
                *buf = '\0';
                tmpbuf--;
                while ((tmpbuf > oldbuf) && (*tmpbuf == '\n'))
                    tmpbuf--;
                while ((tmpbuf > oldbuf) && (*tmpbuf != '\n'))
                    tmpbuf--;
                if (tmpbuf > oldbuf)
                    tmpbuf++;
                strcat(nbuf, tmpbuf);
                *buf = saved;
            }
            buf = get_confs_entry(buf, &ce);
            free_confs_entry(&ce);
        }
        buf = oldbuf;
        buf = get_confs_entry(buf, &ce);
        free_confs_entry(&ce);
        while (buf != NULL) {
            if (ce.num == conf) {
                tmpbuf = buf;
                saved = *buf;
                *buf = '\0';
                tmpbuf--;
                while ((tmpbuf > oldbuf) && (*tmpbuf == '\n'))
                    tmpbuf--;
                while ((tmpbuf > oldbuf) && (*tmpbuf != '\n'))
                    tmpbuf--;
                if (tmpbuf > oldbuf)
                    tmpbuf++;
                strcat(nbuf, tmpbuf);
                *buf = saved;
            }
            buf = get_confs_entry(buf, &ce);
            free_confs_entry(&ce);
        }
        free(oldbuf);
        critical();
        if (write_file(fd, nbuf) == -1)
            return -1;
        if (close_file(fd) == -1)
            return -1;
        non_critical();
        output("\n%s %s %s\n\n", MSG_CONFNAME, exp_confname, MSG_DEPRIO);
        return 0;
    }
    return 0;
}

/*
 * cmd_back_text - backs up <n> texts
 * args: user arguments (args)
 * ret: ok (0) or error (-1)
 */

int
cmd_back_text(char *args)
{
    long textnum, backed, numtexts;
    int conf;
    LINE fname;
    char *ptr;

    Change_prompt = 1;
    if (args && *args) {
        ptr = fname;
        numtexts = strtol(args, &ptr, 10);
        if ((ptr == args) || (numtexts <= 0)) {
            output("\n%s\n\n", MSG_ERRNUMT);
            return 0;
        }
        backed = 0;
        textnum = pop_read(&conf);
        while ((textnum != -1) && (numtexts > 0)) {
            backed++;
            numtexts--;
            mark_as_unread(textnum, conf);
            if (numtexts > 0)
                textnum = pop_read(&conf);
        }
        if (!backed)
            output("\n%s\n\n", MSG_BACK0);
        else if (backed == 1)
            output("\n%s\n", MSG_BACK1);
        else
            output("\n%ld %s\n", backed, MSG_BACK2);
        if (backed) {
            set_conf(conf);
            clear_comment();
            cmd_where(args);
        }
    } else
        output("\n%s\n\n", MSG_ERRNUMT);
    return 0;
}

/*
 * cmd_readall - reads all unread articles
 * args: user arguments (args)
 * ret: ok (0) or error (-1)
 */

int
cmd_readall(char *args)
{
    int oldlines, conf;
    long textnum, counter;

    Change_prompt = 1;
    Change_msg = 1;
    set_avail(Uid, 1);
    oldlines = Numlines;
    Numlines = 0;
    counter = 0;
    conf = 0;
    while (1) {
        textnum = pop_comment();
        if (textnum == -1)
            textnum = next_text(Current_conf);
        if (!textnum)
            conf = more_conf();
        if ((conf == -1) && (textnum <= 0))
            break;
        else if (textnum > 0) {
            display_text(Current_conf, textnum, 1, 0);
            mark_as_read(textnum, Current_conf);
            push_read(Current_conf, textnum);
            Current_text = textnum;
            Last_text = textnum;
            counter++;
        } else if (conf != -1) {
            set_conf(conf);
        }
    }
    if (!counter) {
        output("\n%s\n\n", MSG_NOTEXTLEFT);
    }
    Numlines = oldlines;
    Lines = 0;
    set_avail(Uid, 0);
    return 0;
}

/*
 * cmd_readsome - reads all unread articles in current conference
 * args: user arguments (args)
 * ret: ok (0) or error (-1)
 */

int
cmd_readsome(char *args)
{
    int oldlines;
    long textnum, counter;

    if (args && *args) {
        output("\n%s\n\n", MSG_NOARG);
        return 0;
    }
    Change_prompt = 1;
    Change_msg = 1;
    set_avail(Uid, 1);
    oldlines = Numlines;
    Numlines = 0;
    counter = 0;
    while (1) {
        textnum = pop_comment();
        if (textnum == -1)
            textnum = next_text(Current_conf);
        if (!textnum)
            break;
        if (textnum > 0) {
            display_text(Current_conf, textnum, 1, 0);
            mark_as_read(textnum, Current_conf);
            push_read(Current_conf, textnum);
            Current_text = textnum;
            Last_text = textnum;
            counter++;
        }
    }
    if (!counter) {
        output("\n%s\n\n", MSG_NOTEXTLEFT);
    }
    Numlines = oldlines;
    Lines = 0;
    set_avail(Uid, 0);
    return 0;
}

int
cmd_nethack(char *args)
{
    sigset_t sigmask, oldsigmask;
    char nethack_path[256] = DEFAULT_NETHACK_PATH; /* Now set in sklaff.h as it should be, 2025-09-24 PL */
    FILE *which_fp;
    char which_buf[256];

    Change_msg = 1;
    Change_prompt = 1;

    /* Check default path first */
    if (access(nethack_path, X_OK) != 0) {
        /* Try fallback using "which nethack" */
        which_fp = popen("which nethack", "r");
        if (which_fp && fgets(which_buf, sizeof(which_buf), which_fp)) {
            which_buf[strcspn(which_buf, "\n")] = '\0';  /* strip newline */
            if (access(which_buf, X_OK) == 0) {
                strncpy(nethack_path, which_buf, sizeof(nethack_path) - 1);
                nethack_path[sizeof(nethack_path) - 1] = '\0';
            }
        }
        if (which_fp)
            pclose(which_fp);
    }

    /* If still not found */
    if (access(nethack_path, X_OK) != 0) {
        output("\nFel: Kan inte starta Nethack - har SysOp glömt att installera den?\n");
        return 0;
    }

    sigemptyset(&sigmask);
    sigaddset(&sigmask, SIGNAL_NEW_TEXT);
    sigaddset(&sigmask, SIGNAL_NEW_MSG);
    sigprocmask(SIG_BLOCK, &sigmask, &oldsigmask);
    signal(SIGNAL_NEW_TEXT, SIG_IGN);
    signal(SIGNAL_NEW_MSG, SIG_IGN);
    set_avail(Uid, 1);

    if (!fork()) {
        sig_reset();
        tty_reset();
        execl(nethack_path, nethack_path, NULL);
        perror("execl");
        _exit(1);
    } else {
        wait(NULL);
    }

    signal(SIGNAL_NEW_TEXT, baffo);
    signal(SIGNAL_NEW_MSG, newmsg);
    sigprocmask(SIG_UNBLOCK, &oldsigmask, NULL);
    tty_raw();
    output("\n");
    set_avail(Uid, 0);

    return 0;
}

/*
 * cmd_unreadsub - unreads all texts with a certain subject
 * args: user arguments (args)
 * ret: ok (0) or error (-1)
 */

int
cmd_unreadsub(char *args)
{
    char *buf, *oldbuf, *ptr, *save;
    LONG_LINE cname;
    LINE sub;
    long count, last, first, max;
    int fd;
    struct TEXT_ENTRY te;

    if (args && *args)
        strcpy(sub, args);
    else
        strcpy(sub, Sub);
    if (!strlen(sub)) {
        output("\n%s\n\n", MSG_NOSUBJ);
        return 0;
    }
    max = 1000000;
    down_string(sub);
    if ((ptr = strchr(sub, ',')) != NULL) {
        save = ptr;
        ptr--;
        while (*ptr == ' ')
            ptr--;
        if ((*ptr >= '0') && (*ptr <= '9')) {
            while ((*ptr != ' ') && (ptr > sub))
                ptr--;
            max = atol(ptr);
            strcpy(sub, save + 1);
            ltrim(sub);
        }
    }
    count = 0;
    first = first_text(Current_conf, Uid);
    last = last_text(Current_conf, Uid);
    while (last > first) {
        if (Current_conf > 0)
            snprintf(cname, sizeof(cname), "%s/%d/%ld", SKLAFF_DB, Current_conf, last);
        else
            snprintf(cname, sizeof(cname), "%s/%ld", Mbox, last);
        if (file_exists(cname) != -1 && (fd = open_file(cname, 0)) != -1) {
            if ((buf = read_file(fd)) == NULL)
                return -1;
            oldbuf = buf;
            close_file(fd);
            buf = get_text_entry(buf, &te);
            free_text_entry(&te);
            free(oldbuf);
            down_string(te.th.subject);
            if (strstr(te.th.subject, sub)) {
                count = count + mark_as_unread(last, Current_conf);
                if (count == max)
                    break;
            }
        }
        last--;
    }

    if (count == 1) {
        output("\n%s\n\n", MSG_URONE);
        clear_comment();
        Change_prompt = 1;
    } else if (count == 0) {
        output("\n%s\n\n", MSG_URNONE);
    } else {
        output("\n%d %s\n\n", count, MSG_URTEXT);
        clear_comment();
        Change_prompt = 1;
    }
    return 0;
}

/*
 * cmd_jumpsub - mark as read all texts with a certain subject
 * args: user arguments (args)
 * ret: ok (0) or error (-1)
 */

int
cmd_jumpsub(char *args)
{
    struct UR_STACK *start, *ptr, *saved;
    char *buf, *oldbuf;
    LONG_LINE cname;
    LINE sub;
    long num, count;
    int fd;
    struct TEXT_ENTRY te;

    if (args && *args)
        strcpy(sub, args);
    else
        strcpy(sub, Sub);
    if (!strlen(sub)) {
        output("\n%s\n\n", MSG_NOSUBJ);
        return 0;
    }
    down_string(sub);
    count = 0;
    start = NULL;
    while ((num = next_text(Current_conf)) != 0) {
        if (Current_conf > 0)
            snprintf(cname, sizeof(cname), "%s/%d/%ld", SKLAFF_DB, Current_conf, num);
        else
            snprintf(cname, sizeof(cname), "%s/%ld", Mbox, num);
        if (file_exists(cname) != -1 && (fd = open_file(cname, 0)) != -1) {
            if ((buf = read_file(fd)) == NULL)
                return -1;
            oldbuf = buf;
            close_file(fd);
            buf = get_text_entry(buf, &te);
            free_text_entry(&te);
            free(oldbuf);
            down_string(te.th.subject);
            if (!strstr(te.th.subject, sub)) {
                if (start)
                    saved = ptr;
                ptr = (struct UR_STACK *) malloc(sizeof(struct UR_STACK));
                if (!start)
                    start = ptr;
                else
                    saved->next = ptr;
                ptr->num = num;
                ptr->conf = Current_conf;
                ptr->next = NULL;
            } else
                count++;
        }
        mark_as_read(num, Current_conf);
    }

    /* unread all texts again */

    ptr = start;
    while (ptr != NULL) {
        mark_as_unread(ptr->num, ptr->conf);
        ptr = ptr->next;
    }

    if (count == 1) {
        output("\n%s\n\n", MSG_JUMPONE);
        clear_comment();
        Change_prompt = 1;
    } else if (count == 0) {
        output("\n%s\n\n", MSG_JUMPNONE);
    } else {
        output("\n%d %s\n\n", count, MSG_JUMPTEXT);
        clear_comment();
        Change_prompt = 1;
    }
    return 0;
}

/*
 * cmd_jumpuser - mark as read all texts by a specified user
 * args: user arguments (args)
 * ret: ok (0) or error (-1)
 */

int
cmd_jumpuser(char *args)
{
    struct UR_STACK *start, *ptr, *saved;
    char *buf, *oldbuf;
    LONG_LINE cname;
    char *expname;
    long num, count;
    int fd, jumpuid;
    struct TEXT_ENTRY te;

    if (args && *args) {
        expname = expand_name(args, USER, 0, NULL);
        if (!expname) {
            return 0;
        }
        jumpuid = user_uid(expname);
    } else {
        if (Current_author != -1)
            jumpuid = Current_author;
        else {
            output("\n%s\n\n", MSG_NOUNAME);
            return 0;
        }
    }


    count = 0;
    start = NULL;
    while ((num = next_text(Current_conf)) != 0) {
        if (Current_conf > 0)
            snprintf(cname, sizeof(cname), "%s/%d/%ld", SKLAFF_DB, Current_conf, num);
        else
            snprintf(cname, sizeof(cname), "%s/%ld", Mbox, num);
        if (file_exists(cname) != -1 && (fd = open_file(cname, 0)) != -1) {
            if ((buf = read_file(fd)) == NULL)
                return -1;
            oldbuf = buf;
            close_file(fd);
            buf = get_text_entry(buf, &te);
            free_text_entry(&te);
            free(oldbuf);
            if (te.th.author != jumpuid) {
                if (start)
                    saved = ptr;
                ptr = (struct UR_STACK *) malloc(sizeof(struct UR_STACK));
                if (!start)
                    start = ptr;
                else
                    saved->next = ptr;
                ptr->num = num;
                ptr->conf = Current_conf;
                ptr->next = NULL;
            } else
                count++;
        }
        mark_as_read(num, Current_conf);
    }

    /* unread all texts again */

    ptr = start;
    while (ptr != NULL) {
        mark_as_unread(ptr->num, ptr->conf);
        ptr = ptr->next;
    }

    if (count == 1) {
        output("\n%s\n\n", MSG_JUMPONE);
        clear_comment();
        Change_prompt = 1;
    } else if (count == 0) {
        output("\n%s\n\n", MSG_JUMPNONE);
    } else {
        output("\n%d %s\n\n", count, MSG_JUMPTEXT);
        clear_comment();
        Change_prompt = 1;
    }
    return 0;
}


/*
 * cmd_answermsg - answer last say received
 * args: user arguments (args)
 * ret: ok (0) or error (-1)
 */

int
cmd_answermsg(char *args)
{
    LINE msg;
    int xit, uid;
    struct MSG_LIST *current;

    uid = 0;
    for (current = mlist; current; current = current->prev) {
        /* if (current->me.type == MSG_SAY && current->me.num != Uid) { */
        /* Modification needed after change in cmd_list_says, OR 2000-01-13 */
        if (current->me.type == MSG_SAY && current->me.direct == 0) {
            uid = current->me.num;
            break;
        }
    }
    if (uid == 0) {
        output("\n%s\n\n", MSG_LASTMSGERR);
        return 0;
    }
    output("\n");
    if (args && *args != '\0') {
        send_msg(uid, MSG_SAY, args, 1);
        output("\n");
    } else {
        xit = 0;
        do {
            display_msg(0);
            output(MSG_MSGPROMPT);
            Interrupt_input = 2;
            input("", msg, 65, 0, 1, 0);
            Interrupt_input = 0;
            rtrim(msg);
            if (strlen(msg)) {
                if (send_msg(uid, MSG_SAY, msg, 2) == -1) {
                    xit = 1;
                }
                output("\n");
            } else {
                xit = 1;
            }
        } while (!xit);
        output("\n");
    }
    strcpy(Overflow, "");
    return 0;
}

/*
 * cmd_I - inform all users of your status
 * args: user arguments (args)
 * ret: ok (0) or error (-1)
 */

int
cmd_I(char *args)
{
    rtrim(args);
    output("\n");
    if (strlen(args)) {
        send_msg_to_all(MSG_I, args);
        output("\n");
    }
    return 0;
}

/*
 * cmd_my - inform all users of yours status
 * args: user arguments (args)
 * ret: ok (0) or error (-1)
 */

int
cmd_my(char *args)
{
    rtrim(args);
    output("\n");
    if (strlen(args)) {
        send_msg_to_all(MSG_MY, args);
        output("\n");
    }
    return 0;
}


int
cmd_alias(char *args)
{
    static LINE tmp, cmd, tmp2;
    int (*fcn) (), i, j;

    for (i = 0; (args[i]) && (args[i] != ','); i++);

    strncpy(tmp, args, i);
    tmp[i] = 0;
    rtrim(tmp);
    if (!strlen(tmp)) {
        output("\n%s\n\n", MSG_NOCMD);
        return 0;
    }
    if (args[i] != ',') {
        output("\n%s\n\n", MSG_NOALIAS);
        return 0;
    }
    strcpy(cmd, &args[i + 1]);
    rtrim(cmd);
    ltrim(cmd);

    fcn = parse(tmp, tmp2);
    if (fcn) {
        for (i = 0; Par_ent[i].func[0]; i++) {
            if (fcn == Par_ent[i].addr) {
                for (j = 0; Par_ent[j].func[0]; j++);
                strcpy(Par_ent[j].func, Par_ent[i].func);
                strcpy(Par_ent[j].cmd, cmd);
                strcpy(Par_ent[j].help, Par_ent[i].help);
                Par_ent[j].addr = Par_ent[i].addr;
                if (!Logging_in) {
                    output("\n%s %s %s %s.\n\n",
                        MSG_THECOMMAND,
                        Par_ent[i].cmd,
                        MSG_NOWHASALIAS,
                        Par_ent[j].cmd);
                }
                break;
            }
        }
    }
    return 0;
}

int
cmd_from(char *args)
{
    char newfrom[FROM_FIELD_LEN];

    rtrim(args);

    newfrom[0] = ' ';
    if (strlen(args) > 0)
        strncpy(newfrom + 1, args, FROM_FIELD_LEN - 2);
    else {
        newfrom[0] = '*';
        strncpy(newfrom + 1, get_hostname(), FROM_FIELD_LEN - 2);
    }
    newfrom[FROM_FIELD_LEN - 1] = 0;

    set_from(Uid, newfrom);

    if (!Logging_in) {
        /* output("\n%s%s.\n\n", MSG_YOUAREFROM, strlen(args) ? args :
         * MSG_NOWHERE); */
        output("\n%s%s.\n\n", MSG_YOUAREFROM, newfrom + 1);
    }
    return 0;
}

int
cmd_list_says(char *args)
{
    int max;

    rtrim(args);
    max = strlen(args) ? atoi(args) : 10;
    list_mlist(max, 1 << MSG_SAY);
    return 0;
}

int
cmd_list_yells(char *args)
{
    int max;

    rtrim(args);
    max = strlen(args) ? atoi(args) : 10;
    list_mlist(max, (1 << MSG_YELL) | (1 << MSG_SMS) | (1 << MSG_I) | (1 << MSG_MY));
    return 0;
}

/*
 * cmd_post_survey - post survey in conference
 * args: user arguments (args)
 * ret: ok (0) or error (-1)
 */

int
cmd_post_survey(char *args)
{
    LINE fname, cname;
    char *confname, *un;
    struct TEXT_HEADER th;
    int confid;
    long textnum;
    struct CONF_ENTRY *ce;
    LONG_LINE cmdline;
    FILE *pipe;
    struct tm reporttime;

    Change_prompt = 1;
    un = NULL;
    if (!args || (*args == '\0')) {
        strcpy(args, conf_name(Current_conf, cname));
        confid = Current_conf;
    } else {
        confname = expand_name(args, CONF, 0, NULL);
        if (!confname) {
            return 0;
        }
        confid = conf_num(confname);
    }

    if (!confid) {
        output("\n%s\n\n", MSG_NOSURVEYMBOX);
        return 0;
    }
    if (!member_of(Uid, confid)) {
        output("\n%s\n\n", MSG_NOSUB);
        return 0;
    }
    th.author = Uid;
    ce = get_conf_struct(confid);
    if (ce->type == NEWS_CONF) {
        output("\n%s\n\n", MSG_NONEWS);
        return 0;
    }
    th.num = 0L;
    th.comment_num = 0;
    th.comment_conf = 0;
    th.comment_author = 0;
    th.size = 0;
    th.time = 0;
    th.type = TYPE_SURVEY;
    th.sh.n_questions = 0;
    strcpy(th.subject, "");
    strcpy(fname, Home);
    strcat(fname, EDIT_FILE);
    output("\n");
    display_header(&th, 1, confid, 0, un);
    if (strlen(th.subject) == 0) {
        output("\n");
        return 0;
    }
    if (line_ed(fname, &th, confid, 1, 1, NULL, un) == NULL) {
        output("\n%s\n\n", MSG_SURVEYREM);
        return 0;
    }
    th.sh.n_questions = get_no_survey_questions(fname);
    if (th.sh.n_questions == 0) {
        output("%s\n\n", MSG_NOQUESTFOUND);
        th.type = TYPE_TEXT;
    } else {
        th.sh.time = get_survey_time(th.time);
    }

    if ((textnum = save_text(fname, &th, confid)) == -1) {
        output("\n%s\n\n", MSG_CONFMISSING);
        return -1;
    }
    if (th.type == TYPE_SURVEY) {

        /* To be sure survey really is ready for reporting when survreport is
         * evetually run. */

        th.sh.time += 60;

        /* Get report time in tm struct form */

        memcpy(&reporttime, localtime(&(th.sh.time)), sizeof(struct tm));

        /* Build the necessary at command. Should be a variant of
         * 
         * echo "survreport <confid> <survno>" | at <time spec> 2> /dev/null
         * 
         * Exact format differs between platforms. */

#ifdef SOLARIS
        snprintf(cmdline, sizeof(cmdline), "%s '%s %d %ld' | %s -t %4d%02d%02d%02d%02d 2>/dev/null",
            SKLAFFECHO, SURVREPORT, confid, textnum,
            SKLAFFAT,
            reporttime.tm_year + 1900,
            reporttime.tm_mon + 1,
            reporttime.tm_mday,
            reporttime.tm_hour,
            reporttime.tm_min);
#endif
#ifdef LINUX
        snprintf(cmdline, sizeof(cmdline), "%s '%s %d %ld' | %s %02d%02d%02d %02d%02d 2>/dev/null",
            SKLAFFECHO, SURVREPORT, confid, textnum,
            SKLAFFAT,
            reporttime.tm_mday,
            reporttime.tm_hour,
            reporttime.tm_min,
            reporttime.tm_year,
            reporttime.tm_mon + 1);
#endif
#ifdef FREEBSD
        snprintf(cmdline, sizeof(cmdline), "%s '%s %d %ld' | %s %02d%02d %02d%02d%02d 2>/dev/null",
            SKLAFFECHO, SURVREPORT, confid, textnum,
            SKLAFFAT,
            reporttime.tm_hour,
            reporttime.tm_min,
            reporttime.tm_mon + 1,
            reporttime.tm_mday,
            reporttime.tm_year);

#endif

        /* Restore report time */

        th.sh.time -= 60;

        /* output("Runs cmd: [%s]\n", cmdline); */

        /* Execute at command */

        if ((pipe = (FILE *) popen(cmdline, "r")) == NULL) {
            output("%s\n\n", MSG_NOAT);
            return -1;
        }
        pclose(pipe);
    }
    output("%s %ld %s\n\n",
        (th.type == TYPE_TEXT) ? MSG_TEXTNAME : MSG_SURVEYNAME,
        textnum, MSG_SAVED2);
    if (confid != Current_conf) {
        Last_text = 0;
    } else {
        Last_text = textnum;
    }

    /* Note, don't mark this as read if it was accepted as a survey. Author of
     * survey should take it himself in that case. */

    if (th.type == TYPE_TEXT)
        mark_as_read(textnum, confid);

    return 0;
}



int
cmd_read_last_text(char *args)
{
    strcpy(args, MSG_LASTREAD);
    cmd_read_text(args);
    return 0;
}


/*
 * cmd_survey_result - show full result of survey
 * args: user arguments (args)
 * ret: ok (0) or error (-1)
 */

int
cmd_survey_result(char *args)
{
    long textnum;

    textnum = parse_text(args);
    if (textnum)
        display_survey_result(Last_conf, textnum);
    return 0;
}

/*
 * cmd_survey_result - show full result of survey
 * args: user arguments (args)
 * ret: ok (0) or error (-1)
 */

int
cmd_reclaim_unread(char *args)
{
    long textnum;
    int conf, first_conf = -1, only_in_conf = -1;
    int count = 0;
    LINE confname;
    char *exp_confname;

    if (args && *args) {
        strcpy(confname, args);
        exp_confname = expand_name(confname, SUBSCRIBED, 0, NULL);
        if (exp_confname) {
            only_in_conf = conf_num(exp_confname);
        }
    }
    while (ustack) {
        ++count;
        textnum = pop_unread(&conf);
        if (only_in_conf == -1 || only_in_conf == conf) {
            if (first_conf == -1)
                first_conf = conf;
            mark_as_unread(textnum, conf);
        } else if (only_in_conf != -1) {
            --count;
            push_unread2(conf, textnum);
        }
    }
    while (ustack2) {
        textnum = pop_unread2(&conf);
        if (textnum > 0)
            push_unread(conf, textnum);
    }
    output(MSG_UNREADUNREAD, count);
    Change_prompt = 1;
    if (first_conf != -1) {
        set_conf(first_conf);
        clear_comment();
        cmd_where(args);
    } else {
        output("\n");
    }
    return 0;
}

/*
 * cmd_mod_numlines - set per-user terminal height (Numlines)
 * args: user arguments (args) — optional integer rows (10–200), 0 or 'A'/'a' for autodetect; if empty, run picker UX
 * ret: ok (0) or failure (-1)  [invalid value or could not save to sklaffrc]
 */
int
cmd_mod_numlines(char *args)
{
    LINE buf;
    int v;

    /* fast path: arg provided */
    if (args && *args) {
        /* trim */
        while (*args && isspace((unsigned char)*args)) args++;
        size_t alen = strlen(args);
        while (alen && isspace((unsigned char)args[alen - 1])) { args[--alen] = '\0'; }

        /* 'A'/'a' => auto-detect */
        if (alen == 1 && (args[0] == 'a' || args[0] == 'A')) {
            if (rc_set_scalar(Uid, "Numlines", "0") != 0) {
                output(MSG_NUMLNSFAIL);
                return -1;
            }
            Numlines = detect_terminal_lines();
            output(MSG_NUMLNSOK, Numlines);
            return 0;
        }

        /* numeric (0 => auto) */
        char *endp = NULL;
        long t = strtol(args, &endp, 10);
        while (endp && *endp && isspace((unsigned char)*endp)) endp++;

        if (endp != args && (!endp || *endp == '\0') && t == 0) {
            if (rc_set_scalar(Uid, "Numlines", "0") != 0) {
                output(MSG_NUMLNSFAIL);
                return -1;
            }
            Numlines = detect_terminal_lines();
            output(MSG_NUMLNSOK, Numlines);
            return 0;
        }

        if (endp == args || (endp && *endp) || t < 10 || t > 200) {
            output(MSG_NUMLNSERR);
            return 0;  /* don’t kick user out */
        }

        v = (int)t;  /* valid explicit value */
    } else {
        /* interactive countdown */
        int top = 80;
        int old_num = Numlines;
        int old_lines = Lines;

        /* temporarily disable paging */
        Numlines = 0;
        Lines = 0;

        output("\n");
        for (int n = top; n >= 2; n--) output("%d\n", n);
        output(MSG_NUMLNSHELLO);

        /* IMPORTANT: make initial value empty to avoid garbage */
        buf[0] = '\0';
        input(buf, buf, sizeof(buf)-1, 0, 0, 0);

        /* trim both ends */
        rtrim(buf);
        char *p = buf;
        while (*p && isspace((unsigned char)*p)) p++;

        /* restore line counter; Numlines set below only on success */
        Lines = old_lines;

        /* cancel on empty or q/Q */
        if (*p == '\0' || ((p[0] == 'q' || p[0] == 'Q') && p[1] == '\0')) {
            Numlines = old_num;
            output(MSG_NUMLNCONF);
            return 0;
        }

        /* 'A'/'a' => auto */
        if ((p[0] == 'a' || p[0] == 'A') && p[1] == '\0') {
            if (rc_set_scalar(Uid, "Numlines", "0") != 0) {
                output(MSG_NUMLNSFAIL);
                Numlines = old_num;
                return -1;
            }
            Numlines = detect_terminal_lines();
            output(MSG_NUMLNSOK, Numlines);
            return 0;
        }

        /* numeric (0 => auto) */
        char *endp = NULL;
        long t = strtol(p, &endp, 10);
        while (endp && *endp && isspace((unsigned char)*endp)) endp++;

        if (endp != p && (!endp || *endp == '\0') && t == 0) {
            if (rc_set_scalar(Uid, "Numlines", "0") != 0) {
                output(MSG_NUMLNSFAIL);
                Numlines = old_num;
                return -1;
            }
            Numlines = detect_terminal_lines();
            output(MSG_NUMLNSOK, Numlines);
            return 0;
        }

        if (endp == p || (endp && *endp) || t < 10 || t > 200) {
            Numlines = old_num;
            output(MSG_NUMLNSERR);
            return 0;  /* don’t kick user out */
        }

        v = (int)t;
        Numlines = v;  /* accept immediately */
    }

    /* persist explicit value to sklaffrc */
    {
        char vstr[16];
        snprintf(vstr, sizeof(vstr), "%d", v);
        if (rc_set_scalar(Uid, "Numlines", vstr) != 0) {
            output(MSG_NUMLNSFAIL);
            return -1;  /* real error: file write */
        }
    }

    output(MSG_NUMLNSOK, v);
    return 0;
}

/*
* Implementation of Zork (and possibly other z-code games
* Work in progress, only testing for now
*/

static int ensure_dir(const char *path, mode_t mode)
{
    struct stat st;
    if (stat(path, &st) == 0) {
        if (S_ISDIR(st.st_mode)) return 0;
        errno = ENOTDIR;
        return -1;
    }
    if (errno != ENOENT) return -1;
    return mkdir(path, mode);
}

static int file_exists_r(const char *path)
{
    return access(path, R_OK) == 0;
}

static int find_frotz(char *out, size_t outsz)
{
    /* Prefer pinned path */
    if (access(FROTZ_BIN, X_OK) == 0) {
        snprintf(out, outsz, "%s", FROTZ_BIN);
        return 0;
    }
    /* Fallback to PATH using which (silent) */
    FILE *pf = popen("which frotz 2>/dev/null", "r");
    if (!pf) return -1;
    char buf[PATH_MAX] = {0};
    if (fgets(buf, sizeof buf, pf) == NULL) {
        pclose(pf);
        return -1;
    }
    pclose(pf);
    /* strip trailing newline */
    size_t n = strcspn(buf, "\r\n");
    buf[n] = '\0';
    if (buf[0] == '\0' || access(buf, X_OK) != 0) return -1;
    snprintf(out, outsz, "%s", buf);
    return 0;
}

int
cmd_zork(char *args)
{
    char *p = args;
    char gamefile[PATH_MAX], zdir[PATH_MAX], datadir[PATH_MAX];
    char frotz_path[PATH_MAX];
    const char *zname = NULL;

    Change_msg = 1;
    Change_prompt = 1;

    /* No arguments => prompt */
    if (!p) {
        output(MSG_BADARG);
        return 0;
    }

    while (*p && isspace((unsigned char)*p)) p++;
    if (*p == '\0') {
        output(MSG_BADARG);
        return 0;
    }

    /* Accept “1”, “2”, or “3” (also tolerant of words like “zork 1”) */
    if (*p == '1') zname = FROTZ_ZORK1;
    else if (*p == '2') zname = FROTZ_ZORK2;
    else if (*p == '3') zname = FROTZ_ZORK3;
    else {
        output(MSG_NOARG);
        return 0;
    }

    /* Build paths: /usr/local/sklaff/doors/frotz and .../data */
    {
        int r;

        r = snprintf(zdir, sizeof zdir, "%s", FROTZ_HOME);
        if (r < 0 || (size_t)r >= sizeof zdir) {
            output("Frotz home path is too long.\n");
            return 0;
        }

        r = snprintf(datadir, sizeof datadir, "%s/data", FROTZ_HOME);
        if (r < 0 || (size_t)r >= sizeof datadir) {
            output("Frotz data path is too long.\n");
            return 0;
        }
    }

    if (ensure_dir(zdir, 0755) == -1) {
        output("Could not find or create Frotz home directory.\n");
        return 0;
    }

    if (ensure_dir(datadir, 0700) == -1) {
        output("Could not find or create Frotz save directory.\n");
        return 0;
    }

    /* Verify chosen z-code file exists */
    {
        size_t need = strlen(zdir) + 1 + strlen(zname) + 1;
        if (need > sizeof gamefile) {
            output("Zork game path is too long.\n");
            return 0;
        }
        int r = snprintf(gamefile, sizeof gamefile, "%s/%s", zdir, zname);
        if (r < 0 || (size_t)r >= sizeof gamefile) {
            output("Failed to build Zork game path.\n");
            return 0;
        }
    }

    if (!file_exists_r(gamefile)) {
        output(MSG_NO_ZORK);
        return 0;
    }

    /* Find frotz binary */
    if (find_frotz(frotz_path, sizeof frotz_path) == -1) {
        output("Frotz not found. Please install it or set FROTZ_BIN.\n");
        return 0;
    }

    /* Prepare argv: frotz -Z 0 -R <datadir> <gamefile> */
    char *const argv[] = {
        frotz_path,
        "-Z", "0",
        "-R", (char *)datadir,
        (char *)gamefile,
        NULL
    };

    /* Full signal handling and terminal reset block */
    sigset_t sigmask, oldsigmask;
    sigemptyset(&sigmask);
    sigaddset(&sigmask, SIGNAL_NEW_TEXT);
    sigaddset(&sigmask, SIGNAL_NEW_MSG);
    sigprocmask(SIG_BLOCK, &sigmask, &oldsigmask);
    signal(SIGNAL_NEW_TEXT, SIG_IGN);
    signal(SIGNAL_NEW_MSG, SIG_IGN);
    set_avail(Uid, 1);

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        set_avail(Uid, 0);
        sigprocmask(SIG_UNBLOCK, &oldsigmask, NULL);
        return 0;
    } else if (pid == 0) {
        /* child */
        sig_reset();
        tty_reset();
        execv(frotz_path, argv);
        perror("execv");
        _exit(127);
    }

    /* parent */
    int status = 0;
    (void)waitpid(pid, &status, 0);
    signal(SIGNAL_NEW_TEXT, baffo);
    signal(SIGNAL_NEW_MSG, newmsg);
    sigprocmask(SIG_UNBLOCK, &oldsigmask, NULL);
    tty_raw();
    output("\n");
    set_avail(Uid, 0);
    return 1;
}

int cmd_bbslink(char *args)
{
    Change_msg = 1;
    Change_prompt = 1;

    static char uid_buf[16];
    snprintf(uid_buf, sizeof(uid_buf), "%d", Uid);

    /* Define lookup table for command → script arg mapping */
    static const struct {
        const char *input;
        const char *mapped;
    } bbslink_aliases[] = {
        { "usurper", "usrp" },
        { "lord",    "lord" },
        { NULL,      NULL }
    };

    const char *mapped = NULL;

    if (args && *args) {
        while (*args == ' ')
            args++;

        for (int i = 0; bbslink_aliases[i].input != NULL; i++) {
            if (strcasecmp(args, bbslink_aliases[i].input) == 0) {
                mapped = bbslink_aliases[i].mapped;
                break;
            }
        }

        if (!mapped) {
            output("\nOkänt BBSLink-spel: %s\n", args);
            return 0;
        }
    }

    if (!mapped) {
        FILE *fp = fopen(BBSLINK_INTRO, "r");
        if (!fp) {
            output("\nKunde inte läsa introduktionstexten (%s)\n", BBSLINK_INTRO);
            return 0;
        }

        char line[256];
        while (fgets(line, sizeof(line), fp)) {
            output("%s", line);
        }

        fclose(fp);
        return 0;
    }

    /* Build command */
    if (Utf8) {
        const char *cmd[] = {
            CP437_WRAPPER,
            "/doors/bbslink/bbslink.py",
            mapped,
            uid_buf,
            NULL
        };
        return run_external_cmd_args(cmd, 0);
    } else {
        const char *cmd[] = {
            "/doors/bbslink/bbslink.py",
            mapped,
            uid_buf,
            NULL
        };
        return run_external_cmd_args(cmd, 0);
    }
}

