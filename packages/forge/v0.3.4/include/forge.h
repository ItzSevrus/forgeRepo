#ifndef FORGE_H
#define FORGE_H

#define _POSIX_C_SOURCE 200809L
#include <stddef.h>
#include <stdio.h>
#include <limits.h>
#include <sys/types.h>
#include <stdbool.h>

#define FORGE_VERSION "0.3.3"
#define FORGE_MAX_REPOS 256
#define FORGE_MAX_NAME 64
#define FORGE_MAX_URL 2048
#define FORGE_MAX_FIELD 2048
#define FORGE_MAX_DEPS 64
#define FORGE_MAX_DEPTH 32

typedef struct {
    char name[FORGE_MAX_NAME];
    char url[FORGE_MAX_URL];
} ForgeRepo;

typedef struct {
    char name[FORGE_MAX_NAME];
    char version[128];
    char description[512];
    char license[128];
    char source_url[FORGE_MAX_URL];
    char checksum[256];
    char build_script[256];
    char build_command[512];
    char compilers[16][FORGE_MAX_NAME];
    size_t compiler_count;
    char apt_deps[FORGE_MAX_DEPS][FORGE_MAX_NAME];
    size_t apt_dep_count;
    char pacman_deps[FORGE_MAX_DEPS][FORGE_MAX_NAME];
    size_t pacman_dep_count;
    char dnf_deps[FORGE_MAX_DEPS][FORGE_MAX_NAME];
    size_t dnf_dep_count;
    char zypper_deps[FORGE_MAX_DEPS][FORGE_MAX_NAME];
    size_t zypper_dep_count;
    char runtime_deps[FORGE_MAX_DEPS][FORGE_MAX_NAME];
    size_t runtime_dep_count;
    char build_deps[FORGE_MAX_DEPS][FORGE_MAX_NAME];
    size_t build_dep_count;
} ForgePackage;

typedef struct {
    char config_dir[PATH_MAX];
    char data_dir[PATH_MAX];
    char cache_dir[PATH_MAX];
    char prefix[PATH_MAX];
    char registry_path[PATH_MAX];
    char settings_path[PATH_MAX];
} ForgeConfig;

typedef struct {
    char *data;
    size_t len;
    size_t cap;
    long status;
} ForgeBuffer;

int forge_init(ForgeConfig *cfg);
int forge_setup(const ForgeConfig *cfg);
int forge_config_command(const ForgeConfig *cfg, int argc, char **argv);
int forge_repo_command(const ForgeConfig *cfg, int argc, char **argv);
int forge_load_repos(const ForgeConfig *cfg, ForgeRepo repos[], size_t *count);
int forge_search(const ForgeConfig *cfg, const char *query);
int forge_info(const ForgeConfig *cfg, const char *name);
int forge_install(const ForgeConfig *cfg, const char *name, int assume_yes);
int forge_uninstall(const ForgeConfig *cfg, const char *name);
int forge_list_installed(const ForgeConfig *cfg);
int forge_upgrade(const ForgeConfig *cfg, const char *name);
int forge_clean_cache(const ForgeConfig *cfg);
int forge_resolve_package(const ForgeConfig *cfg, const char *name, char *repo_name, size_t repo_sz, char *repo_url, size_t url_sz, char *path, size_t path_sz, char *commit, size_t commit_sz, ForgePackage *pkg);
int forge_manifest_parse(const char *text, ForgePackage *pkg, char *err, size_t errsz);
int forge_manifest_validate(const ForgePackage *pkg, char *err, size_t errsz);

int forge_mkdir_p(const char *path, mode_t mode);
int forge_path_join(char *out, size_t size, const char *a, const char *b);
int forge_write_file(const char *path, const char *data, size_t len, mode_t mode);
char *forge_read_file(const char *path, size_t *len_out);
int forge_http_get(const char *url, ForgeBuffer *out, const char *accept, bool supress);
void forge_buffer_free(ForgeBuffer *b);
int forge_github_parts(const char *url, char *owner, size_t owner_sz,
                       char *repo, size_t repo_sz);
int forge_json_string(const char *json, const char *key, char *out, size_t outsz);
int forge_valid_name(const char *name);
int forge_copy_tree(const char *src, const char *dst, const char *prefix,
                    FILE *manifest, int reject_existing);
int forge_remove_tree_files(const char *prefix, const char *manifest_path);
int forge_run_build(const char *command, const char *workdir, const char *stage,
                    const char *prefix, const char *source_dir);
int forge_sha256_file(const char *path, char out[65]);

#endif
