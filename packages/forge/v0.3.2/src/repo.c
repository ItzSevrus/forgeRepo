#define _POSIX_C_SOURCE 200809L
#include "forge.h"
#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define CATALOG_LINE 4096

int forge_load_repos(const ForgeConfig *cfg, ForgeRepo repos[], size_t *count)
{
    *count = 0;
    FILE *f = fopen(cfg->registry_path, "r");
    if (!f) {
        if (errno == ENOENT) return 0;
        perror(cfg->registry_path); return -1;
    }
    char line[FORGE_MAX_URL + FORGE_MAX_NAME + 8];
    while (fgets(line, sizeof(line), f)) {
        char *s = line;
        while (isspace((unsigned char)*s)) s++;
        if (!*s || *s == '#') continue;
        s[strcspn(s, "\r\n")] = '\0';
        char *tab = strchr(s, '\t');
        if (!tab) continue;
        *tab++ = '\0';
        if (!forge_valid_name(s) || !*tab || *count >= FORGE_MAX_REPOS) continue;
        memcpy(repos[*count].name, s, strlen(s) + 1);
        snprintf(repos[*count].url, sizeof(repos[*count].url), "%s", tab);
        (*count)++;
    }
    fclose(f);
    return 0;
}

static int registry_save(const ForgeConfig *cfg, const ForgeRepo repos[], size_t count)
{
    char tmp[PATH_MAX];
    if (snprintf(tmp, sizeof(tmp), "%s.tmpXXXXXX", cfg->registry_path) >= (int)sizeof(tmp)) return -1;
    int fd = mkstemp(tmp);
    if (fd < 0) { perror("mkstemp"); return -1; }
    FILE *f = fdopen(fd, "w");
    if (!f) { close(fd); unlink(tmp); return -1; }
    for (size_t i = 0; i < count; i++) fprintf(f, "%s\t%s\n", repos[i].name, repos[i].url);
    int bad = ferror(f) || fflush(f) != 0 || fsync(fd) != 0;
    if (fclose(f) != 0) bad = 1;
    if (bad || rename(tmp, cfg->registry_path) != 0) { perror("save repository registry"); unlink(tmp); return -1; }
    chmod(cfg->registry_path, 0600);
    return 0;
}

static int repo_index(const ForgeRepo repos[], size_t count, const char *name)
{
    for (size_t i = 0; i < count; i++) if (strcmp(repos[i].name, name) == 0) return (int)i;
    return -1;
}

static int catalog_path(const ForgeConfig *cfg, const char *name, char *out, size_t size)
{
    char dir[PATH_MAX];
    if (forge_path_join(dir, sizeof(dir), cfg->data_dir, "catalog")) return -1;
    if (forge_mkdir_p(dir, 0700)) return -1;
    return forge_path_join(out, size, dir, name);
}

static int url_encode_ref(const char *in, char *out, size_t outsz)
{
    static const char hex[] = "0123456789ABCDEF";
    size_t n = 0;
    for (const unsigned char *p = (const unsigned char *)in; *p; p++) {
        if (isalnum(*p) || *p == '-' || *p == '_' || *p == '.' || *p == '~') {
            if (n + 1 >= outsz) return -1;
            out[n++] = (char)*p;
        } else {
            if (n + 3 >= outsz) return -1;
            out[n++] = '%'; out[n++] = hex[*p >> 4]; out[n++] = hex[*p & 15];
        }
    }
    out[n] = '\0'; return 0;
}

static int json_string_at(const char **p, char *out, size_t outsz)
{
    if (**p != '"') return -1;
    (*p)++;
    size_t n = 0;
    while (**p && **p != '"') {
        unsigned char c = (unsigned char)*(*p)++;
        if (c == '\\') {
            c = (unsigned char)*(*p)++;
            if (!c) return -1;
            if (c == 'u') { for (int i=0;i<4;i++) if (!isxdigit((unsigned char)(*p)[i])) return -1; *p += 4; c='?'; }
            else if (c == 'n') c='\n'; else if (c == 'r') c='\r'; else if (c == 't') c='\t';
        }
        if (n + 1 < outsz) out[n++] = (char)c;
    }
    if (**p != '"') return -1;
    (*p)++; out[n] = '\0'; return 0;
}

static int check_repository_conf(const ForgeRepo *repo, const char *commit)
{
    char owner[128], name[128], url[FORGE_MAX_URL];
    if (forge_github_parts(repo->url, owner, sizeof(owner), name, sizeof(name))) {
        fprintf(stderr, "Forge: '%s' is not a supported GitHub repository URL.\n", repo->url); return -1;
    }
    if (snprintf(url, sizeof(url), "https://raw.githubusercontent.com/%s/%s/%s/repository.conf", owner, name, commit) >= (int)sizeof(url)) return -1;
    ForgeBuffer b;
    if (forge_http_get(url, &b, "text/plain", false) != 0) return -1;
    int ok = strstr(b.data, "[repository]") != NULL;
    char *format = NULL;
char *line = b.data;

while (*line) {
    while (*line == ' ' || *line == '\t' || *line == '\r' || *line == '\n')
        line++;

    if (!*line)
        break;

    char *line_end = strpbrk(line, "\r\n");
    if (line_end)
        *line_end = '\0';

    char *s = line;
    while (isspace((unsigned char)*s))
        s++;

    if (strncmp(s, "format", 6) == 0 &&
        (s[6] == ' ' || s[6] == '\t' || s[6] == '=')) {
        format = strchr(s, '=');
        if (format) {
            format++;
            while (isspace((unsigned char)*format))
                format++;

            char *end = NULL;
            long value = strtol(format, &end, 10);

            while (end && isspace((unsigned char)*end))
                end++;

            if ((value != 1 && value != 2) ||
                !end || (end != format && *end && *end != '#')) {
                ok = 0;
            }
        } else {
            ok = 0;
        }

        break;
    }

    if (!line_end)
        break;

    line = line_end + 1;
}
if (!format)
    ok = 0;
    fprintf(stderr, "DEBUG metadata validation result: %s\n",
        ok ? "accepted" : "rejected");
    if (!ok) fprintf(stderr, "Forge: %s has invalid or unsupported repository.conf metadata.\n", repo->name);
    forge_buffer_free(&b);
    return ok ? 0 : -1;
}

static int sync_one(const ForgeConfig *cfg, const ForgeRepo *repo)
{
    char owner[128], name[128];
    if (forge_github_parts(repo->url, owner, sizeof(owner), name, sizeof(name))) {
        fprintf(stderr, "Forge: only public GitHub HTTPS/SSH repositories are supported currently: %s\n", repo->url);
        return 1;
    }
    char url[FORGE_MAX_URL];
    ForgeBuffer b;
    snprintf(url, sizeof(url), "https://api.github.com/repos/%s/%s", owner, name);
    if (forge_http_get(url, &b, NULL, false) != 0) return 1;
    char branch[256];
    if (forge_json_string(b.data, "default_branch", branch, sizeof(branch)) != 0) {
        fprintf(stderr, "Forge: Could not find default branch for %s/%s.\n", owner, name);
        forge_buffer_free(&b); return 1;
    }
    forge_buffer_free(&b);
    char encoded[768];
    if (url_encode_ref(branch, encoded, sizeof(encoded))) return 1;
    snprintf(url, sizeof(url), "https://api.github.com/repos/%s/%s/commits/%s", owner, name, encoded);
    if (forge_http_get(url, &b, NULL, false) != 0) return 1;
    char commit[128];
    if (forge_json_string(b.data, "sha", commit, sizeof(commit)) != 0) {
        fprintf(stderr, "Forge: Could not resolve commit for %s.\n", repo->name); forge_buffer_free(&b); return 1;
    }
    forge_buffer_free(&b);
    if (snprintf(url, sizeof(url), "https://api.github.com/repos/%s/%s/git/trees/%s?recursive=1", owner, name, commit) >= (int)sizeof(url)) return 1;
    if (forge_http_get(url, &b, NULL, false) != 0) return 1;
    if (strstr(b.data, "\"truncated\": true") || strstr(b.data, "\"truncated\":true")) {
        fprintf(stderr, "Forge: GitHub returned a truncated tree for '%s'; this repository is too large for one index request.\n", repo->name);
        forge_buffer_free(&b); return 1;
    }
    if (check_repository_conf(repo, commit) != 0) { forge_buffer_free(&b); return 1; }
    char catpath[PATH_MAX], tmp[PATH_MAX];
    if (catalog_path(cfg, repo->name, catpath, sizeof(catpath))) { forge_buffer_free(&b); return 1; }
    if (snprintf(tmp, sizeof(tmp), "%s.tmpXXXXXX", catpath) >= (int)sizeof(tmp)) { forge_buffer_free(&b); return 1; }
    int fd = mkstemp(tmp);
    if (fd < 0) { perror("catalog mkstemp"); forge_buffer_free(&b); return 1; }
    FILE *out = fdopen(fd, "w");
    if (!out) { close(fd); unlink(tmp); forge_buffer_free(&b); return 1; }
    fprintf(out, "#commit\t%s\n", commit);
    size_t packages = 0;
    const char *p = b.data;
    while ((p = strstr(p, "\"path\"")) != NULL) {
        p += 6;
        while (*p && isspace((unsigned char)*p)) p++;
        if (*p++ != ':') continue;
        while (*p && isspace((unsigned char)*p)) p++;
        char path[2048];
        if (json_string_at(&p, path, sizeof(path)) != 0) continue;
        const char *prefix = "packages/";
        size_t prelen = strlen(prefix), len = strlen(path);
        if (strncmp(path, prefix, prelen) != 0 || len <= prelen + strlen("/forge.pkg")) continue;
        const char *rest = path + prelen;
        const char *slash = strchr(rest, '/');
        if (!slash) continue;
        char pkgname[FORGE_MAX_NAME]; size_t pn = (size_t)(slash - rest);
        if (pn >= sizeof(pkgname)) continue;
        memcpy(pkgname, rest, pn); pkgname[pn] = '\0';
        if (!forge_valid_name(pkgname)) continue;
        const char *tail = slash + 1;
        char key[FORGE_MAX_NAME + 140];
        if (strcmp(tail, "forge.pkg") == 0) {
            /* Backward-compatible legacy layout: packages/name/forge.pkg */
            snprintf(key, sizeof(key), "%s", pkgname);
        } else {
            const char *vslash = strchr(tail, '/');
            if (!vslash || strcmp(vslash, "/forge.pkg") != 0 || vslash == tail) continue;
            size_t vn = (size_t)(vslash - tail);
            if (vn >= 128) continue;
            char version[128]; memcpy(version, tail, vn); version[vn] = '\0';
            int valid_version = 1;
            for (const unsigned char *vp = (const unsigned char *)version; *vp; vp++)
                if (!isalnum(*vp) && *vp != '.' && *vp != '-' && *vp != '+' && *vp != '_') valid_version = 0;
            if (!valid_version || strstr(version, "..")) continue;
            snprintf(key, sizeof(key), "%s@%s", pkgname, version);
        }
        fprintf(out, "%s\t%s\n", key, path);
        packages++;
    }
    int bad = ferror(out) || fflush(out) != 0 || fsync(fd) != 0;
    if (fclose(out) != 0) bad = 1;
    if (bad || rename(tmp, catpath) != 0) { perror("save catalog"); unlink(tmp); forge_buffer_free(&b); return 1; }
    chmod(catpath, 0600);
    forge_buffer_free(&b);
    printf("Synced %-16s %zu package version(s) at commit %.12s\n", repo->name, packages, commit);
    return 0;
}

static int repo_list(const ForgeRepo repos[], size_t count)
{
    if (!count) { puts("No repositories configured. Run 'forge setup'."); return 0; }
    for (size_t i = 0; i < count; i++) printf("%-16s %s\n", repos[i].name, repos[i].url);
    return 0;
}

static int repo_validate(const ForgeConfig *cfg, const ForgeRepo repos[], size_t count,
                         const char *only, int deep)
{
    int failed = 0; size_t checked = 0;
    for (size_t i = 0; i < count; i++) {
        if (only && strcmp(only, repos[i].name) != 0) continue;
        char path[PATH_MAX];
        if (catalog_path(cfg, repos[i].name, path, sizeof(path))) { failed = 1; continue; }
        FILE *f = fopen(path, "r");
        if (!f) { printf("SKIP    %-16s not synced\n", repos[i].name); continue; }
        char line[CATALOG_LINE]; size_t n = 0;
        char commit[128] = "";
        if (fgets(line, sizeof(line), f)) {
            if (line[0] == '#') (void)sscanf(line, "#commit\t%127s", commit);
            else rewind(f);
        }
        int repo_failed = 0;
        while (fgets(line, sizeof(line), f)) {
            if (line[0] == '#') continue;
            char *tab = strchr(line, '\t'); if (!tab) { failed = 1; continue; }
            *tab++ = '\0'; tab[strcspn(tab, "\r\n")] = '\0';
            char base_name[FORGE_MAX_NAME];
            const char *at = strchr(line, '@');
            size_t base_len = at ? (size_t)(at - line) : strlen(line);
            int valid_key = base_len > 0 && base_len < sizeof(base_name);
            if (valid_key) { memcpy(base_name, line, base_len); base_name[base_len] = '\0'; valid_key = forge_valid_name(base_name); }
            if (at && (!at[1] || strchr(at + 1, '@'))) valid_key = 0;
            if (!valid_key || strncmp(tab, "packages/", 9) != 0 || strstr(tab, "..")) { failed = repo_failed = 1; continue; }
            n++;
            if (deep) {
                char owner[128], repo_name[128], url[FORGE_MAX_URL];
                if (!commit[0] || forge_github_parts(repos[i].url, owner, sizeof(owner), repo_name, sizeof(repo_name))) { failed = repo_failed = 1; continue; }
                int urln = snprintf(url, sizeof(url), "https://raw.githubusercontent.com/%s/%s/%s/%s", owner, repo_name, commit, tab);
                if (urln < 0 || (size_t)urln >= sizeof(url)) { failed = repo_failed = 1; continue; }
                ForgeBuffer b; char err[256] = "manifest parse failed"; ForgePackage pkg;
                if (forge_http_get(url, &b, "text/plain", false) != 0) { failed = repo_failed = 1; continue; }
                int parse_rc = forge_manifest_parse(b.data, &pkg, err, sizeof(err));
                char manifest_key_name[FORGE_MAX_NAME];
                const char *key_at = strchr(line, '@'); size_t key_len = key_at ? (size_t)(key_at - line) : strlen(line);
                if (key_len >= sizeof(manifest_key_name)) key_len = 0;
                memcpy(manifest_key_name, line, key_len); manifest_key_name[key_len] = '\0';
                if (parse_rc != 0 || strcmp(pkg.name, manifest_key_name) != 0) {
                    fprintf(stderr, "INVALID %s/%s: %s\n", repos[i].name, line, parse_rc ? err : "manifest package name mismatch"); failed = repo_failed = 1;
                }
                forge_buffer_free(&b);
            }
        }
        fclose(f); checked++;
        printf("%s    %-16s %zu catalog entr%s\n", repo_failed ? "CHECK" : "VALID", repos[i].name, n, n == 1 ? "y" : "ies");
    }
    if (!checked) { puts("No synced repositories found. Run 'forge repo sync' first."); return 1; }
    return failed ? 1 : 0;
}

int forge_repo_command(const ForgeConfig *cfg, int argc, char **argv)
{
    ForgeRepo repos[FORGE_MAX_REPOS]; size_t count = 0;
    if (forge_load_repos(cfg, repos, &count) != 0) return 1;
    if (argc == 3 && strcmp(argv[2], "list") == 0) return repo_list(repos, count);
    if (argc == 5 && strcmp(argv[2], "add") == 0) {
        if (!forge_valid_name(argv[3])) { fprintf(stderr, "Forge: Invalid repository name. Use letters, digits, '-' and '_'.\n"); return 1; }
        char owner[128], name[128];
        if (forge_github_parts(argv[4], owner, sizeof(owner), name, sizeof(name))) {
            fprintf(stderr, "Forge: this build supports public GitHub URLs (https://github.com/OWNER/REPO).\n"); return 1;
        }
        if (repo_index(repos, count, argv[3]) >= 0) { fprintf(stderr, "Forge: Repository '%s' already exists.\n", argv[3]); return 1; }
        if (count >= FORGE_MAX_REPOS) { fprintf(stderr, "Forge: repository limit reached.\n"); return 1; }
        snprintf(repos[count].name, sizeof(repos[count].name), "%s", argv[3]);
        snprintf(repos[count].url, sizeof(repos[count].url), "https://github.com/%s/%s", owner, name);
        if (registry_save(cfg, repos, count + 1)) return 1;
        printf("Added repository '%s'. Run 'forge repo sync %s'.\n", argv[3], argv[3]); return 0;
    }
    if (argc == 4 && strcmp(argv[2], "remove") == 0) {
        int idx = repo_index(repos, count, argv[3]);
        if (idx < 0) { fprintf(stderr, "Forge: Repository '%s' not found.\n", argv[3]); return 1; }
        for (size_t i = (size_t)idx; i + 1 < count; i++) repos[i] = repos[i + 1];
        count--;
        if (registry_save(cfg, repos, count)) return 1;
        printf("Removed '%s' from repository registry; its cached catalog is preserved.\n", argv[3]); return 0;
    }
    if (argc == 3 && strcmp(argv[2], "sync") == 0) {
        if (!count) { puts("No repositories configured."); return 0; }
        int failed = 0; for (size_t i = 0; i < count; i++) if (sync_one(cfg, &repos[i])) failed = 1;
        if (failed) { fprintf(stderr, "Forge: one or more repositories failed to sync.\n"); return 1; }
        puts("Forge: all repository catalogs are up to date."); return 0;
    }
    if (argc == 4 && strcmp(argv[2], "sync") == 0) {
        int idx = repo_index(repos, count, argv[3]);
        if (idx < 0) { fprintf(stderr, "Forge: Repository '%s' not found.\n", argv[3]); return 1; }
        return sync_one(cfg, &repos[idx]);
    }
    if ((argc == 3 || argc == 4 || argc == 5) && strcmp(argv[2], "validate") == 0) {
        const char *only = NULL; int deep = 0;
        for (int i = 3; i < argc; i++) { if (!strcmp(argv[i], "--deep")) deep = 1; else only = argv[i]; }
        return repo_validate(cfg, repos, count, only, deep);
    }
    fprintf(stderr, "Usage: forge repo {list|add <name> <github-url>|remove <name>|sync [name]|validate [name] [--deep]}\n");
    return 1;
}
