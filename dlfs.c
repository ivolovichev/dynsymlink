/* This program partly uses example code from
   the prominent libfuse original distribution */

#include "dlfs.h"

struct _options {
    const char *config;
    int show_help;
};

static time_t t_init;
static struct _options options;

static const struct fuse_opt option_spec[] = {
    OPTION("-c %s", config),
    OPTION("--config=%s", config),
	OPTION("-h", show_help),
	OPTION("--help", show_help),
	FUSE_OPT_END
};

uid_t myuid;
gid_t mygid;

static dlfsSymlink *DynLinks;

static void free_acl(dlfsACL*);
static int applyACL(dlfsACL*, const char*, struct fuse_context*);

static void *dlfs_init(struct fuse_conn_info *conn,
			struct fuse_config *cfg)
{
	(void) conn;
    myuid = getuid();
    mygid = getgid();
    cfg->attr_timeout = 0;
    cfg->direct_io = 1;
	return NULL;
}

static void dlfs_destroy(void *data)
{
    (void) data;
    dlfsSymlink *link, *next;
    dlfsTarget *t, *t_next;

    for(link = DynLinks; link != NULL; link = next) {
        free(link->name);
        free(link->def_target);
        if(!link->log) free(link->log);
        for(t = link->target; t != NULL; t = t_next) {
            free(t->name);
            free_acl(t->acl);
            t_next = t->next;
            free(t);
        }
        next = link->next;
        free(link);
    }
}

static void free_acl(dlfsACL *acl)
{
    dlfsACL *a;
    int i;

    for(; acl != NULL; acl = a) {
        switch(acl->type) {
            case COMM:
                for(i = 0; i < acl->plen; i++) free(((char**)acl->pattern)[i]);
                free(acl->pattern);
                break;

            case UID:
            case GID:
            case EXTERN:
                free(acl->pattern);
                break;

            case ANY:
            case EVERY:
            case NOT:
                free_acl((dlfsACL *)acl->pattern);
                break;
        }
        a = acl->next;
        free(acl);
    }
}

static int dlfs_getattr(const char *path, struct stat *stbuf,
			 struct fuse_file_info *fi)
{
	(void) fi;

    int res = 0;
    time_t t;
    int not_found = 1;
    dlfsSymlink *link;
    struct fuse_context *ctx = fuse_get_context();
    t = time(NULL);

    memset(stbuf, 0, sizeof(struct stat));
	if (strcmp(path, "/") == 0) {
		stbuf->st_mode = S_IFDIR | 0755;
        stbuf->st_nlink = 2;
        stbuf->st_uid = myuid;
        stbuf->st_gid = mygid;
        stbuf->st_ctim.tv_sec = t_init;
        stbuf->st_atim.tv_sec = t;
        stbuf->st_mtim.tv_sec = t;
        }
    else {
        for(link = DynLinks; link != NULL; link = link->next)
            if(strcmp(path+1, link->name) == 0) {
                not_found = 0;
                break;
                }

        if(!not_found) {
            stbuf->st_mode = S_IFLNK | 0777;
            stbuf->st_nlink = 1;
            stbuf->st_size = _POSIX_SYMLINK_MAX;
            stbuf->st_uid = ctx->uid;
            stbuf->st_gid = ctx->gid;
            stbuf->st_ctim.tv_sec = t_init;
            stbuf->st_atim.tv_sec = t;
            stbuf->st_mtim.tv_sec = t;
            }
        else
            res = -ENOENT;
        }

return res;
}

static int dlfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
			 off_t offset, struct fuse_file_info *fi,
			 enum fuse_readdir_flags flags)
{
	(void) offset;
	(void) fi;
	(void) flags;
    dlfsSymlink *link;

	if (strcmp(path, "/") != 0)
		return -ENOENT;

	filler(buf, ".", NULL, 0, 0);
	filler(buf, "..", NULL, 0, 0);
    for(link = DynLinks; link != NULL; link = link->next) {
        filler(buf, link->name, NULL, 0, 0);
        }

	return 0;
}

static int dlfs_readlink(const char *path, char *buf, size_t size)
{
    size_t len, offt = 0;
    dlfsSymlink *link;
    dlfsTarget *t;
    char *target, *home;
    FILE *log;
    struct fuse_context *ctx = fuse_get_context();

    for(link = DynLinks; link != NULL; link = link->next)
        if(strcmp(path+1, link->name) == 0) break;

    if(link == NULL) return -ENOENT;

    target = NULL;
    for(t = link->target; t != NULL; t = t->next) {
        if(applyACL(t->acl, path, ctx)) {
            target = t->name;
            break;
        }
    }

    if(link->log) {
        log = fopen(link->log, "a");
        if(log) fprintf(log, "%s resolved as %s%s by PID/UID/GID=%u/%u/%u\n",
                        path,
                        target ? "" : "default target ",
                        target ? target : link->def_target,
                        ctx->pid,
                        ctx->uid,
                        ctx->gid);
        if(log) fclose(log);
    }

    if(target == NULL) target = link->def_target;

    if(target[0] == '~') {
        home = getpwuid(ctx->uid)->pw_dir;
        offt = strlen(home);
        if(offt > size - 1) offt = size - 1;
        memcpy(buf, home, offt);
        buf[offt] = '\0';
        target++;
    }

    len = strlen(target);
    if(len > size - 1 - offt) len = size - 1 - offt;
    memcpy(buf + offt, target, len);
    buf[offt + len] = '\0';

    return 0;
}

static const struct fuse_operations dlfs_oper = {
    .init       = dlfs_init,
    .destroy    = dlfs_destroy,
    .getattr	= dlfs_getattr,
    .readdir	= dlfs_readdir,
    .readlink   = dlfs_readlink,
};

static int applyACL(dlfsACL *acl, const char *path, struct fuse_context *ctx)
{
    int i, fd, status, ret = 0;
    pid_t p;
    dlfsACL *a;
    char buf[PATH_MAX], mypidstr[8], myuidstr[12], mygidstr[12];

    if(acl == NULL) return 0;

    switch(acl->type) {
        case COMM:
            if(acl->pattern == NULL) return 0;
            sprintf(buf, "/proc/%u/comm", ctx->pid);
            fd = open(buf, 0);
            if(fd < 0) return 0;
            i = read(fd, buf, PATH_MAX);
            close(fd);
            if(i <= 1) return 0;
            buf[i - 1] = '\0';
            for(i = 0; i < acl->plen; i++) if(strcmp(((char **)acl->pattern)[i], buf) == 0) return 1;
            break;

        case UID:
            if(acl->pattern == NULL) return 0;
            for(i = 0; i < acl->plen; i++) if(((unsigned int *)acl->pattern)[i] == ctx->uid) return 1;
            break;

        case GID:
            if(acl->pattern == NULL) return 0;
            for(i = 0; i < acl->plen; i++) if(((unsigned int*)acl->pattern)[i] == ctx->gid) return 1;
            break;

        case EXTERN:
            if(acl->pattern == NULL) return 0;
            p = fork();
            if(p == -1) {
                perror("Fork failed");
                return 0;
                }
            else if(p == 0) {
                sprintf(mypidstr, "%u", ctx->pid);
                sprintf(myuidstr, "%u", ctx->uid);
                sprintf(mygidstr, "%u", ctx->gid);
                execl((char *)acl->pattern, (char *)acl->pattern,
                                path, mypidstr, myuidstr, mygidstr, (char *)0);
                perror("Execution failed");
                return 0;
                }
            if(waitpid(p, &status, 0) == -1 ) {
                perror("Waitpid failed");
                return 0;
                }
            if(WIFEXITED(status) && WEXITSTATUS(status) == 0)
                return 1;
            return 0;
            break;

        case ANY:
            ret = 0;
            for(a = (dlfsACL *)acl->pattern; a != NULL; a = a->next) {
                ret |= applyACL(a, path, ctx);
                if(ret) return 1;
                }
            break;

        case EVERY:
            ret = 1;
            for(a = (dlfsACL *)acl->pattern; a != NULL; a = a->next) {
                ret &= applyACL(a, path, ctx);
                if(!ret) return 0;
                }
            break;

        case NOT:
            ret = 0;
            if(acl->pattern) return !applyACL((dlfsACL *)acl->pattern, path, ctx);
            break;
    }

    return ret;
}

static void show_help(const char *progname)
{
    printf("Usage: %s [options] <mountpoint>\n\n", progname);
    printf("Dynamic symlink filesystem specific options:\n"
           "\t-c <s> | --config=<s>\t Configuration file\n"
           "\t                     \t (default: %s)\n"
           "\t-h | --help          \t Show this screen\n"
           "\n", DEFAULT_CONFIG_FNAME);
}

int main(int argc, char *argv[])
{
	int ret;

    t_init = time(NULL);

    /* Parse options */
    struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
    options.config = strdup(DEFAULT_CONFIG_FNAME);
	if (fuse_opt_parse(&args, &options, option_spec, NULL) == -1)
		return 1;

    if (options.show_help) {
		show_help(argv[0]);
        fuse_opt_add_arg(&args, "--help");
		args.argv[0][0] = '\0';
        }
    else {
        if (readConfig(options.config, &DynLinks) < 0) return 1;
        }

    ret = fuse_main(args.argc, args.argv, &dlfs_oper, NULL);

    freeConfig();
	fuse_opt_free_args(&args);

    return ret;
}

