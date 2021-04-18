#include <json.h>
#include "dlfs.h"

extern dlfsSymlink *DynLinks;
static int parse_json_acl(json_object*, dlfsACL**);

#ifdef DLFS_PRINT_CONFIG
void print_config(dlfsSymlink*);
static void printACL(dlfsACL*, int);
#endif

int readConfig(const char *cfile, dlfsSymlink **pcSymLink)
{
    dlfsSymlink *cSymLink;
    dlfsTarget *cTrgt, **pcTrgt;
    int n;
    size_t len;
#ifdef DLFS_PRINT_CONFIG
    dlfsSymlink **DynLinks = pcSymLink;
#endif
    const char *str;
    char buf[PATH_MAX], *home;

    if(cfile[0] == '~') {
        home = getpwuid(getuid())->pw_dir;
        len = strlen(home);
        strcpy(buf, home);
        strcpy(buf + len, cfile + 1);
        cfile = buf;
    }

    json_object *root = json_object_from_file(cfile);
    if(!root) {
        perror("Can't parse configuration file");
        return -1;
    }

    json_object_object_foreach(root, skey, sval) {
        if(json_object_get_type(sval) != json_type_object) {
            fprintf(stderr, "Invalid json type in symlink definition\n");
             return -1;
            }

        cSymLink = (dlfsSymlink *) malloc(sizeof(dlfsSymlink));
        if(!cSymLink) {
            fprintf(stderr, "Can't allocate memory\n");
             return -1;
            }

        *pcSymLink = cSymLink;
        pcSymLink = &cSymLink->next;
        cSymLink->next = NULL;
        cSymLink->target = NULL;
        cSymLink->log = NULL;
        pcTrgt = &cSymLink->target;
        if(strlen(skey) < 1) {
            fprintf(stderr, "Invalid (empty) symlink definition\n");
            return -1;
        }
        cSymLink->name = strdup(skey);
        if(!cSymLink->name) {
            fprintf(stderr, "Can't allocate memory\n");
             return -1;
            }

        json_object_object_foreach(sval, tkey, tval) {
            if(strlen(tkey) < 1) {
                fprintf(stderr, "Invalid parameter in definition of symlink %s \n", skey);
                return -1;
            }

            switch(json_object_get_type(tval)) {
                case json_type_string:
                    str = json_object_get_string(tval);
                    if(strlen(str) < 1) {
                        fprintf(stderr, "Empty value for parameter %s, symlink: %s\n", tkey, skey);
                        return -1;
                    }

                    if(strcasecmp(tkey, "default") == 0) {
                        cSymLink->def_target = strdup(str);
                        if(!cSymLink->def_target) {
                            fprintf(stderr, "Can't allocate memory\n");
                             return -1;
                            }

                    }
                    else if(strcasecmp(tkey, "log") == 0) {
                        cSymLink->log = strdup(str);
                        if(!cSymLink->log) {
                            fprintf(stderr, "Can't allocate memory\n");
                             return -1;
                            }
                    }
                    else {
                        fprintf(stderr, "Invalid parameter %s for symlink %s\n", tkey, skey);
                        return -1;
                    }
                    break;

                case json_type_object:
                    cTrgt = (dlfsTarget *) malloc(sizeof(dlfsTarget));
                    if(!cTrgt) {
                        fprintf(stderr, "Can't allocate memory\n");
                         return -1;
                        }
                    *pcTrgt = cTrgt;
                    pcTrgt = &cTrgt->next;
                    cTrgt->next = NULL;
                    cTrgt->name = strdup(tkey);
                    if(!cTrgt->name) {
                        fprintf(stderr, "Can't allocate memory\n");
                         return -1;
                        }

                    n = parse_json_acl(tval, &cTrgt->acl);
                    if(n < 0) {
                        fprintf(stderr, "Invalid ACL for target %s, symlink %s\n", tkey, skey);
                        return -1;
                    }
                    if(n > 1 && cTrgt->acl->type != ANY && cTrgt->acl->type != EVERY) {
                        fprintf(stderr, "Mutiple ACLs must be inside ANY/EVERY clasuse (target %s, symlink %s)\n", tkey, skey);
                        return -1;
                    }
                    break;

                default:
                    fprintf(stderr, "Invalid json object in symlink definition %s\n", skey);
                    return -1;
                    break;
            }
        }

        if(!cSymLink->def_target) cSymLink->def_target = strdup(DEFAULT_TARGET);
        if(!cSymLink->def_target) {
            fprintf(stderr, "Can't allocate memory\n");
             return -1;
        }
    }

    json_object_put(root);

#ifdef DLFS_PRINT_CONFIG
    print_config(*DynLinks);
#endif
    return 0;
}

static int parse_json_acl(json_object *tval, dlfsACL **pacl)
{
    dlfsACL *acl;
    int i, n = 0;

    json_object_object_foreach(tval, key, val) {
        if(strlen(key) < 1) {
            fprintf(stderr, "Empty ACL type\n");
            return -1;
        }
        while(1) {
            if(strcasecmp(key, "COMM") == 0){
                if(json_object_get_type(val) != json_type_array) {
                    fprintf(stderr, "Invalid JSON type for ACL COMM\n");
                    return -1;
                }
                acl = (dlfsACL *)malloc(sizeof(dlfsACL));
                if(!acl) {
                    fprintf(stderr, "Can't allocate memory\n");
                     return -1;
                    }
                acl->next = NULL;
                *pacl = acl;
                pacl = &acl->next;
                acl->type = COMM;
                acl->plen = json_object_array_length(val);
                acl->pattern = (char**)malloc(acl->plen * sizeof(char*));
                if(!acl->pattern) {
                    fprintf(stderr, "Can't allocate memory\n");
                     return -1;
                    }
                for(i = 0; i < acl->plen; i++) {
                    ((char **)acl->pattern)[i] = strdup(json_object_get_string(json_object_array_get_idx(val, i)));
                    if(!((char **)acl->pattern)[i]) {
                        fprintf(stderr, "Can't allocate memory\n");
                         return -1;
                    }
                }
                n++;
                break;
            }

            if(strcasecmp(key, "UID") == 0 || strcasecmp(key, "GID") == 0){
                if(json_object_get_type(val) != json_type_array) {
                    fprintf(stderr, "Invalid JSON type for ACL UID/GID\n");
                    return -1;
                }
                acl = (dlfsACL *)malloc(sizeof(dlfsACL));
                if(!acl) {
                    fprintf(stderr, "Can't allocate memory\n");
                     return -1;
                    }
                acl->next = NULL;
                *pacl = acl;
                pacl = &acl->next;
                acl->type = strcasecmp(key, "UID") == 0 ? UID : GID;
                acl->plen = json_object_array_length(val);
                acl->pattern = (unsigned int*)malloc(acl->plen * sizeof(unsigned int));
                if(!acl->pattern) {
                    fprintf(stderr, "Can't allocate memory\n");
                     return -1;
                    }
                for(i = 0; i < acl->plen; i++) {
                    ((unsigned int *)acl->pattern)[i] = json_object_get_int(json_object_array_get_idx(val, i));
                }
                n++;
                break;
            }

            if(strcasecmp(key, "EXTERN") == 0){
                if(json_object_get_type(val) != json_type_string) {
                    fprintf(stderr, "Invalid JSON type for ACL EXTERN\n");
                    return -1;
                }
                acl = (dlfsACL *)malloc(sizeof(dlfsACL));
                if(!acl) {
                    fprintf(stderr, "Can't allocate memory\n");
                     return -1;
                    }
                acl->next = NULL;
                *pacl = acl;
                pacl = &acl->next;
                acl->type = EXTERN;
                acl->plen = 0;
                acl->pattern = strdup(json_object_get_string(val));
                if(!acl->pattern) {
                    fprintf(stderr, "Can't allocate memory\n");
                    return -1;
                }
                n++;
                break;
            }

            if(strcasecmp(key, "ANY") == 0 || strcasecmp(key, "EVERY") == 0){
                if(json_object_get_type(val) != json_type_object) {
                    fprintf(stderr, "Invalid JSON type for ACL ANY/EVERY\n");
                    return -1;
                }
                acl = (dlfsACL *)malloc(sizeof(dlfsACL));
                if(!acl) {
                    fprintf(stderr, "Can't allocate memory\n");
                     return -1;
                    }
                acl->next = NULL;
                *pacl = acl;
                pacl = &acl->next;
                acl->type = strcasecmp(key, "ANY") == 0 ? ANY : EVERY;
                acl->pattern = (unsigned int*)malloc(sizeof(dlfsACL));
                if(!acl->pattern) {
                    fprintf(stderr, "Can't allocate memory\n");
                     return -1;
                    }
                acl->plen = parse_json_acl(val, (dlfsACL**)&acl->pattern);
                if(acl->plen < 0) {
                    fprintf(stderr, "Error parsing ANY/EVERY clause\n");
                     return -1;
                    }
                if(acl->plen < 2) {
                    fprintf(stderr, "ACL of ANY/EVERY type must contain at least two ACLs\n");
                     return -1;
                    }
                n++;
                break;
            }

            if(strcasecmp(key, "NOT") == 0){
                if(json_object_get_type(val) != json_type_object) {
                    fprintf(stderr, "Invalid JSON type for NOT ACL\n");
                    return -1;
                }
                acl = (dlfsACL *)malloc(sizeof(dlfsACL));
                if(!acl) {
                    fprintf(stderr, "Can't allocate memory\n");
                     return -1;
                    }
                acl->next = NULL;
                *pacl = acl;
                pacl = &acl->next;
                acl->type = NOT;
                acl->pattern = (unsigned int*)malloc(sizeof(dlfsACL));
                if(!acl->pattern) {
                    fprintf(stderr, "Can't allocate memory\n");
                     return -1;
                    }
                acl->plen = parse_json_acl(val, (dlfsACL**)&acl->pattern);
                if(acl->plen < 0) {
                    fprintf(stderr, "Error parsing NOT clause\n");
                     return -1;
                    }
                if(acl->plen != 1) {
                    fprintf(stderr, "ACL of NOT type must contain exactly one ACL\n");
                     return -1;
                    }
                n++;
                break;
            }
        fprintf(stderr, "Invalid ACL type %s\n", key);
        return -1;
        }
    }
    return n;
}

void freeConfig()
{

}

#ifdef DLFS_PRINT_CONFIG
void print_config(dlfsSymlink *Links)
{
    dlfsSymlink *link;
    dlfsTarget *t;

    if(Links == NULL) {
        printf("No symlink configured\n");
        return;
        }

    for(link = Links; link != NULL; link = link->next) {
        fprintf(stderr,"\nSymlink %s: default target %s", link->name, link->def_target);
        if(link->log) printf("(logged to %s)", link->log);
        printf("\n");

        for(t = link->target; t != NULL; t = t->next) {
            printf("   Target %s to be selected by ACLs:\n", t->name);
            printACL(t->acl, 6);
            }
        }
}

static void printACL(dlfsACL *acl, int ind)
{
    int i;

    if(acl == NULL) return;

    for(; acl != NULL; acl = acl->next) {
        switch(acl->type) {
        case COMM:
            printf("%*s COMM in [", ind, "");
            for(i = 0; i < acl->plen - 1; i++)
                printf("\"%s\", ", ((char **)acl->pattern)[i]);
            if(acl->plen > 0)
                printf("\"%s\"", ((char **)acl->pattern)[acl->plen - 1]);
            printf("]\n");
            break;

        case UID:
            printf("%*s UID in [", ind, "");
            for(i = 0; i < acl->plen - 1; i++)
                printf("%u, ", ((int *)acl->pattern)[i]);
            if(acl->plen > 0)
                printf("%u", ((int *)acl->pattern)[acl->plen - 1]);
            printf("]\n");
            break;

        case GID:
            printf("%*s GID in [", ind, "");
            for(i = 0; i < acl->plen - 1; i++)
                printf("%u, ", ((int *)acl->pattern)[i]);
            if(acl->plen > 0)
                printf("%u", ((int *)acl->pattern)[acl->plen - 1]);
            printf("]\n");
            break;

        case EXTERN:
            printf("%*s EXTERN %s\n", ind, "", (char *)acl->pattern);
            break;

        case ANY:
            printf("%*s ANY of:\n", ind, "");
            if(acl->pattern) printACL((dlfsACL *)acl->pattern, ind + 3);
            break;

        case EVERY:
            printf("%*s EVERY of:\n", ind, "");
            if(acl->pattern) printACL((dlfsACL *)acl->pattern, ind + 3);
            break;

        case NOT:
            printf("%*s Negate ACL:\n", ind, "");
            if(acl->pattern) printACL((dlfsACL *)acl->pattern, ind + 3);
            break;
        }
    }
}
#endif
