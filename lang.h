/* lang.h */

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

#ifdef SWEDISH

/* lib/input.c */

#define MSG_MSGPROMPT 	"Meddelande: "

/* lib/output.c */

#ifdef LINUX
/* MSG_MORE is defined in Linux socket API. First ensure it is
   included, and then undefine it so we can re-define it. */
# include <sys/socket.h>
# if defined(MSG_MORE)
#  undef MSG_MORE
# endif
#endif
#define MSG_MORE 	"-Forts-"

/* lib/time_string.c */

#define MSG_SUNDAY	"S|ndags"
#define MSG_MONDAY	"M}ndags"
#define MSG_TUESDAY	"Tisdags"
#define MSG_WEDNESDAY	"Onsdags"
#define MSG_THURSDAY	"Torsdags"
#define MSG_FRIDAY	"Fredags"
#define MSG_SATURDAY	"L|rdags"
#define MSG_YESTERDAY	"Ig}r"
#define MSG_TODAY	"Idag"
#define MSG_JAN		"Jan"
#define MSG_FEB		"Feb"
#define MSG_MAR		"Mar"
#define MSG_APR		"Apr"
#define MSG_MAY		"Maj"
#define MSG_JUN		"Jun"
#define MSG_JUL		"Jul"
#define MSG_AUG		"Aug"
#define MSG_SEP		"Sep"
#define MSG_OCT		"Okt"
#define MSG_NOV		"Nov"
#define MSG_DEC		"Dec"

/* admin.c */

#define MSG_CPY1        "SklaffKOM version "
#define MSG_CPY2        "Copyright (C) 1993-1996  Torbj|rn B}}th, Peter Forsberg, Peter Lindberg,\n"
#define MSG_CPY3        "                         Odd Petersson, Carl Sundbom.\n"
#define MSG_CPY4        "QWK-hantering (C) 1994   Daniel Gr|njord\n"
#define MSG_CPY4a       "Enk{tfunktion (C) 1996   Olof Runborg\n\n"
//#define MSG_CPY5        "Uppdateringar/buggfixar gjorda av Petri Stenberg, Fredrik Bj|reman, Marcus Sundberg, Peter London och Joakim Melin.\n\n"
#define MSG_CPY6        "Programmet till{gnat Staffan Bergstr|ms minne.\n\n"
#define MSG_CPY7        "SklaffKOM distribueras UTAN GARANTI. Detta {r fri programvara och alla\n"
#define MSG_CPY8        "har r{tt sprida den enligt vissa villkor.\n\n"
#define MSG_CPY9        "Skriv visa licens f|r n{rmare detaljer. \n\n"
#define MSG_REPLYPROMPT	"(L{sa) n{sta kommentar"
#define MSG_TEXTPROMPT	"(L{sa) n{sta text"
#define MSG_CONFPROMPT	"(G} till) n{sta m|te"
#define MSG_ENDPROMPT	"Sluta"
#define MSG_TIMEPROMPT	"(Se) tiden"
#define MSG_LANG	"Svenska"
#define MSG_LICENS	"visa licens"
#define MSG_CANTCREATE	"Kan inte skapa dig som anv{ndare."
#define MSG_CANTADD     "Kan inte addera dig till vilka-listan."
#define MSG_NOTELL	"Kan inte skapa din meddelandefil."
#define MSG_CLOSETELL	"Kan inte st{nga din meddelandefil."
#define MSG_WELCOME	"V{lkommen"
#define MSG_LASTHERE	"Du var senast h{r"
#define MSG_ON		"P}"
#define MSG_OFF		"Av"
#define MSG_NOGREP	"Kan inte k|ra grep."
#define MSG_NOAT	"Varning, kan inte k|ra 'at'. Enk{ten m}ste redovisas manuellt."
#define MSG_PIPEERR	"Felaktig skrivning till pipe, kontakta operat|r!"
#define MSG_CPUERR	"CPU-tid f|rbrukad, kontakta operat|r!"
#define MSG_DISKERR	"Diskquota |verskriden, kontakta operat|r!"
#define MSG_POWER	"Str|mavbrott, kontakta operat|r!"
#define MSG_OP		"Utloggad av operat|r!"
#define MSG_TIMEOUT	"Utloggad pga inaktivitet!"
#define MSG_WELBACK	"V{lkommen }ter"
#define MSG_ACTIVETIME	"Du var inloggad i"
#define MSG_MINUTES	"minuter."
#define MSG_ONEMIN	"en minut."
#define MSG_WARNING	"Om en minut blir jag utloggad pga inaktivitet!"

/* commands.c */

#define MSG_BUSY	"{r upptagen."
#define MSG_ACT		"Idle"
#define MSG_CRPACK	"Skapar paket..."
#define MSG_FINPACK	"klart!"
#define MSG_URQ		"Vill du l{smarkera de skickade texterna? "
#define MSG_LISTCOM	"F|ljande kommandon finns:"
#define MSG_WHERE	"Du {r i"
#define MSG_HOWMANY	"H{r finns"
#define MSG_UNREADTEXTS	"nya texter."
#define MSG_NOUNREAD	"Inga nya texter."
#define MSG_ONEUNREAD	"En ny text."
#define MSG_JOIN	"Vill du bli medlem i"
#define MSG_YES		"Ja"
#define MSG_YES2	"ja"
#define MSG_YESANSWER	'j'
#define MSG_NOCONFNAME	"Du m}ste ange ett m|tesnamn."
#define MSG_ACTIVE	"Inloggad"
#define MSG_NOARG	"Kommandot tar inga parametrar."
#define MSG_NAME	"Namn"
#define MSG_TIME	"Min"
#define MSG_WHEN	"Inloggad"
#define MSG_FROM	"Fr}n"
#define MSG_RIGHTMAIL	"Bara du har r{ttigheter i din brevl}da."
#define MSG_WRONGCONF	"Felaktigt m|te."
#define MSG_YESRIGHTS	"F|ljande anv{ndare har r{ttigheter i"
#define MSG_NORIGHTS	"F|ljande anv{ndare har inte r{ttigheter i"
#define MSG_CNAMEASK	"M|tets namn: "
#define MSG_ERRCNAME	"M|tesnamnet krockar med ett annat m|tesnamn."
#define MSG_CONFTPROMPT	"Typ av m|te, (|)ppet, (s)lutet, (h)emligt, (n)ews: "
#define MSG_CONFDEFAULT	"|ppet"
#define MSG_CONFOPEN	'|'
#define MSG_CONFCLOSED	's'
#define MSG_CONFSECRET	'h'
#define MSG_CONFNEWS	'n'
#define MSG_CONFNAME	"M|tet"
#define MSG_CREATED2	"skapat."
#define MSG_CREATOR	"  (Skapare)"
#define MSG_NOTALLOW	"Du saknar beh|righet."
#define MSG_SUBOK	"Du {r nu medlem i"
#define MSG_ALREADYSUB	"Du {r redan medlem i m|tet."
#define MSG_USUBOK	"Du har nu uttr{tt ur"
#define MSG_NOSUB	"Du {r inte medlem i m|tet."
#define MSG_NONEWS	"Du f}r inte skriva texter i news-m|ten."
#define MSG_NOUSUBMAIL	"Du kan inte uttr{da ur din brevl}da."
#define MSG_NOADDMAIL   "Du kan inte addera r{ttigheter i din brevl}da."
#define MSG_NOSUBMAIL   "Du kan inte subtrahera r{ttigheter i din brevl}da."
#define MSG_NOSUBSELF   "Du kan inte subtrahera dina egna r{ttigheter."
#define MSG_ALREADYRIG  "Anv{ndaren har redan r{ttigheter h{r."
#define MSG_NORIG  	"Anv{ndaren har inga r{ttigheter h{r."
#define MSG_ADDRIGHTOK	"R{ttigheter adderade f|r"
#define MSG_SUBRIGHTOK	"R{ttigheter subtraherade f|r"
#define MSG_RIGHTCREATE "Bara m|tets skapare kan addera r{ttigheter."
#define MSG_SUBCREATE 	"Bara m|tets skapare kan subtrahera r{ttigheter."
#define MSG_NOMORECOM	"Det finns inga fler kommenterar."
#define MSG_NOTEXTLEFT	"Det finns inga nya texter h{r."
#define MSG_NOCONFLEFT	"Det finns inga m|ten med nya texter."
#define MSG_JUMPNONE	"Inga texter |verhoppade."
#define MSG_JUMPONE	"En text |verhoppad."
#define MSG_JUMPTEXT	"texter |verhoppade."
#define MSG_URNONE	"Inga texter ol{smarkerade."
#define MSG_URONE	"En text ol{smarkerad."
#define MSG_URTEXT	"texter ol{smarkerade."
#define MSG_NOTINMBOX	"Detta kommando fungerar inte i din brevl}da."
#define MSG_READONE	"Du m}ste l{sa en text f|rst."
#define MSG_ERRNUMT	"Felaktigt textantal."
#define MSG_TEXTREM	"Texten borttagen."
#define MSG_SURVEYREM	"Enk{ten borttagen."
#define MSG_SAVED	"Sparad."
#define MSG_SAVED2	"sparad."
#define MSG_NORECIP	"Du m}ste ange en mottagare."
#define MSG_NOPERMBOX	"Du kan inte g|ra personliga kommentarer i din brevl}da."
#define MSG_NOPOSTMBOX	"Du kan inte skriva inl{gg i din brevl}da."
#define MSG_NOSURVEYMBOX "Du kan inte g\xf6ra enk{ter i din brevl}da."
#define MSG_NOQUESTFOUND "Inga enk{tfr}gor funna. Konverterar till vanlig text."
#define MSG_NOREPPTR	"Kan inte spara kommentarspekaren."
#define MSG_NAMECOL	"Namn:"
#define MSG_ADDRCOL	"Adress:  "
#define MSG_TELECOL	"Telefon: "
#define MSG_EMAILCOL	"Email:   "
#define MSG_URLCOL      "URL:     "
#define MSG_ORGCOL	"Org:"
#define MSG_INACT	"Timeout: "
#define MSG_PDATE	"Betalt:  "
#define MSG_NOPAY	"Ej betalt"
#define MSG_USERON	"Anv{ndaren {r inloggad sedan"
#define MSG_FROM2	"fr}n"
#define MSG_LASTON	"Senast inloggad"
#define MSG_NOTE	"Lapp:"
#define MSG_SIG		"Plan:"
#define MSG_SUBTO	"Medlem i:"
#define MSG_CONFTYPE	"M|testyp:      "
#define MSG_CLOSED2	"slutet"
#define MSG_SECRET2	"hemligt"
#define MSG_NEWS2	"usenet"
#define MSG_CONFCREATE	"Skapat:        "
#define MSG_CONFCOM	"Kommentarer i: "
#define MSG_NUMTEXT	"Antal texter: "
#define MSG_NOMBOXSTAT	"Statusinformation lagras inte f|r din brevl}da."
#define MSG_NOUNAME	"Du m}ste ange ett anv{ndarnamn."
#define MSG_DELETED	"raderad."
#define MSG_EXITUNREAD	"markeras som ol{st vid utloggning."
#define MSG_ONLYMEMBER	"Du {r enda medlemmen i din brevl}da."
#define MSG_BADCONF	"Felaktigt m|te."
#define MSG_FOLLMEM	"F|ljande personer {r medlemmar i"
#define MSG_NOCHMBOX	"Du kan inte byta namn p} din brevl}da."
#define MSG_NOTCREATOR	"Du {r inte skapare av"
#define MSG_COMEXT	"M|tet {r ett kommentarsm|te till ett annat m|te."
#define MSG_NEWNAME	"Nytt namn: "
#define MSG_NEWCOM	"Nytt m|te: "
#define MSG_NOCHNAME	"M|tesnamnet ej {ndrat."
#define MSG_PRESNAME	"M|tets nya namn {r"
#define MSG_COMCNAME	"M|tets nya kommentarsm|te {r"
#define MSG_NODELMBOX	"Du kan inte radera din brevl}da."
#define MSG_DELCONF	"Vill du verkligen radera"
#define MSG_CONFCALL	"M|te"
#define MSG_DELETED2	"raderat."
#define MSG_PURGED	"raderat."
#define MSG_NOINFO	"Det finns ingen information lagrad."
#define MSG_NOHELP	"Det finns ingen hj{lp om det kommandot."
#define MSG_COMMAND	"Kommando:"
#define MSG_GREPPROMPT	"S|kstr{ng: "
#define MSG_BADGREP	"Felaktig s|kstr{ng."
#define MSG_GREPNFOUND	"S|kstr{ngen hittades inte n}gonstans i detta m|te."
#define MSG_GGREPNFOUND	"Ingen text inneh|ll s|kstr{ngen."
#define MSG_MODPINFO	"Uppdatering av personuppgifter f|r"
//#define MSG_MODPINFO2	"Samtliga uppgifter {r givetvis frivilliga, men fyller du i stad ser du v{dret hos dig i prompten.\n\n" /* Upcoming feature */
#define MSG_INFOADDR	"Adress  : "
#define MSG_INFOZIP	"Postnr  : "
#define MSG_INFOTOWN	"Stad    : "
#define MSG_INFOTELE1	"Telef}n : "
#define MSG_INFOTELE2	"Tele2   : "
#define MSG_INFOTELE3	"Tele3   : "
#define MSG_INFOMAIL1	"E-post  : "
#define MSG_INFOMAIL2	"E-mail2 : "
#define MSG_INFOURL     "Hemsida : "
#define MSG_INFOORG	"Org     : "
#define MSG_SAVEINFO	"Ska f|r{ndringarna sparas (Ja/Nej) ? "
#define MSG_INFOSAVED	"Personuppgifterna uppdaterade."
#define MSG_INFONOSAVE	"Inga f|r{ndringar gjorda."
#define MSG_ASKTIMEOUT	"Inaktivitetsperiod i minuter: "
#define MSG_TIMESET	"Inaktivitetsperiod satt till"
#define MSG_NONE	"Ingen"
#define MSG_FILENAME	"Filnamn: "
#define MSG_NOFILES	"M|tet inneh}ller inga filer."
#define MSG_FILEHEAD	"filnamn               storlek  beskrivning"
#define MSG_LINEROW	"------------------------------------------"
#define MSG_BADFILE	"Filen finns inte."
#define MSG_DESCRIBE	"Beskrivning: "
#define MSG_NOMAIL	"Kan inte k|ra mail."
#define MSG_NOINEWS	"Kan inte k|ra inews."
#define MSG_MAILED	"Mail skickat."
#define MSG_EMSUB	"Subject: "
#define MSG_EMFROM	"From: "
#define MSG_EMFROM2	"from: "
#define MSG_EMFROM3	"FROM: "
#define MSG_EMFROM4     "From " /* Newer mailprgs may need this, PL 2025-0714 */
#define MSG_EMFROM5     "from " /* Newer mailprgs may need this, PL 2025-0714 */
#define MSG_EMRETURN	"Return-Path: "
#define MSG_PRIO	"prioriterat."
#define MSG_DEPRIO	"avprioriterat."
#define MSG_NOPRIOMBOX	"Du kan inte prioritera om din brevl}da."
#define MSG_BACK0	"Inga texter backade."
#define MSG_BACK1	"En text backad."
#define MSG_BACK2	"texter backade."
#define MSG_POSTED	"News postat."
#define MSG_NGROUP	"Newsgroups:"
#define MSG_NOTINCONF	"Du kan inte kommentera texter i andra m|ten."
#define MSG_NOSUBJ	"Du m}ste ange ett {rende."
#define MSG_MIMETYPE	"Mime-Version: 1.0"
#define MSG_MIMECONT	"Content-Type: text/plain; charset=\"iso-8859-1\""
#define MSG_MIMEENC	"Content-Transfer-Encoding: quoted-printable"
#define MSG_NOCMD       "Du m}ste ange ett kommandonamn."
#define MSG_NOALIAS     "Du m}ste ange ett alias."
#define MSG_THECOMMAND  "Kommandot"
#define MSG_NOWHASALIAS "har nu aliaset"
#define MSG_CMNTMOVED   "Kommentaren automatiskt flyttad till "
#define MSG_YOUAREFROM  "Du {r nu fr}n "
#define MSG_NOWHERE     "ingenstans"
#define MSG_UNREADUNREAD "\n%d text(er) ol{smarkerades.\n"
#define MSG_NUMLNSERR	"Ogiltigt v{rde. Ange 10 – 200.\n"
#define MSG_NUMLNSHELLO	"Skriv in siffran du ser h|gst upp (0 f|r auto) :"
#define MSG_NUMLNSFAIL	"Kunde tyvärr inte spara inst{llningen, meddela SysOp!\n"
#define MSG_NUMLNSOK	"Antal rader satt till %d.\n"
#define MSG_NUMLNCONF	"Ingen {ndring.\n"
#define MSG_NO_ZORK	"No z3-file found, sorry"  					/*Zork */
#define MSG_BADARG      "\nF|ljande infocom-spel finns f|r n{rvarande :\n\nZork 1\nZork 2\nZork 3\n\nFör att spela, ge kommandot Zork <nr>.\n\n"	/* Zork */
/* Nytt (Se) tiden-kommando */
#define MSG_DISPTIME    "Klockan {r nu"
#define MSG_IT          "den"
#define MSG_DISPTIME2   "och du har varit inloggad i"
#define MSG_BEATS1      "Tiden på internet {r"
#define MSG_BEATS2      "och i Sverige har"
#define MSG_BEATS3      "av dygnet hunnit g}."


/* conf.c */

#define MSG_CONFSORT	"Sorterar m|teslistan..."
#define MSG_MAILBOX	"Brevl}dan"
#define MSG_CLOSED	"  (Slutet)"
#define MSG_SECRET	"  (Hemligt)"
#define MSG_NEWS	"  (Usenet)"
#define MSG_MEMBERMARK	'*'
#define MSG_FILAREA	'F'
#define MSG_CONFCREATOR	'S'
#define MSG_TOTNULL	"Inga nya texter."
#define MSG_TOTONE	"Totalt en ny text."
#define MSG_TOTAL	"Totalt"
#define MSG_WHERE2	"Du befinner dig i"
/* edit.c */

#define MSG_REMTEXT	"Vill du verkligen ta bort din text?"
#define MSG_EDCHANGE	"!{ndra"
#define EDCHANGEN	1
#define MSG_EDREAD	"!l{sa"
#define EDREADN		3
#define MSG_EDWHOLE	"!hela"
#define EDWHOLEN	1
#define MSG_EDINCL	"!inkludera"
#define EDINCLN		3
#define MSG_EDTELL   	"!s{g"
#define EDTELLN		2
#define MSG_EDYELL	"!skrik"
#define EDYELLN		2
#define MSG_EDREVIEW	"!}terse"
#define EDREVIEWN	1
#define MSG_EDREMOVE	"!bort"
#define EDREMOVEN	1
#define MSG_EDSAVE	"!spara"
#define EDSAVEN		2
#define MSG_EDPOST	"!l{gga"
#define EDPOSTN		3
#define MSG_EDMOVE	"!flytta"
#define EDMOVEN		1
#define MSG_EDDEL	"!radera"
#define EDDELN		2
#define MSG_EDINS	"!infoga"
#define EDINSN		3
#define MSG_EDQUOTE	"!citera"
#define EDQUOTEN	1
#define MSG_EDWHO	"!vilka"
#define EDWHO    	1
#define MSG_EDI         "!jag"
#define EDI             1
#define MSG_EDMY        "!min"
#define EDMY            1
#define MSG_EDYELL2     "!ropa"
#define EDYELL2N        2
#define MSG_FNPROMPT	"Filnamn: ~/"
#define MSG_BADFNAME	"Felaktigt filnamn."
#define MSG_OUTMEM	"Slut p} minnet!"
#define MSG_FILEINC	"Filen inluderad."
#define MSG_BADLINE	"Felaktigt radnummer."
#define MSG_MOVED	"Texten flyttad till "
#define MSG_EDHELP0	"\n!{ndra <radnummer>\n"
#define MSG_EDHELP1	"!radera <radnummer>\n"
#define MSG_EDHELP2	"!infoga (rad f|re) <radnummer>\n"
#define MSG_EDHELP3	"!l{sa (texten)\n"
#define MSG_EDHELP4	"!hela (texten)\n"
#define MSG_EDHELP5	"!inkludera <filnamn>\n"
#define MSG_EDHELP6	"!s{g <anv{ndare>, <meddelande>\n"
#define MSG_EDHELP7	"!skrik <meddelande>\n"
#define MSG_EDHELP8	"!}terse <textnummer>\n"
#define MSG_EDHELP9	"!citera (kommenterad text)\n"
#define MSG_EDHELP9p5	"!vilka ({r inloggade)\n"
#define MSG_EDHELP10	"!flytta (till) <m|te>\n"
#define MSG_EDHELP11	"!bort (med texten)\n"
#define MSG_EDHELP12	"!spara (texten)\n"
#define MSG_EDHELP13	"!l{gga (texten)\n\n"
#define MSG_EDHELP14   	"Ctrl-U/X raderar raden\n"
#define MSG_EDHELP15	"Ctrl-W raderar ett ord\n"
#define MSG_EDHELP16	"Ctrl-Z/D sparar texten\n"
#define MSG_EDHELP17	"Ctrl-L visar hela texten\n"
#define MSG_EDHELP18	"Ctrl-C tar bort texten\n\n"
#define MSG_EDRECOVER	"]terupptar en avbruten editering..."
#define MSG_NOSAVE	"Du f}r inte spara texten h{r, flytta den f|rst."
#define MSG_QUSE	"j = citera rad, a = citera resten, q = avbryt, annat = n{sta rad"
#define MSG_NOBODYQUOTE	"Sorry, hittar ingen text att citera\n"

#define MSG_SURVHELP1    "Fr}getyper f|r enk{ter\n\n"
#define MSG_SURVHELP2    "Fritext  : ## text\n"
#define MSG_SURVHELP3    "Tal      : ## nummer\n"
#define MSG_SURVHELP4    "Enkelval : ## singel : <antal alternativ>\n"
#define MSG_SURVHELP5    "Flerval  : ## multipel : <antal alternativ>\n"
#define MSG_SURVHELP6    "Intervall: ## intervall : <nedre gr{ns> : <|vre gr{ns>\n\n"

/* flag.c */

#define MSG_FLAG0	"ibm-pc"
#define MSG_FLAG0N	2 	/*ambiguity check - how many chars required to make flag name unique /Detective PL 2025-07-30 */
#define MSG_FLAG0F	"IBM-PC (teckenupps{ttning)"
#define MSG_FLAG1	"iso 8859-1"
#define MSG_FLAG1N	2
#define MSG_FLAG1F	"ISO 8859-1 (teckenupps{ttning)"
#define MSG_FLAG2	"mac"
#define MSG_FLAG2N	2
#define MSG_FLAG2F	"Macintosh (teckenupps{ttning)"
#define MSG_FLAG3	"n{rvaro"
#define MSG_FLAG3N	1
#define MSG_FLAG3F	"(Visa) n{rvaromeddelanden"
#define MSG_FLAG4    	"skrik"
#define MSG_FLAG4N	2
#define MSG_FLAG4F	"(Till}t) skrik"
#define MSG_FLAG5	"sluta"
#define MSG_FLAG5N	2
#define MSG_FLAG5F	"Sluta (som default)"
#define MSG_FLAG6	"s{g"
#define MSG_FLAG6N	2
#define MSG_FLAG6F	"(Till}t) s{g"
#define MSG_FLAG7	"{rende{ndring"
#define MSG_FLAG7N	1
#define MSG_FLAG7F	"[rende{ndring (vid kommentar)"
#define MSG_FLAG8	"mellanslag"
#define MSG_FLAG8N	1
#define MSG_FLAG8F	"Mellanslag (ger vagnretur)"
#define MSG_FLAG9	"kopia"
#define MSG_FLAG9N	1
#define MSG_FLAG9F	"Kopia (av s{nda brev)"
#define MSG_FLAG10	"f|rfattare"
#define MSG_FLAG10N	1
#define MSG_FLAG10F	"F|rfattare (efter text)"
#define MSG_FLAG11	"datum"
#define MSG_FLAG11N	1
#define MSG_FLAG11F	"Datum (i texthuvud)"
#define MSG_FLAG12	"pip"
#define MSG_FLAG12N	1
#define MSG_FLAG12F	"(Till}t) pip"
#define MSG_FLAG13	"rensa"
#define MSG_FLAG13N	1
#define MSG_FLAG13F	"Rensa (sk{rmen f|re varje text)"
#define MSG_FLAG14	"header"
#define MSG_FLAG14N	1
#define MSG_FLAG14F	"Header (vid l{sning av email/news)"
/*
#define MSG_FLAG15	"strippa"
#define MSG_FLAG15N	2
#define MSG_FLAG15F	"Strippa (bit 8 vid inmatning)"
*/
#define MSG_FLAG15	"om|jligt"
#define MSG_FLAG15N	1
#define MSG_FLAG15F	"Om|jligt (speciall{ge)"
#define MSG_FLAG16	"bell"
#define MSG_FLAG16N	1
#define MSG_FLAG16F	"Bell (vid n{rvaromeddelanden)"
#define MSG_FLAG17      "gammal"
#define MSG_FLAG17N     1
#define MSG_FLAG17F     "Gammal (vilkalistesortering)"
#define MSG_FLAG18	"ansi"
#define MSG_FLAG18N	2
#define MSG_FLAG18F	"ANSI-färger"
#define MSG_FLAG19	"utf8"
#define MSG_FLAG19N	1
#define MSG_FLAG19F	"UTF-8 (teckenupps{ttning)"
#define MSG_NOFLAG	"Du m}ste ange en flagga."
#define MSG_PCWARN	"Du b|r sl} av IBM-PC f|rst."
#define MSG_ISOWARN	"Du b|r sl} av ISO 8859-1 f|rst."
#define MSG_MACWARN	"Du b|r sl} av Macintosh f|rst."
#define MSG_UTFWARN	"Du b|r sl} av UTF-8 f|rst."
#define MSG_BADFLAG	"Felaktig flagga."
#define MSG_FLAG	"Flaggan"
#define MSG_FLON	"p}slagen."
#define MSG_FLOFF	"avslagen."

/* msg.c */

#define MSG_YELLS	"skriker:"
#define MSG_LOGGEDIN	"har loggat in."
#define MSG_LOGGEDOUT	"har loggat ut."
#define MSG_STRANGEMSG	"Ok{nd meddelandetyp fr}n"
#define MSG_NOSAY	"har st{ngt av s{gfunktionen."
#define MSG_NOYELL	"har st{ngt av skrikfunktionen."
#define MSG_NOACTIVE	"{r inte inloggad."
#define MSG_CONFMSG	"Meddelandet skickat till"
#define MSG_IDLEWARNING "inaktiv sedan"

/* parse.c */

#define MSG_TEXTPROMPT2	"L{sa"
#define MSG_MULTICOM	"Flertydigt kommando:"
#define MSG_CHOSECOM	"Dessa matchade:"
#define MSG_COMERR	"Felaktigt kommando:"
#define MSG_HELPQ	"Skriv ? f|r hj{lp!"
#define MSG_ECONFN	"\nDet finns inget s}dant m|te.\n\n"
#define MSG_EUSERN	"\nDet finns ingen s}dan anv{ndare.\n\n"
#define MSG_EUSERA	"\nDet finns ingen s}dan inloggad anv{ndare.\n\n"
#define MSG_ECONFS	"\nDet finns inget s}dant m|te som du {r medlem i.\n\n"
#define MSG_ECONFU	"\nDet finns inget s}dant m|te som du inte {r medlem i.\n\n"
#define MSG_ECOMN	"\nFelaktigt namn.\n\n"
#define MSG_MULNAM	"Flertydigt namn, dessa matchade:"
#define MSG_MOREMUL	"(Fler matchade men visas inte.)"
#define MSG_PICKLIST	"Ange nummer eller namn: "
#define MSG_BADPICK	"Felaktig inmatning."

/* text.c */

#define MSG_FROMSUB	"Fr}n: "
#define MSG_TOSUB	"Till: "
#define MSG_TEXTNAME	"Inl{gg"
#define MSG_SURVEYNAME	"Enk{t"
#define MSG_WRITTENBY	"Skriven av"
#define MSG_TOFROM	"Till/Fr}n"
#define MSG_EMPTYTEXT	"Tom text"
#define MSG_EMPTYSURVEY	"Tom enk{t"
#define MSG_ONELINE	"En rad"
#define MSG_LINES	"rader"
#define MSG_REPLYIN	"Kommentar i"
#define MSG_REPLYTO	"Kommentar till text"
#define MSG_COPYTO	"Kopia av brev till"
#define MSG_IN		"i"
#define MSG_BY		"av"
#define MSG_RECIPIENT	"Mottagare:"
#define MSG_CONF	"M|te:"
#define MSG_SUBJECT	"[rende: "
#define MSG_SUBJECT2	"[rende"
#define MSG_NOTEXTNUM	"Du m}ste ange ett textnummer."
#define MSG_NOTEXT	"Texten finns inte."
#define MSG_NOSURVEY	"Enk{ten finns inte."
#define MSG_NOREAD	"Kan inte l{sa texten."
#define MSG_EOT		"Slut p} text"
#define MSG_EOSURVEY	"Slut p} enk{t"
#define MSG_ERRTNUM	"Felaktigt textnummer."
#define MSG_LASTREAD	"senaste"
#define MSG_REPLIED	"kommenterade"
#define MSG_LASTMSG	"s{g"
#define MSG_LASTMSGERR	"Du har inte mottagit n}gon s{g."
#define MSG_NOLASTTEXT	"Det finns ingen senaste text."
#define MSG_NOREPLY	"Texten {r ingen kommentar."
#define MSG_CONFMISSING	"Detta ska inte h{nda."
#define MSG_BADAD	"Otill}ten emailadress."
#define MSG_ERRCREATET	"Kan inte skapa texten."
#define MSG_UNKNOWNU	"(ok{nd)"
#define MSG_NQUESTIONS  "Antal fr}gor"
#define MSG_REPORTRESULT "Redovisas"

#define MSG_NOTASURVEY  "Texten {r inte en enk{t"
#define MSG_NOTREPORTED "Enk{ten {r inte redovisad {nnu"
#define MSG_RESULTFOR   "Resultat f|r enk{t"
#define MSG_NOREPLIES   "Inga svar"
#define MSG_ONEANSWER   "Ett svar"
#define MSG_ANSWER      "svar"
#define MSG_EORESULT    "Slut p} resultat f|r enk{t"


/* user.c */

#define MSG_SYSOP	"Systemoperat|r"
#define MSG_ROOTERR	"Root kan inte k|ra SklaffKOM av interna orsaker."
#define MSG_ALREADYON	"{r redan inloggad i en annan terminal."
#define MSG_QUITASK	"Vill att vi loggar ut dig d{rifr}n, s} att du kan forts{tta h{r?"
#define MSG_QUITTRY	"\nF|rs|ker avsluta den andra processen..."
#define MSG_QUITFAIL	"misslyckades."
#define MSG_QUITOK	"lyckades!"
#define MSG_KILLTRY	"F|rs|ker d|da den andra processen..."
#define MSG_KILLOK	"det gick nog bra."
#define MSG_QUITEXIT	"Avslutar."
#define MSG_NOPAR       " (Nej)"
#define MSG_USERS	"anv{ndare."
#define MSG_USER_ALREADY_EXIST "Anv{ndaren finns redan."

/* survey.c */

#define MSG_SURVEYANS   "Dina enk{tsvar var"
#define MSG_SAVESURVANS "Spara dina svar"
#define MSG_SURVSAVED   "Svaren sparade"
#define MSG_SURVNOSAVE  "Svaren ej sparade"
#define MSG_SURVDELIMIT1 "-------------------------- Svar ----------------------------"
#define MSG_SURVDELIMIT2 "------------------------------------------------------------"
#define MSG_WHENREPORT   "Om hur l}ng tid ska enk{ten redovisas?"
#define MSG_WHENFORMAT   "(dagar:timmar:minuter)"
#define MSG_WRONGFORMAT  "Felaktig angivelse"
#define MSG_TOTANSWERS   "Totalt antal svar"
#define MSG_BLANKVOTES   "Blankr|ster"
#define MSG_NOANSWERS    "Antal svar"
#define MSG_ANSWER2      "Svar"
#define MSG_MEAN         "Medel"
#define MSG_STD          "Standardavvikelse"

/* sklaffadm.c */

#define MSG_INETNAME	"Internetanv{ndare"
#define MSG_MODEMNAME	"Modempoolsanv{ndare"
#define MSG_OTHERNAME	"\\vriga anv{ndare"
#define MSG_NONAME	"Du m}ste ange ett anv{ndarnamn."
#define MSG_NODATE	"Du m}ste ange ett datum."
#define MSG_NOMODEM	"Anv{ndaren {r inte en modempoolsanv{ndare."
#define MSG_PAYDATE	"Betalningsdatum f|r"
#define MSG_CHANGED	"{ndrat till"
#define MSG_DENY	"avst{ngd."
#define MSG_ALLOW	"p}sl{ppt."
#define MSG_ERRUID	"Felaktigt uid."
#define MSG_CONFHEAD	" Num     Textintervall  Textantal Liv  M|tesnamn"
#define MSG_FILEHEAD2	" Num     Kilobyte filer  M|tesnamn"
#define MSG_TEXTS	"texter"
#define MSG_ONLINE	"online"
#define MSG_CTERR	"Du m}ste b}de m|tesnummer och antal texter."
#define MSG_DELTEXTS	"texter raderade."
#define MSG_ARGERR	"Felaktigt argument."
#define MSG_ALREADYDOWN	"SklaffKOM {r redan nere."
#define MSG_CAUSE	"Mata in orsak"
#define MSG_CREATED	"skapad."
#define MSG_NOTIFY	"Meddelar alla att SklaffKOM tas ned."
#define MSG_EXITNOW	"Logga ut omedelbart, SklaffKOM tas ned om en minut!\n"
#define MSG_WAITMIN	"V{ntar en minut"
#define MSG_THROWOUT	"Kastar ut alla inloggade."
#define MSG_THROW	"utkastad."
#define MSG_DOWNREADY	"nedtaget."
#define MSG_ALREADYUP	"{r redan |ppet."
#define MSG_UPREADY	"|ppnat."
#define MSG_LIFE1	"Livsl{ngd f|r "
#define MSG_LIFE2	" satt till "
#define MSG_LIFE3	" dagar."
#define MSG_ADMHELP1	"\nSyntax: sklaffadm [kommando] [argument]\n\n"
#define MSG_ADMHELP2	"   userlist [date] - Listar anv{ndare och status.\n"
#define MSG_ADMHELP3	"   disable \"user\" - St{nger av en anv{ndare.\n"
#define MSG_ADMHELP4	"   enable \"user\" - Sl{pper p} en anv{ndare.\n"
#define MSG_ADMHELP5	"   paydate \"user\" date - Anger betalningsdatum.\n\n"
#define MSG_ADMHELP6	"   conflist - Listar m|ten och antal texter.\n"
#define MSG_ADMHELP7	"   filelist - Listar storleken p} filer i m|ten.\n"
#define MSG_ADMHELP8	"   maillist - Listar brevl}dor och antal texter.\n"
#define MSG_ADMHELP9	"   life <conf> <num> - S{tter livsl{ngd f|r texter i <conf>, 0 = o{ndlig.\n"
#define MSG_ADMHELP10	"   maillife <uid> <num> - S{tter livsl{ngd f|r texter i <uid>, 0 = o{ndlig.\n"
#define MSG_ADMHELP11	"   purge <conf> <num> - Rensar texter, beh}ller minst <num> st.\n"
#define MSG_ADMHELP12	"                        conf = -1 rensar i alla m|ten.\n"
#define MSG_ADMHELP13	"   mailpurge <uid> <num> - Rensar brev, beh}ller minst <num> st.\n"
#define MSG_ADMHELP14	"                           mbox = -1 rensar i alla brevl}dor.\n\n"
#define MSG_ADMHELP15	"   who - Inloggade anv{ndare.\n"
#define MSG_ADMHELP16	"   down - Tar ner SklaffKOM snyggt.\n"
#define MSG_ADMHELP17	"   up - Till}ter inloggning i SklaffKOM.\n\n"

/* sklaffacct.c */
/* Let's do UTF-8 because at least we'll have a higher hitrate than SF7 */
#define MSG_INNAME	"För och efternamn      : "
#define MSG_INLOGIN	"Inloggningsnamn        : "
#define MSG_INPASSWD	"Önskat lösenord        : "
#define MSG_INMODEM	"Modempools-tillgång    : "
#define MSG_INPOST	"E-postadress           : "
#define MSG_INTELE	"Telefonnummer          : "
#define MSG_ACCAPP	"Kontoansökning: "
#define MSG_INSNAME	"Namn     : "
#define MSG_INSLOGIN	"\nLogin    : "
#define MSG_INSPASSWD	"\nLösen    : "
#define MSG_INSMODEM	"\nModem    : "
#define MSG_INSPOST	"\nE-post   : "
#define MSG_INSTELE	"\nTelefon  : "
#define MSG_APPLIED	"Tackar för det! \nDin ansökan är registrerad och vi hör snart av oss.\nVälkommen åter.\n\n"
#define MSG_UIDINUSE	"Inloggningsnamn kan ej anv{ndas. Vänligen välj annat."

/* mailtoss.c */

#define MSG_MTINFO	"Syntax: mailtoss <user>"

/* newstoss.c */

#define MSG_NTINFO	"Syntax: newstoss <conference>"
#define MSG_BADACTIVE	"Kan inte |ppna active-filen."
#define MSG_NOGROUP	"Det finns ingen s}dan newsgrupp i din active-fil."
#define MSG_MSGID	"Message-ID:"
#define MSG_REFID	"References:"

/* survreport.c */

#define MSG_SRINFO      "Syntax: survreport <confid> <survey no>"
#define MSG_NOREPORT    "Enk{ten ska inte redovisas {nnu"
#define MSG_REPORT      "Redovisning: "

#else

/* lib/input.c */

#define MSG_MSGPROMPT 	"Message: "

/* lib/output.c */

#ifdef LINUX
/* MSG_MORE is defined in Linux socket API. First ensure it is
   included, and then undefine it so we can re-define it. */
# include <sys/socket.h>
# if defined(MSG_MORE)
#  undef MSG_MORE
# endif
#endif
#define MSG_MORE 	"-More-"

/* lib/time_string.c */

#define MSG_SUNDAY	"Sunday"
#define MSG_MONDAY	"Monday"
#define MSG_TUESDAY	"Tuesday"
#define MSG_WEDNESDAY	"Wednesday"
#define MSG_THURSDAY	"Thursday"
#define MSG_FRIDAY	"Friday"
#define MSG_SATURDAY	"Saturday"
#define MSG_YESTERDAY	"Yesterday"
#define MSG_TODAY	"Today"
#define MSG_JAN		"Jan"
#define MSG_FEB		"Feb"
#define MSG_MAR		"Mar"
#define MSG_APR		"Apr"
#define MSG_MAY		"May"
#define MSG_JUN		"Jun"
#define MSG_JUL		"Jul"
#define MSG_AUG		"Aug"
#define MSG_SEP		"Sep"
#define MSG_OCT		"Oct"
#define MSG_NOV		"Nov"
#define MSG_DEC		"Dec"

/* admin.c */

#define MSG_CPY1	"SklaffKOM version "
#define MSG_CPY2	"Copyright (C) 1993-1994  Torbj|rn B}}th, Peter Forsberg, Peter Lindberg,\n"
#define MSG_CPY3	"                         Odd Petersson, Carl Sundbom.\n"
#define MSG_CPY4	"QWK extensions (C) 1994  Daniel Gr|njord\n\n"
#define MSG_CPY4a	"Surveys (C) 1996         Olof Runborg\n\n" /* Missing language strings made the english binary crash, now fixed 2025-07-14 PL */
#define MSG_CPY5	"Program dedicated to the memory of Staffan Bergstr|m.\n\n"
#define MSG_CPY6	"SklaffKOM comes with ABSOLUTELY NO WARRANTY. This is free software, \n"
#define MSG_CPY7	"and you are welcome to redistribute it under certain conditions.\n\n"
#define MSG_CPY8	"Type "
#define MSG_CPY9	" for details.\n\n"
#define MSG_REPLYPROMPT	"(Read) next follow-up"
#define MSG_TEXTPROMPT	"(Read) next article"
#define MSG_CONFPROMPT	"(Goto) next conference"
#define MSG_ENDPROMPT	"Logout"
#define MSG_TIMEPROMPT	"(Display) time"
#define MSG_LANG	"English"
#define MSG_LICENS	"show license"
#define MSG_CANTCREATE	"Cannot create user."
#define MSG_CANTADD     "Cannot add to active user file."
#define MSG_NOTELL	"Cannot create message file."
#define MSG_CLOSETELL	"Cannot close message file."
#define MSG_WELCOME	"Welcome"
#define MSG_LASTHERE	"You were last here"
#define MSG_ON		"On"
#define MSG_OFF		"Off"
#define MSG_NOGREP	"Cannot exec grep."
#define MSG_NOAT	"Warning, cannot execute 'at'. Survey must be reported manually."
#define MSG_PIPEERR	"Error writing to pipe, contact operator!"
#define MSG_CPUERR	"CPU-time exceeded, contant operator!"
#define MSG_DISKERR	"Diskquota exceeded, contact operator!"
#define MSG_POWER	"Power failure, contact operator!"
#define MSG_OP		"Logged out by operator!"
#define MSG_TIMEOUT	"Inactive too long!"
#define MSG_WELBACK	"Welcome back anytime"
#define MSG_ACTIVETIME	"You were online"
#define MSG_MINUTES	"minutes."
#define MSG_ONEMIN	"one minute."
#define MSG_WARNING	"Inactivity logout in one minute!"

/* commands.c */

#define MSG_BUSY	"is busy."
#define MSG_ACT		"Idle   "
#define MSG_CRPACK	"Creating packet..."
#define MSG_FINPACK	"finished!"
#define MSG_URQ		"Mark as read all sent texts? "
#define MSG_LISTCOM	"Commands available:"
#define MSG_WHERE	"Conference:"
#define MSG_HOWMANY	"You have"
#define MSG_UNREADTEXTS	"new articles."
#define MSG_NOUNREAD	"No new articles."
#define MSG_ONEUNREAD	"One new article."
#define MSG_JOIN	"Do you want to subscribe to"
#define MSG_YES		"Yes"
#define MSG_YES2	"yes"
#define MSG_YESANSWER	'y'
#define MSG_NOCONFNAME	"You must enter a conference name."
#define MSG_ACTIVE	"Online"
#define MSG_NOARG	"No parameters to this command."
#define MSG_NAME	"User"
#define MSG_TIME	"Mins"
#define MSG_WHEN	"Logged in"
#define MSG_FROM	"From"
#define MSG_RIGHTMAIL	"Only you have permissions to your mailbox."
#define MSG_WRONGCONF	"Wrong conference."
#define MSG_YESRIGHTS	"The following users have permissions to"
#define MSG_NORIGHTS	"The following users have no permissions to"
#define MSG_CNAMEASK	"Conference name: "
#define MSG_ERRCNAME	"That name collides with another conference name."
#define MSG_CONFTPROMPT	"Type of conference, (o)pen, (c)losed, (s)ecret, (n)ews: "
#define MSG_CONFDEFAULT	"open"
#define MSG_CONFOPEN	'o'
#define MSG_CONFCLOSED	'c'
#define MSG_CONFSECRET	's'
#define MSG_CONFNEWS	'n'
#define MSG_CONFNAME	"Conference"
#define MSG_CREATED2	"created."
#define MSG_CREATOR	"  (Creator)"
#define MSG_NOTALLOW	"You are not allowed to do that."
#define MSG_SUBOK	"You now subscribe to"
#define MSG_ALREADYSUB	"You already subscribe to that conference."
#define MSG_USUBOK	"You have now unsubscribed to"
#define MSG_NOSUB	"You do not subscribe to that conference."
#define MSG_NONEWS	"News postings not allowed."
#define MSG_NOUSUBMAIL	"You cannot unsubscribe to your mailbox."
#define MSG_NOADDMAIL   "You cannot add permissions to your mailbox."
#define MSG_NOSUBMAIL   "You cannot subtract permissions from your mailbox."
#define MSG_NOSUBSELF   "You cannot subtract your own permissions."
#define MSG_ALREADYRIG  "This user already have permissions here."
#define MSG_NORIG  	"That user does not have any permissions here."
#define MSG_ADDRIGHTOK	"Permissions added for"
#define MSG_SUBRIGHTOK	"Permissions subtracted for"
#define MSG_RIGHTCREATE "Only the creator can add permissions."
#define MSG_SUBCREATE 	"Only the creator can subtract permissions."
#define MSG_NOMORECOM	"There are no more follow-ups."
#define MSG_NOTEXTLEFT	"There are no new articles here."
#define MSG_NOCONFLEFT	"There are no conferences with new articles."
#define MSG_JUMPNONE	"No articles skipped."
#define MSG_JUMPONE	"One article skipped."
#define MSG_JUMPTEXT	"articles skipped."
#define MSG_URNONE	"No articles marked as new."
#define MSG_URONE	"One article marked as new."
#define MSG_URTEXT	"articles marked as new."
#define MSG_NOTINMBOX	"This command does not work in the mailbox."
#define MSG_READONE	"You must read a article first."
#define MSG_ERRNUMT	"Wrong number of articles."
#define MSG_TEXTREM	"Article deleted."
#define MSG_SURVEYREM	"Survey deleted."
#define MSG_SAVED	"Saved."
#define MSG_SAVED2	"saved."
#define MSG_NORECIP	"Please supply a recipient."
#define MSG_NOPERMBOX	"You cannot make personal repiles in your mailbox."
#define MSG_NOPOSTMBOX	"You cannot write articles in your mailbox."
#define MSG_NOSURVEYMBOX	"You cannot make surveys in your mailbox."
#define MSG_NOQUESTFOUND "No survey questions found. Converting to ordinary text."
#define MSG_NOREPPTR	"Cannot save reply pointer."
#define MSG_NAMECOL	"Name:"
#define MSG_ADDRCOL	"Address: "
#define MSG_TELECOL	"Phone:   "
#define MSG_EMAILCOL	"Email:   "
#define MSG_URLCOL      "URL:     "
#define MSG_ORGCOL	"Org:"
#define MSG_INACT	"Timeout: "
#define MSG_PDATE	"Paid to: "
#define MSG_NOPAY	"Not paid"
#define MSG_USERON	"User is online since"
#define MSG_FROM2	"from"
#define MSG_LASTON	"Last logged in"
#define MSG_NOTE	"Note:"
#define MSG_SIG		"Plan:"
#define MSG_SUBTO	"Member of:"
#define MSG_CONFTYPE	"Type:         "
#define MSG_CLOSED2	"closed"
#define MSG_SECRET2	"secret"
#define MSG_NEWS2	"usenet"
#define MSG_CONFCREATE	"Created:      "
#define MSG_CONFCOM	"Replies in:   "
#define MSG_NUMTEXT	"Articles:    "
#define MSG_NOMBOXSTAT	"Status information in not collected for the mailbox."
#define MSG_NOUNAME	"Please supply a user name."
#define MSG_DELETED	"deleted."
#define MSG_EXITUNREAD	"is marked as new when you logout."
#define MSG_ONLYMEMBER	"You are the sole member of your mailbox."
#define MSG_BADCONF	"Wrong conference."
#define MSG_FOLLMEM	"The following users subscribe to"
#define MSG_NOCHMBOX	"You cannot change the name of your mailbox."
#define MSG_NOTCREATOR	"You are not the creator of"
#define MSG_COMEXT	"This conference is a reply conference to another conference."
#define MSG_NEWNAME	"New name: "
#define MSG_NEWCOM	"New conference: "
#define MSG_NOCHNAME	"Conference name unchanged."
#define MSG_PRESNAME	"New conference name is"
#define MSG_COMCNAME	"New reply conference is"
#define MSG_NODELMBOX	"You cannot delete your mailbox."
#define MSG_DELCONF	"Do you really want to deleted"
#define MSG_CONFCALL	"Conference"
#define MSG_DELETED2	"deleted."
#define MSG_PURGED	"deleted."
#define MSG_NOINFO	"There is no information stored."
#define MSG_NOHELP	"There is no help for this command."
#define MSG_COMMAND	"Command:"
#define MSG_GREPPROMPT	"Search string: "
#define MSG_BADGREP	"Bad search string."
#define MSG_GREPNFOUND	"The search string was not found in this conference."
#define MSG_GGREPNFOUND	"No articles contained the search string."
#define MSG_MODPINFO	"Changing personal information for"
//#define MSG_MODPINFO2 "Samtliga uppgifter {r givetvis frivilliga, men fyller du i stad ser du v{dret hos dig i prompten.\n\n" /* Upcoming feature */
#define MSG_INFOADDR	"Address: "
#define MSG_INFOZIP	"Zip    : "
#define MSG_INFOTOWN	"Town   : "
#define MSG_INFOTELE1	"Phone 1: "
#define MSG_INFOTELE2	"Phone 2: "
#define MSG_INFOTELE3	"Phone 3: "
#define MSG_INFOMAIL1	"E-mail1: "
#define MSG_INFOMAIL2	"E-mail2: "
#define MSG_INFOURL     "URL    : "
#define MSG_INFOORG	"Organiz: "
#define MSG_SAVEINFO	"Save changes (Yes/No) ? "
#define MSG_INFOSAVED	"Changes saved."
#define MSG_INFONOSAVE	"Changes not saved."
#define MSG_ASKTIMEOUT	"Inactivity limit in minutes: "
#define MSG_TIMESET	"Inactivity limit set to"
#define MSG_NONE	"None"
#define MSG_FILENAME	"File(s): "
#define MSG_NOFILES	"No files."
#define MSG_FILEHEAD	"file                  size     description"
#define MSG_LINEROW	"------------------------------------------"
#define MSG_BADFILE	"File not found."
#define MSG_DESCRIBE	"Description: "
#define MSG_NOMAIL	"Cannot exec mail."
#define MSG_NOINEWS	"Cannot exec inews."
#define MSG_MAILED	"Mailed."
#define MSG_EMSUB	"Subject: "
#define MSG_EMFROM	"From: "
#define MSG_EMFROM2	"FROM: "
#define MSG_EMFROM3	"from: "
#define MSG_EMFROM4     "From " /* Newer mailprgs may need this, PL 2025-07-30 */
#define MSG_EMFROM5     "from " /* Newer mailprgs may need this, PL 2025-07-30 */
#define MSG_EMRETURN	"Return-Path: "
#define MSG_PRIO	"given high priority."
#define MSG_DEPRIO	"given low priority."
#define MSG_NOPRIOMBOX	"You cannot change the priority of your mailbox."
#define MSG_BACK0	"No articles backed."
#define MSG_BACK1	"One article backed."
#define MSG_BACK2	"articles backed."
#define MSG_POSTED	"News posted."
#define MSG_NGROUP	"Newsgroups:"
#define MSG_NOTINCONF	"You cannot reply to articles in other conferences."
#define MSG_NOSUBJ	"You must supply a subject."
#define MSG_MIMETYPE	"Mime-Version: 1.0"
#define MSG_MIMECONT	"Content-Type: text/plain; charset=\"iso-8859-1\""
#define MSG_MIMEENC	"Content-Transfer-Encoding: quoted-printable"
#define MSG_NOCMD	"Please supply a command."
#define MSG_NOALIAS     "Please supply an alias."
#define MSG_NOWHASALIAS "now has the alias" /* Missing language strings made the english binary crash, now fixed 2025-07-14 PL */
#define MSG_CMNTMOVED   "Comment automatically moved to "
#define MSG_YOUAREFROM  "You are now from "
#define MSG_NOWHERE     "nowhere"
#define MSG_UNREADUNREAD "\n%d text(s) marked unread.\n"
#define MSG_THECOMMAND  "The command" /* Missing language strings made the english binary crash, now fixed 2025-07-14 PL */
#define MSG_NUMLNSERR	"Invalid entry, try 10-200"
#define MSG_NUMLNSHELLO	"Type the number at the top (0 equals auto):"
#define MSG_NUMLNSFAIL	"Save failed, please contact SysOp!\n"
#define MSG_NUMLNSOK	"Number of rows set to %d.\n"
#define MSG_NUMLNCONF	"Same as before then.\n"
#define MSG_NO_ZORK     "No z3-file found, sorry"       /*Zork */
#define MSG_BADARG      "Oops bad input! Type Zork 1, or 2, or 3.\n\n"   /* Zork */
#define MSG_DISPTIME    "Current time is"
#define MSG_IT          "on the"
#define MSG_DISPTIME2   "and you've been here for"
#define MSG_BEATS1      "Internet time is now"
#define MSG_BEATS2      "and in Sweden"
#define MSG_BEATS3      "of the day has passed."



/* conf.c */

#define MSG_CONFSORT	"Sorting conference list..."
#define MSG_MAILBOX	"Mailbox"
#define MSG_CLOSED	"  (Closed)"
#define MSG_SECRET	"  (Secret)"
#define MSG_NEWS	"  (Usenet)"
#define MSG_MEMBERMARK	'*'
#define MSG_FILAREA	'F'
#define MSG_CONFCREATOR	'C'
#define MSG_TOTNULL	"No new articles."
#define MSG_TOTONE	"One new article."
#define MSG_TOTAL	"A total of"
#define MSG_WHERE2       "You're currently in"

/* edit.c */

#define MSG_REMTEXT	"Do you really want to delete current editing?"
#define MSG_EDCHANGE	"!change"
#define EDCHANGEN	1
#define MSG_EDREAD	"!read"
#define EDREADN		3
#define MSG_EDWHOLE	"!view"
#define EDWHOLEN	1
#define MSG_EDINCL	"!include"
#define EDINCLN		1
#define MSG_EDTELL   	"!tell"
#define EDTELLN		1
#define MSG_EDYELL	"!shout"
#define EDYELLN		2
#define MSG_EDYELL2	"!yell"
#define EDYELL2N	1
#define MSG_EDREVIEW	"!review"
#define EDREVIEWN	3
#define MSG_EDREMOVE	"!remove"
#define EDREMOVEN	3
#define MSG_EDSAVE	"!save"
#define EDSAVEN		2
#define MSG_EDPOST	"!post"
#define EDPOSTN		1
#define MSG_EDMOVE	"!move"
#define EDMOVEN		2
#define MSG_EDDEL	"!delete"
#define EDDELN		1
#define MSG_EDINS	"!insert"
#define EDINSN		3
#define MSG_EDQUOTE	"!quote"
#define EDQUOTEN	1
#define MSG_EDWHO	"!who"
#define EDWHO	        1
#define MSG_EDI         "!poppy"
#define EDI             3
#define MSG_EDMY         "!my"
#define EDMY             2
#define MSG_FNPROMPT	"Filename: ~/"
#define MSG_BADFNAME	"Bad filename."
#define MSG_OUTMEM	"Out of memory!"
#define MSG_FILEINC	"File included."
#define MSG_BADLINE	"Bad line number."
#define MSG_MOVED	"Text moved to "
#define MSG_EDHELP0	"\n!change <linenumber>\n"
#define MSG_EDHELP1	"!delete <linenumber>\n"
#define MSG_EDHELP2	"!insert (empty line before) <linenumber>\n"
#define MSG_EDHELP3	"!read (article)\n"
#define MSG_EDHELP4	"!view (article)\n"
#define MSG_EDHELP5	"!include <filename>\n"
#define MSG_EDHELP6	"!tell <user>, <message>\n"
#define MSG_EDHELP7	"!shout <message>\n"
#define MSG_EDHELP8	"!review <article number>\n"
#define MSG_EDHELP9	"!quote (article replied to)\n"
#define MSG_EDHELP9p5	"!who (is online)\n"
#define MSG_EDHELP10	"!move (to) <conference>\n"
#define MSG_EDHELP11	"!remove (article)\n"
#define MSG_EDHELP12	"!save (article)\n"
#define MSG_EDHELP13	"!post (article)\n\n"
#define MSG_EDHELP14   	"Ctrl-U/X deletes current line\n"
#define MSG_EDHELP15	"Ctrl-W deletes previous word\n"
#define MSG_EDHELP16	"Ctrl-Z/D saves article\n"
#define MSG_EDHELP17	"Ctrl-L displays article\n"
#define MSG_EDHELP18	"Ctrl-C removes article\n\n"
#define MSG_EDRECOVER	"Resuming an aborted editing session..."
#define MSG_NOSAVE	"You can't save your text here, please move it."
#define MSG_QUSE	"j = quote line, a = quote all lines, q = abort, any other key = next line"
#define MSG_NOBODYQUOTE	"Sorry, there's 'no body' to be found for quation\n"

#define MSG_SURVHELP1    "Question types for surveys\n\n"
#define MSG_SURVHELP2    "Free text      : ## text\n"
#define MSG_SURVHELP3    "Number         : ## number\n"
#define MSG_SURVHELP4    "Single choice  : ## single : <number of alternatives>\n"
#define MSG_SURVHELP5    "Multiple choice: ## multiple : <number of alternatives>\n"
#define MSG_SURVHELP6    "Interval       : ## interval : <lower range limit> : <upper range limit>\n\n"


/* msg.c */

#define MSG_YELLS	"shouts:"
#define MSG_LOGGEDIN	"has logged in."
#define MSG_LOGGEDOUT	"has logged out."
#define MSG_STRANGEMSG	"Unknown message type from"
#define MSG_NOSAY	"has turned off say messages."
#define MSG_NOYELL	"has turned off shout messages."
#define MSG_NOACTIVE	"is not logged in."
#define MSG_CONFMSG	"Message sent to"
#define MSG_IDLEWARNING "idle since"

/* flag.c */

#define MSG_FLAG0	"ibm-pc"
#define MSG_FLAG0N	2
#define MSG_FLAG0F	"IBM-PC (character set)"
#define MSG_FLAG1	"iso 8859-1"
#define MSG_FLAG1N	2
#define MSG_FLAG1F	"ISO 8859-1 (character set)"
#define MSG_FLAG2	"mac"
#define MSG_FLAG2N	1
#define MSG_FLAG2F	"Macintosh (character set)"
#define MSG_FLAG3	"presence"
#define MSG_FLAG3N	1
#define MSG_FLAG3F	"(Show) presence messages"
#define MSG_FLAG4    	"shouting"
#define MSG_FLAG4N	2
#define MSG_FLAG4F	"(Allow) shouting"
#define MSG_FLAG5	"logout"
#define MSG_FLAG5N	1
#define MSG_FLAG5F	"Logout (as default)"
#define MSG_FLAG6	"tell"
#define MSG_FLAG6N	1
#define MSG_FLAG6F	"(Allow) tell"
#define MSG_FLAG7	"subjectchange"
#define MSG_FLAG7N	2
#define MSG_FLAG7F	"Subject change (when replying)"
#define MSG_FLAG8	"space"
#define MSG_FLAG8N	2
#define MSG_FLAG8F	"Space (equals CR)"
#define MSG_FLAG9	"copy"
#define MSG_FLAG9N	2
#define MSG_FLAG9F	"Copy (of mail sent)"
#define MSG_FLAG10	"author"
#define MSG_FLAG10N	1
#define MSG_FLAG10F	"Author (after text)"
#define MSG_FLAG11	"date"
#define MSG_FLAG11N	1
#define MSG_FLAG11F	"Date (i textheader)"
#define MSG_FLAG12	"beeps"
#define MSG_FLAG12N	3
#define MSG_FLAG12F	"(Allow) beeps"
#define MSG_FLAG13	"clear"
#define MSG_FLAG13N	2
#define MSG_FLAG13F	"Clear (screen before textdisplay)"
#define MSG_FLAG14	"header"
#define MSG_FLAG14N	1
#define MSG_FLAG14F	"Header (when reading email/news)"
#define MSG_FLAG15	"strip"
#define MSG_FLAG15N	2
#define MSG_FLAG15F	"Strip (bit 8 in input)"
#define MSG_FLAG16	"bell"
#define MSG_FLAG16N	3
#define MSG_FLAG16F	"Bell (with present messages)"
#define MSG_FLAG17      "old"
#define MSG_FLAG17N     1
#define MSG_FLAG17F     "Old (who-list sort order)"
#define MSG_FLAG18      "ansi"
#define MSG_FLAG18N     2
#define MSG_FLAG18F      "ANSI colors"
#define MSG_FLAG19	"utf8"
#define MSG_FLAG19N	1
#define MSG_FLAG19F	"UTF-8 (character set)"
#define MSG_NOFLAG	"You must supply a flagname."
#define MSG_PCWARN	"You should turn off IBM-PC first."
#define MSG_ISOWARN	"You should turn off ISO 8859-1 first."
#define MSG_MACWARN	"You should turn off Macintosh first."
#define MSG_UTFWARN	"You should turn off UTF-8 first."
#define MSG_BADFLAG	"Bad flag."
#define MSG_FLAG	"The flag"
#define MSG_FLON	"turned on."
#define MSG_FLOFF	"turned off."

/* parse.c */

#define MSG_TEXTPROMPT2	"Read"
#define MSG_MULTICOM	"Ambiguous command:"
#define MSG_CHOSECOM	"These matched:"
#define MSG_COMERR	"Unknown command:"
#define MSG_HELPQ	"Type ? for help!"
#define MSG_ECONFN	"\nThere are no such conference.\n\n"
#define MSG_EUSERN	"\nThere are no such user.\n\n"
#define MSG_EUSERA	"\nThere are no logged in user by that name.\n\n"
#define MSG_ECONFS	"\nYou are not subscribing to a conference by that name.\n\n"
#define MSG_ECONFU	"\nYou are subscribing to a conference by that name.\n\n"
#define MSG_ECOMN	"\nWrong name.\n\n"
#define MSG_MULNAM	"Ambiguous name, these matched:"
#define MSG_MOREMUL	"(More exists but is not shown.)"
#define MSG_PICKLIST	"Supply number or name: "
#define MSG_BADPICK	"Error in input."

/* text.c */

#define MSG_FROMSUB	"From: "
#define MSG_TOSUB	"To:   "
#define MSG_TEXTNAME	"Article"
#define MSG_SURVEYNAME	"Survey"
#define MSG_WRITTENBY	"Written by"
#define MSG_TOFROM	"To/From"
#define MSG_EMPTYTEXT	"Empty article"
#define MSG_EMPTYSURVEY	"Empty survey"
#define MSG_ONELINE	"One line"
#define MSG_LINES	"lines"
#define MSG_REPLYIN	"Follow-up in"
#define MSG_REPLYTO	"Follow-up to article"
#define MSG_COPYTO	"Copy of mail to"
#define MSG_IN		"in"
#define MSG_BY		"by"
#define MSG_RECIPIENT	"To:"
#define MSG_CONF	"Conference:"
#define MSG_SUBJECT	"Subject: "
#define MSG_SUBJECT2	"Subject"
#define MSG_NOTEXTNUM	"You must supply a article number."
#define MSG_NOTEXT	"The article does not exist."
#define MSG_NOSURVEY	"The survey does not exist."
#define MSG_NOREAD	"Cannot read article."
#define MSG_EOT		"End of article"
#define MSG_EOSURVEY	"End of survey"
#define MSG_ERRTNUM	"Wrong article number."
#define MSG_LASTREAD	"last"
#define MSG_REPLIED	"followed-up"
#define MSG_LASTMSG	"msg"
#define MSG_LASTMSGERR	"You have not received a message."
#define MSG_NOLASTTEXT	"There is no last read article."
#define MSG_NOREPLY	"Article is not a follow-up."
#define MSG_CONFMISSING	"This should not happen."
#define MSG_BADAD	"Bad email address."
#define MSG_ERRCREATET	"Cannot create article."
#define MSG_UNKNOWNU	"(unknown)"
#define MSG_NQUESTIONS  "Number of questions"
#define MSG_REPORTRESULT "Results reported"
#define MSG_NOTASURVEY  "The text is not a survey"
#define MSG_NOTREPORTED "The survey is not yet reported"
#define MSG_RESULTFOR   "Result for survey"
#define MSG_NOREPLIES   "No replies"
#define MSG_ONEANSWER   "One reply"
#define MSG_ANSWER      "replies"
#define MSG_EORESULT    "End of report for survey"

/* user.c */

#define MSG_SYSOP	"System operator"
#define MSG_ROOTERR	"Root cannot run SklaffKOM for internal reasons."
#define MSG_ALREADYON	"is already logged in."
#define MSG_QUITASK	"Du you want to quit the other process? "
#define MSG_QUITTRY	"\nTrying to quit the other process..."
#define MSG_QUITFAIL	"failed."
#define MSG_QUITOK	"made it!"
#define MSG_KILLTRY	"Trying to kill the other process..."
#define MSG_KILLOK	"it probably went ok."
#define MSG_QUITEXIT	"Exiting."
#define MSG_NOPAR	" (No)"
#define MSG_USERS	"users."
#define MSG_USER_ALREADY_EXIST "User already axists."

/* survey.c */

#define MSG_SURVEYANS   "Your survey replies were"
#define MSG_SAVESURVANS "Save your replies"
#define MSG_SURVSAVED   "Replies saved"
#define MSG_SURVNOSAVE  "Replies not saved"
#define MSG_SURVDELIMIT1 "------------------------- Replies --------------------------"
#define MSG_SURVDELIMIT2 "------------------------------------------------------------"
#define MSG_WHENREPORT   "How long time from now should the survey result be reported?"
#define MSG_WHENFORMAT   "(days:hours:minutes)"
#define MSG_WRONGFORMAT  "Invalid time"
#define MSG_TOTANSWERS   "Totalt number of replies"
#define MSG_BLANKVOTES   "Blank replies"
#define MSG_NOANSWERS    "Number of replies"
#define MSG_ANSWER2      "Replies"
#define MSG_MEAN         "Mean"
#define MSG_STD          "Standard deviation"

/* sklaffadm.c */

#define MSG_INETNAME	"Internet users"
#define MSG_MODEMNAME	"Modempool users"
#define MSG_OTHERNAME	"Other users"
#define MSG_NONAME	"You must supply a user name."
#define MSG_NODATE	"You must supply a date."
#define MSG_NOMODEM	"This user is not a modempool user."
#define MSG_PAYDATE	"Paydate for"
#define MSG_CHANGED	"changed to"
#define MSG_DENY	"disabled."
#define MSG_ALLOW	"enabled."
#define MSG_ERRUID	"Wrong uid."
#define MSG_CONFHEAD	" Num     Articles       Number   Life  Conference"
#define MSG_FILEHEAD2	" Num     Files in KB     Conference"
#define MSG_TEXTS	"articles"
#define MSG_ONLINE	"online"
#define MSG_CTERR	"You must supply both conference and number of articles."
#define MSG_DELTEXTS	"articles deleted."
#define MSG_ARGERR	"Wrong argument."
#define MSG_ALREADYDOWN	"SklaffKOM is already down."
#define MSG_CAUSE	"Enter cause"
#define MSG_CREATED	"created"
#define MSG_NOTIFY	"Notifying users about SklaffKOM shutdown."
#define MSG_EXITNOW	"Logout at once, SklaffKOM is being shutdown in one minute!\n"
#define MSG_WAITMIN	"Waiting one minute"
#define MSG_THROWOUT	"Logging out all users."
#define MSG_THROW	"logged out."
#define MSG_DOWNREADY	"shutdown."
#define MSG_ALREADYUP	"is already open."
#define MSG_UPREADY	"opened."
#define MSG_LIFE1	"Lifetime for "
#define MSG_LIFE2	" set to "
#define MSG_LIFE3	" days."
#define MSG_ADMHELP1	"\nSyntax: sklaffadm [command] [parameter]\n\n"
#define MSG_ADMHELP2	"   userlist [date] - List users and status.\n"
#define MSG_ADMHELP3	"   disable \"user\" - Disable login for user.\n"
#define MSG_ADMHELP4	"   enable \"user\" - Enable login for user.\n"
#define MSG_ADMHELP5	"   paydate \"user\" date - Set paydate for user.\n\n"
#define MSG_ADMHELP6	"   conflist - List conference and article status.\n"
#define MSG_ADMHELP7	"   filelist - List total files in conferences.\n"
#define MSG_ADMHELP8	"   maillist - List mailboxes and mail status.\n"
#define MSG_ADMHELP9	"   life <conf> <num> - Sets life-time for texts in <conf>, 0 = eternal.\n"
#define MSG_ADMHELP10	"   maillife <uid> <num> - Sets life-time for texts in <uid>, 0 = eternal.\n"
#define MSG_ADMHELP11	"   purge <conf> <num> - Purges articles, leaves at leat <num> articles.\n"
#define MSG_ADMHELP12	"                        conf = -1 purges all conferences.\n"
#define MSG_ADMHELP13	"   mailpurge <uid> <num> - Purges mail, leaves at least <num> mails.\n"
#define MSG_ADMHELP14	"                           mbox = -1 purges in all mailboxes.\n\n"
#define MSG_ADMHELP15	"   who - Display active users.\n"
#define MSG_ADMHELP16	"   down - Shuts down SklaffKOM properly.\n"
#define MSG_ADMHELP17	"   up - Opens SklaffKOM for login.\n\n"

/* sklaffacct.c */

#define MSG_INNAME	"First and last name    : "
#define MSG_INLOGIN	"Login name             : "
#define MSG_INPASSWD	"Password               : "
#define MSG_INMODEM	"Modempool-access       : "
#define MSG_INTELE	"Voice telephone	: "
#define MSG_INPOST      "E-mail address         : "
#define MSG_ACCAPP	"Account application: "
#define MSG_INSNAME	"Name     : "
#define MSG_INSLOGIN	"\nLogin    : "
#define MSG_INSPASSWD	"\nPasswd   : "
#define MSG_INSMODEM	"\nModem    : "
#define MSG_INSTELE	"\nTelephone: "
#define MSG_INSPOST     "\nE-mail : "
#define MSG_APPLIED	"Your application has been posted.\nWelcome back.\n\n"
#define MSG_UIDINUSE	"login-name in use by another user."

/* mailtoss.c */

#define MSG_MTINFO	"Syntax: mailtoss <user>"

/* newstoss.c */

#define MSG_NTINFO	"Syntax: newstoss <newsgroup>"
#define MSG_BADACTIVE	"Cannot open active file."
#define MSG_NOGROUP	"No such newsgroup."
#define MSG_MSGID	"Message-ID:"
#define MSG_REFID	"References:"

/* survreport.c */

#define MSG_SRINFO      "Syntax: survreport <confid> <survey no>"
#define MSG_NOREPORT    "The survey should not yet be reported"
#define MSG_REPORT      "Report: "

#endif
