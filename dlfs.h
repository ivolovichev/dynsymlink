#ifndef DLFS_H
#define DLFS_H
#define FUSE_USE_VERSION 31

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <assert.h>
#include <bits/posix1_lim.h>
#include <linux/limits.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <pwd.h>
#include <fuse.h>

#define DEFAULT_CONFIG_FNAME "~/.config/dynlink/config"
#define DEFAULT_TARGET "/dev/null"

#define OPTION(t, p) { t, offsetof(struct _options, p), 1 }

enum dlfsACLType {COMM, UID, GID, EXTERN, ANY, EVERY, NOT};
struct _dlfsSymlink;
struct _dlfsTarget;
struct _dlfsACL;

typedef struct _dlfsACL
{
    enum    dlfsACLType type;
    void    *pattern;
    int     plen;
    struct  _dlfsACL *next;
} dlfsACL;

typedef struct _dlfsTarget
{
    char    *name;
    struct  _dlfsACL *acl;
    struct  _dlfsTarget *next;
} dlfsTarget;

typedef struct _dlfsSymlink
{
    char *name;
    char *def_target;
    char *log;
    struct _dlfsTarget *target;
    struct _dlfsSymlink *next;
} dlfsSymlink;

extern int readConfig(const char*, dlfsSymlink**);
extern void freeConfig();

#endif // DLFS_H
