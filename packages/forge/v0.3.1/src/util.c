#define _POSIX_C_SOURCE 200809L
#include "forge.h"
#include <curl/curl.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <libgen.h>

#define DEFAULT_OFFICIAL_URL "https://github.com/ItzSevrus/forgeRepo"
#define HTTP_LIMIT (32U * 1024U * 1024U)

static int append_buffer(ForgeBuffer *b, const char *data, size_t n)
{
    if (n > HTTP_LIMIT || b->len > HTTP_LIMIT - n) return 0;
    if (b->len + n + 1 > b->cap) {
        size_t cap = b->cap ? b->cap : 8192;
        while (cap < b->len + n + 1) cap *= 2;
        char *p = realloc(b->data, cap);
        if (!p) return 0;
        b->data = p;
        b->cap = cap;
    }
    memcpy(b->data + b->len, data, n);
    b->len += n;
    b->data[b->len] = '\0';
    return 1;
}

static size_t curl_write(char *ptr, size_t size, size_t nmemb, void *userdata)
{
    size_t n = size * nmemb;
    return append_buffer((ForgeBuffer *)userdata, ptr, n) ? n : 0;
}

int forge_http_get(const char *url, ForgeBuffer *out, const char *accept)
{
    memset(out, 0, sizeof(*out));
    static int curl_ready = 0;
    if (!curl_ready) {
        if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK) return -1;
        curl_ready = 1;
    }
    CURL *curl = curl_easy_init();
    if (!curl) return -1;
    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "User-Agent: Forge-Package-Manager/0.3.0");
    headers = curl_slist_append(headers, "Accept: application/vnd.github+json");
    if (accept && *accept) {
        char h[256];
        snprintf(h, sizeof(h), "Accept: %s", accept);
        headers = curl_slist_append(headers, h);
    }
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 5L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 12L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 75L);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, out);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
    CURLcode rc = curl_easy_perform(curl);
    if (rc == CURLE_OK) curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &out->status);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    if (rc != CURLE_OK || out->status < 200 || out->status >= 300) {
        fprintf(stderr, "Forge: HTTP request failed for %s (HTTP %ld%s%s).\n",
                url, out->status, rc == CURLE_OK ? "" : ", ",
                rc == CURLE_OK ? "" : curl_easy_strerror(rc));
        forge_buffer_free(out);
        return -1;
    }
    if (!out->data) {
        out->data = calloc(1, 1);
        if (!out->data) return -1;
    }
    return 0;
}

void forge_buffer_free(ForgeBuffer *b)
{
    if (!b) return;
    free(b->data);
    memset(b, 0, sizeof(*b));
}

int forge_mkdir_p(const char *path, mode_t mode)
{
    if (!path || !*path) return -1;
    char tmp[PATH_MAX];
    if (snprintf(tmp, sizeof(tmp), "%s", path) >= (int)sizeof(tmp)) return -1;
    size_t n = strlen(tmp);
    while (n > 1 && tmp[n - 1] == '/') tmp[--n] = '\0';
    for (char *p = tmp + (tmp[0] == '/' ? 1 : 0); ; p++) {
        if (*p == '/' || *p == '\0') {
            char saved = *p;
            *p = '\0';
            if (*tmp && mkdir(tmp, mode) != 0 && errno != EEXIST) {
                perror(tmp); return -1;
            }
            *p = saved;
            if (!saved) break;
        }
    }
    struct stat st;
    if (stat(tmp, &st) != 0 || !S_ISDIR(st.st_mode)) {
        fprintf(stderr, "Forge: '%s' is not a directory.\n", tmp);
        return -1;
    }
    return 0;
}

int forge_path_join(char *out, size_t size, const char *a, const char *b)
{
    size_t alen = strlen(a), blen = strlen(b);
    if (alen + 1 + blen + 1 > size) return -1;
    memcpy(out, a, alen);
    out[alen] = '/';
    memcpy(out + alen + 1, b, blen + 1);
    return 0;
}

int forge_write_file(const char *path, const char *data, size_t len, mode_t mode)
{
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, mode);
    if (fd < 0) { perror(path); return -1; }
    size_t off = 0;
    while (off < len) {
        ssize_t n = write(fd, data + off, len - off);
        if (n < 0) { if (errno == EINTR) continue; perror(path); close(fd); return -1; }
        off += (size_t)n;
    }
    if (fchmod(fd, mode) != 0) { perror("fchmod"); close(fd); return -1; }
    return close(fd);
}

char *forge_read_file(const char *path, size_t *len_out)
{
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return NULL; }
    long size = ftell(f);
    if (size < 0 || size > (long)HTTP_LIMIT || fseek(f, 0, SEEK_SET) != 0) { fclose(f); return NULL; }
    char *data = malloc((size_t)size + 1);
    if (!data) { fclose(f); return NULL; }
    size_t n = fread(data, 1, (size_t)size, f);
    fclose(f);
    data[n] = '\0';
    if (len_out) *len_out = n;
    return data;
}

int forge_valid_name(const char *name)
{
    if (!name || !*name || strlen(name) >= FORGE_MAX_NAME || name[0] == '.') return 0;
    if (strstr(name, "..")) return 0;
    for (const unsigned char *p = (const unsigned char *)name; *p; p++)
        if (!isalnum(*p) && *p != '-' && *p != '_' && *p != '.' && *p != '+') return 0;
    return 1;
}

/* Decode a JSON string at *p. Advances p beyond the closing quote. */
static int json_decode(const char **p, char *out, size_t outsz)
{
    size_t n = 0;
    if (**p != '"') return -1;
    (*p)++;
    while (**p && **p != '"') {
        unsigned char c = (unsigned char)*(*p)++;
        if (c == '\\') {
            c = (unsigned char)*(*p)++;
            if (!c) return -1;
            switch (c) {
                case '"': case '\\': case '/': break;
                case 'b': c = '\b'; break;
                case 'f': c = '\f'; break;
                case 'n': c = '\n'; break;
                case 'r': c = '\r'; break;
                case 't': c = '\t'; break;
                case 'u':
                    /* Repository paths and branch names should be ASCII. */
                    for (int i = 0; i < 4; i++) if (!isxdigit((unsigned char)(*p)[i])) return -1;
                    *p += 4; c = '?'; break;
                default: return -1;
            }
        }
        if (n + 1 < outsz) out[n++] = (char)c;
    }
    if (**p != '"') return -1;
    (*p)++;
    if (outsz) out[n] = '\0';
    return 0;
}

int forge_json_string(const char *json, const char *key, char *out, size_t outsz)
{
    char needle[256];
    if (snprintf(needle, sizeof(needle), "\"%s\"", key) >= (int)sizeof(needle)) return -1;
    const char *p = json;
    while ((p = strstr(p, needle)) != NULL) {
        p += strlen(needle);
        while (*p && isspace((unsigned char)*p)) p++;
        if (*p++ != ':') continue;
        while (*p && isspace((unsigned char)*p)) p++;
        if (*p != '"') continue;
        return json_decode(&p, out, outsz);
    }
    return -1;
}

int forge_github_parts(const char *url, char *owner, size_t owner_sz,
                       char *repo, size_t repo_sz)
{
    const char *p = NULL;
    if (strncmp(url, "https://github.com/", 19) == 0) p = url + 19;
    else if (strncmp(url, "http://github.com/", 18) == 0) p = url + 18;
    else if (strncmp(url, "git@github.com:", 15) == 0) p = url + 15;
    else return -1;
    while (*p == '/') p++;
    const char *slash = strchr(p, '/');
    if (!slash || slash == p) return -1;
    size_t on = (size_t)(slash - p);
    if (on >= owner_sz) return -1;
    memcpy(owner, p, on); owner[on] = '\0';
    p = slash + 1;
    const char *end = p;
    while (*end && *end != '/' && *end != '?' && *end != '#') end++;
    size_t rn = (size_t)(end - p);
    if (rn >= 4 && strncmp(p + rn - 4, ".git", 4) == 0) rn -= 4;
    if (!rn || rn >= repo_sz) return -1;
    memcpy(repo, p, rn); repo[rn] = '\0';
    for (const unsigned char *q = (const unsigned char *)owner; *q; q++)
        if (!isalnum(*q) && *q != '-' && *q != '_') return -1;
    for (const unsigned char *q = (const unsigned char *)repo; *q; q++)
        if (!isalnum(*q) && *q != '-' && *q != '_' && *q != '.') return -1;
    return 0;
}

static void trim_trailing_slashes(char *path)
{
    size_t n = strlen(path);
    while (n > 1 && path[n - 1] == '/') path[--n] = '\0';
}

static int load_settings(ForgeConfig *cfg)
{
    FILE *f = fopen(cfg->settings_path, "r");
    if (!f) return 0;
    char line[PATH_MAX + 64];
    while (fgets(line, sizeof(line), f)) {
        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq++ = '\0';
        line[strcspn(line, "\r\n")] = '\0';
        eq[strcspn(eq, "\r\n")] = '\0';
        if (strcmp(line, "prefix") == 0 && eq[0] == '/')
            snprintf(cfg->prefix, sizeof(cfg->prefix), "%s", eq);
    }
    fclose(f);
    return 0;
}

int forge_init(ForgeConfig *cfg)
{
    memset(cfg, 0, sizeof(*cfg));
    const char *home = getenv("HOME");
    if (!home || home[0] != '/') { fprintf(stderr, "Forge: HOME must be an absolute path.\n"); return -1; }
    const char *xdg_config = getenv("XDG_CONFIG_HOME");
    const char *xdg_data = getenv("XDG_DATA_HOME");
    const char *xdg_cache = getenv("XDG_CACHE_HOME");
    if (xdg_config && xdg_config[0] == '/') snprintf(cfg->config_dir, sizeof(cfg->config_dir), "%s/forge", xdg_config);
    else snprintf(cfg->config_dir, sizeof(cfg->config_dir), "%s/.config/forge", home);
    trim_trailing_slashes(cfg->config_dir);
    if (xdg_data && xdg_data[0] == '/') snprintf(cfg->data_dir, sizeof(cfg->data_dir), "%s/forge", xdg_data);
    else snprintf(cfg->data_dir, sizeof(cfg->data_dir), "%s/.local/share/forge", home);
    trim_trailing_slashes(cfg->data_dir);
    if (xdg_cache && xdg_cache[0] == '/') snprintf(cfg->cache_dir, sizeof(cfg->cache_dir), "%s/forge", xdg_cache);
    else snprintf(cfg->cache_dir, sizeof(cfg->cache_dir), "%s/.cache/forge", home);
    trim_trailing_slashes(cfg->cache_dir);
    snprintf(cfg->prefix, sizeof(cfg->prefix), "%s/.local", home);
    forge_path_join(cfg->registry_path, sizeof(cfg->registry_path), cfg->config_dir, "repos.conf");
    forge_path_join(cfg->settings_path, sizeof(cfg->settings_path), cfg->config_dir, "settings.conf");
    if (forge_mkdir_p(cfg->config_dir, 0700) || forge_mkdir_p(cfg->data_dir, 0700) ||
        forge_mkdir_p(cfg->cache_dir, 0700)) return -1;
    load_settings(cfg);
    return 0;
}

int forge_setup(const ForgeConfig *cfg)
{
    if (access(cfg->registry_path, F_OK) != 0) {
        char data[FORGE_MAX_URL + FORGE_MAX_NAME + 8];
        int n = snprintf(data, sizeof(data), "official\t%s\n", DEFAULT_OFFICIAL_URL);
        if (n < 0 || (size_t)n >= sizeof(data) || forge_write_file(cfg->registry_path, data, (size_t)n, 0600)) return 1;
        puts("Added default repository: official");
    }
    if (access(cfg->settings_path, F_OK) != 0) {
        char data[PATH_MAX + 32];
        int n = snprintf(data, sizeof(data), "prefix=%s\n", cfg->prefix);
        if (n < 0 || (size_t)n >= sizeof(data) || forge_write_file(cfg->settings_path, data, (size_t)n, 0600)) return 1;
    }
    char p[PATH_MAX];
    const char *dirs[] = {"catalog", "installed", "builds", "downloads"};
    for (size_t i = 0; i < sizeof(dirs)/sizeof(dirs[0]); i++) {
        if (forge_path_join(p, sizeof(p), cfg->data_dir, dirs[i]) || forge_mkdir_p(p, 0700)) return 1;
    }
    printf("Forge %s is ready. Prefix: %s\n", FORGE_VERSION, cfg->prefix);
    return 0;
}

static int config_write(const ForgeConfig *cfg, const char *prefix)
{
    char data[PATH_MAX + 32];
    int n = snprintf(data, sizeof(data), "prefix=%s\n", prefix);
    if (n < 0 || (size_t)n >= sizeof(data)) return 1;
    return forge_write_file(cfg->settings_path, data, (size_t)n, 0600) == 0 ? 0 : 1;
}

int forge_config_command(const ForgeConfig *cfg, int argc, char **argv)
{
    if (argc == 2) {
        printf("config_dir = %s\ndata_dir   = %s\ncache_dir  = %s\nprefix     = %s\n",
               cfg->config_dir, cfg->data_dir, cfg->cache_dir, cfg->prefix);
        return 0;
    }
    if (argc == 4 && strcmp(argv[2], "get") == 0) {
        if (strcmp(argv[3], "prefix") == 0) { puts(cfg->prefix); return 0; }
        fprintf(stderr, "Forge: Unknown setting '%s'. Available: prefix\n", argv[3]);
        return 1;
    }
    if (argc == 5 && strcmp(argv[2], "set") == 0) {
        if (strcmp(argv[3], "prefix") != 0) {
            fprintf(stderr, "Forge: Unknown setting '%s'. Available: prefix\n", argv[3]); return 1;
        }
        if (argv[4][0] != '/' || strlen(argv[4]) >= PATH_MAX) {
            fprintf(stderr, "Forge: prefix must be an absolute path.\n"); return 1;
        }
        if (forge_mkdir_p(argv[4], 0755) != 0) return 1;
        int rc = config_write(cfg, argv[4]);
        if (!rc) printf("Forge: prefix set to %s\n", argv[4]);
        return rc;
    }
    fprintf(stderr, "Usage: forge config [get prefix | set prefix /absolute/path]\n");
    return 1;
}
