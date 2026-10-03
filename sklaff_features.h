/* sklaff_features.h
 *
 * Optional SklaffKOM features.
 *
 * Set features to 1 to enable or 0 to disable.
 * Dependencies may still be required for an enabled feature.
 */

#ifndef SKLAFF_FEATURES_H
#define SKLAFF_FEATURES_H

/*
 * QWK batch download support.
 *
 * This feature is disabled by default until the legacy QWK
 * implementation has been tested on modern systems.
 */
#ifndef ENABLE_QWK
#define ENABLE_QWK 0
#endif

/*
 * BBSLink door games.
 *
 * Requires a working BBSLink installation.
 * UTF-8 sessions also require the CP437 wrapper.
 */
#ifndef ENABLE_BBSLINK
#define ENABLE_BBSLINK 0
#endif

/*
 * Zork / Infocom games.
 *
 * Requires Frotz and the appropriate game data files.
 */
#ifndef ENABLE_ZORK
#define ENABLE_ZORK 1
#endif

/*
 * Nethack.
 *
 * Requires a working Nethack executable.
 */
#ifndef ENABLE_NETHACK
#define ENABLE_NETHACK 1
#endif

#endif /* SKLAFF_FEATURES_H */
