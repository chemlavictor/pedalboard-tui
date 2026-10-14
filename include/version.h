/* Version du build : macros injectées par le Makefile (-DTPD_GIT_HASH,
 * -DTPD_BUILD_DATE), avec repli si absentes (build hors git / test). */
#ifndef TPD_VERSION_H
#define TPD_VERSION_H

#ifndef TPD_GIT_HASH
#define TPD_GIT_HASH "nogit"
#endif
#ifndef TPD_BUILD_DATE
#define TPD_BUILD_DATE "1970.01.01"
#endif

#endif /* TPD_VERSION_H */
