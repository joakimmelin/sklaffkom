/* sklaff_conf.h
 *
 * Conference type definitions and helpers for SklaffKOM.
 */

#ifndef SKLAFF_CONF_H
#define SKLAFF_CONF_H

/*
 * Conference type values are stored persistently and must not be renumbered.
 *
 * Values 0..4 retain their historic meaning.  The helpers below separate
 * the two concepts encoded in the stored integer:
 *
 *   transport: local / news / FTN
 *   access:    open / closed / secret
 */

#define OPEN_CONF           0
#define CLOSED_CONF         1
#define SECRET_CONF         2
#define NEWS_CONF           3
#define FTN_CONF            4

#define NEWS_CLOSED_CONF    5
#define NEWS_SECRET_CONF    6
#define FTN_CLOSED_CONF     7
#define FTN_SECRET_CONF     8

static inline int
conf_access_type(int type)
{
    switch (type) {
    case OPEN_CONF:
    case NEWS_CONF:
    case FTN_CONF:
        return OPEN_CONF;

    case CLOSED_CONF:
    case NEWS_CLOSED_CONF:
    case FTN_CLOSED_CONF:
        return CLOSED_CONF;

    case SECRET_CONF:
    case NEWS_SECRET_CONF:
    case FTN_SECRET_CONF:
        return SECRET_CONF;

    default:
        return -1;
    }
}

static inline int
conf_is_news(int type)
{
    return type == NEWS_CONF ||
           type == NEWS_CLOSED_CONF ||
           type == NEWS_SECRET_CONF;
}

static inline int
conf_is_ftn(int type)
{
    return type == FTN_CONF ||
           type == FTN_CLOSED_CONF ||
           type == FTN_SECRET_CONF;
}

static inline int
conf_is_external(int type)
{
    return conf_is_news(type) || conf_is_ftn(type);
}

/*
 * Return the canonical transport for a combined conference type.
 * OPEN_CONF represents local transport here.
 */
static inline int
conf_transport_type(int type)
{
    if (conf_is_news(type))
        return NEWS_CONF;

    if (conf_is_ftn(type))
        return FTN_CONF;

    if (conf_access_type(type) >= 0)
        return OPEN_CONF;

    return -1;
}

/*
 * Combine transport and access into the single integer stored in CONF_FILE.
 */
static inline int
conf_make_type(int transport, int access)
{
    if (access != OPEN_CONF &&
        access != CLOSED_CONF &&
        access != SECRET_CONF)
        return -1;

    switch (transport) {
    case OPEN_CONF:
        return access;

    case NEWS_CONF:
        if (access == CLOSED_CONF)
            return NEWS_CLOSED_CONF;
        if (access == SECRET_CONF)
            return NEWS_SECRET_CONF;
        return NEWS_CONF;

    case FTN_CONF:
        if (access == CLOSED_CONF)
            return FTN_CLOSED_CONF;
        if (access == SECRET_CONF)
            return FTN_SECRET_CONF;
        return FTN_CONF;

    default:
        return -1;
    }
}

#endif /* SKLAFF_CONF_H */
