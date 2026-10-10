#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200809L
#include "forge.h"
#include <curl/curl.h>
#include <openssl/evp.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

typedef struct { const char *label; curl_off_t last_print; } DownloadProgress;

static void human_size(curl_off_t bytes, char out[64])
{
    const char *units[] = {"B", "KiB", "MiB", "GiB"};
    double value = (double)bytes; size_t unit = 0;
    while (value >= 1024.0 && unit < 3) { value /= 1024.0; unit++; }
    snprintf(out, 64, unit ? "%.1f %s" : "%.0f %s", value, units[unit]);
}

static int download_progress(void *userdata, curl_off_t total, curl_off_t now,
                             curl_off_t upload_total, curl_off_t upload_now)
{
    (void)upload_total; (void)upload_now;
    DownloadProgress *p = userdata;
    if (now == 0 || (now - p->last_print < 65536 && now != total)) return 0;
    p->last_print = now;
    char got[64], all[64]; human_size(now, got);
    if (total > 0) { human_size(total, all); fprintf(stderr, "\r  Downloading %-22s %s / %s", p->label, got, all); }
    else fprintf(stderr, "\r  Downloading %-22s %s", p->label, got);
    fflush(stderr);
    return 0;
}

static int download_file(const char *url, const char *path, const char *label)
{
    static int ready = 0;
    if (!ready) { if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK) return -1; ready = 1; }
    FILE *f = fopen(path, "wb");
    if (!f) { perror(path); return -1; }
    CURL *c = curl_easy_init();
    if (!c) { fclose(f); return -1; }
    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "User-Agent: Forge-Package-Manager/0.3.1");
    DownloadProgress progress = {label, 0};
    fprintf(stderr, "\nDownloading %s\n  From: %s\n  To:   %s\n", label, url, path);
    curl_easy_setopt(c, CURLOPT_URL, url);
    curl_easy_setopt(c, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(c, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(c, CURLOPT_MAXREDIRS, 5L);
    curl_easy_setopt(c, CURLOPT_CONNECTTIMEOUT, 15L);
    curl_easy_setopt(c, CURLOPT_TIMEOUT, 300L);
    curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, NULL);
    curl_easy_setopt(c, CURLOPT_WRITEDATA, f);
    curl_easy_setopt(c, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(c, CURLOPT_SSL_VERIFYHOST, 2L);
    curl_easy_setopt(c, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(c, CURLOPT_XFERINFOFUNCTION, download_progress);
    curl_easy_setopt(c, CURLOPT_XFERINFODATA, &progress);
    CURLcode rc = curl_easy_perform(c); long status = 0;
    if (progress.last_print > 0) fputc('\n', stderr);
    if (rc == CURLE_OK) curl_easy_getinfo(c, CURLINFO_RESPONSE_CODE, &status);
    if (progress.last_print == 0) {
        curl_off_t total = 0; curl_easy_getinfo(c, CURLINFO_CONTENT_LENGTH_DOWNLOAD_T, &total);
        if (total > 0) { char all[64]; human_size(total, all); fprintf(stderr, "  Expected download size: %s\n", all); }
    }
    curl_slist_free_all(headers); curl_easy_cleanup(c);
    int close_rc = fclose(f);
    if (rc != CURLE_OK || status < 200 || status >= 300 || close_rc != 0) {
        fprintf(stderr, "Forge: download failed for %s (HTTP %ld, %s).\n", url, status, curl_easy_strerror(rc));
        unlink(path); return -1;
    }
    struct stat st;
    if (stat(path, &st) == 0) { char size[64]; human_size((curl_off_t)st.st_size, size); fprintf(stderr, "  Download complete: %s\n", size); }
    return 0;
}

int forge_sha256_file(const char *path, char out[65])
{
    FILE *f = fopen(path, "rb"); if (!f) return -1;
    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    if (!ctx) { fclose(f); return -1; }
    int ok = EVP_DigestInit_ex(ctx, EVP_sha256(), NULL) == 1;
    unsigned char buf[65536]; size_t n;
    while (ok && (n = fread(buf, 1, sizeof(buf), f)) > 0)
        ok = EVP_DigestUpdate(ctx, buf, n) == 1;
    if (ferror(f)) ok = 0;
    fclose(f);
    unsigned char digest[EVP_MAX_MD_SIZE]; unsigned int digest_len = 0;
    if (ok) ok = EVP_DigestFinal_ex(ctx, digest, &digest_len) == 1 && digest_len == 32;
    EVP_MD_CTX_free(ctx);
    if (!ok) return -1;
    for (size_t i = 0; i < 32; i++) sprintf(out + i * 2, "%02x", digest[i]);
    out[64] = '\0'; return 0;
}

static int checksum_ok(const char *expected, const char *file)
{
    char normalized[128];
    const char *p = expected;
    if (strncmp(p, "sha256:", 7) == 0) p += 7;
    if (strlen(p) != 64) return 0;
    for (size_t i = 0; i < 64; i++) { if (!isxdigit((unsigned char)p[i])) return 0; normalized[i] = (char)tolower((unsigned char)p[i]); }
    normalized[64] = '\0';
    char actual[65]; if (forge_sha256_file(file, actual)) return 0;
    if (strcmp(normalized, actual) != 0) {
        fprintf(stderr, "Forge: SHA-256 mismatch. Expected %s, got %s.\n", normalized, actual); return 0;
    }
    return 1;
}

static int run_argv(char *const argv[])
{
    pid_t pid = fork();
    if (pid < 0) { perror("fork"); return -1; }
    if (pid == 0) { execvp(argv[0], argv); perror(argv[0]); _exit(127); }
    int status; while (waitpid(pid, &status, 0) < 0) if (errno != EINTR) { perror("waitpid"); return -1; }
    return WIFEXITED(status) ? WEXITSTATUS(status) : 128;
}

int forge_run_build(const char *command, const char *workdir, const char *stage,
                    const char *prefix, const char *source_dir)
{
    pid_t pid = fork();
    if (pid < 0) { perror("fork"); return -1; }
    if (pid == 0) {
        if (chdir(source_dir) != 0) { perror("chdir source"); _exit(126); }
        setenv("FORGE_SOURCE_DIR", source_dir, 1);
        setenv("FORGE_WORK_DIR", workdir, 1);
        setenv("FORGE_STAGE_DIR", stage, 1);
        setenv("DESTDIR", stage, 1);
        setenv("FORGE_PREFIX", prefix, 1);
        setenv("PREFIX", prefix, 1);
        setenv("FORGE_NETWORKED_BUILD", "1", 1);
        execl("/bin/sh", "sh", "-c", command, (char *)NULL);
        perror("/bin/sh"); _exit(127);
    }
    int status; while (waitpid(pid, &status, 0) < 0) if (errno != EINTR) { perror("waitpid"); return -1; }
    return WIFEXITED(status) ? WEXITSTATUS(status) : 128;
}

static int path_is_safe_relative(const char *s)
{
    if (!s || !*s || *s == '/' || strstr(s, "..")) return 0;
    for (const unsigned char *p = (const unsigned char *)s; *p; p++)
        if (!(isalnum(*p) || *p == '/' || *p == '-' || *p == '_' || *p == '.' || *p == '+')) return 0;
    return 1;
}

static int copy_tree_inner(const char *src, const char *dst, const char *rel,
                           FILE *manifest, int reject_existing)
{
    DIR *d = opendir(src); if (!d) { perror(src); return -1; }
    struct dirent *e; int rc = 0;
    while ((e = readdir(d)) != NULL) {
        if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue;
        if (!path_is_safe_relative(e->d_name) || strchr(e->d_name, '/')) { rc = -1; break; }
        char s[PATH_MAX], t[PATH_MAX], childrel[PATH_MAX]; struct stat st;
        if (snprintf(s, sizeof(s), "%s/%s", src, e->d_name) >= (int)sizeof(s) ||
            snprintf(t, sizeof(t), "%s/%s", dst, e->d_name) >= (int)sizeof(t) ||
            snprintf(childrel, sizeof(childrel), "%s%s%s", rel, *rel ? "/" : "", e->d_name) >= (int)sizeof(childrel)) { rc = -1; break; }
        if (lstat(s, &st) != 0) { rc = -1; break; }
        if (S_ISDIR(st.st_mode)) {
            if (mkdir(t, 0755) != 0 && errno != EEXIST) { perror(t); rc = -1; break; }
            if (copy_tree_inner(s, t, childrel, manifest, reject_existing) != 0) { rc = -1; break; }
        } else if (S_ISREG(st.st_mode)) {
            if (reject_existing && access(t, F_OK) == 0) {
                fprintf(stderr, "Forge: refusing to overwrite existing file: %s\n", t); rc = -1; break;
            }
            int in = open(s, O_RDONLY | O_CLOEXEC);
            int out = open(t, O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, st.st_mode & 0777);
            if (in < 0 || out < 0) {
                if (in >= 0) close(in);
                if (out >= 0) close(out);
                perror(t); rc = -1; break;
            }
            char buf[65536]; ssize_t n; int bad = 0;
            while ((n = read(in, buf, sizeof(buf))) > 0) {
                ssize_t off = 0;
                while (off < n) { ssize_t w = write(out, buf + off, (size_t)(n - off)); if (w < 0) { if (errno == EINTR) continue; bad = 1; break; } off += w; }
                if (bad) break;
            }
            if (n < 0) bad = 1;
            close(in); if (close(out) != 0) bad = 1;
            if (bad) { unlink(t); rc = -1; break; }
            if (manifest) fprintf(manifest, "%s\n", childrel);
        } else {
            fprintf(stderr, "Forge: staged symlinks/devices are not supported: %s\n", s); rc = -1; break;
        }
    }
    closedir(d); return rc;
}

int forge_copy_tree(const char *src, const char *dst, const char *prefix,
                    FILE *manifest, int reject_existing)
{
    (void)prefix;
    struct stat st;
    if (stat(src, &st) != 0 || !S_ISDIR(st.st_mode)) return -1;
    if (forge_mkdir_p(dst, 0755) != 0) return -1;
    return copy_tree_inner(src, dst, "", manifest, reject_existing);
}

static int remove_empty_dirs(const char *prefix, const char *relative)
{
    char path[PATH_MAX];
    size_t plen = strlen(prefix), rlen = strlen(relative);
    if (plen + 1 + rlen + 1 > sizeof(path)) return -1;
    memcpy(path, prefix, plen); path[plen] = '/';
    memcpy(path + plen + 1, relative, rlen + 1);
    char *slash = strrchr(path, '/');
    while (slash) {
        *slash = '\0';
        if (strncmp(path, prefix, strlen(prefix)) != 0 || strlen(path) <= strlen(prefix)) break;
        if (rmdir(path) != 0) break;
        slash = strrchr(path, '/');
    }
    return 0;
}

int forge_remove_tree_files(const char *prefix, const char *manifest_path)
{
    FILE *f = fopen(manifest_path, "r"); if (!f) { perror(manifest_path); return -1; }
    char line[PATH_MAX]; int rc = 0;
    while (fgets(line, sizeof(line), f)) {
        if (line[0] == '#') continue;
        line[strcspn(line, "\r\n")] = '\0';
        if (!path_is_safe_relative(line)) { fprintf(stderr, "Forge: unsafe path in installed manifest; skipping.\n"); rc = -1; continue; }
        char path[PATH_MAX];
        if (snprintf(path, sizeof(path), "%s/%s", prefix, line) >= (int)sizeof(path)) { rc = -1; continue; }
        if (unlink(path) != 0 && errno != ENOENT) { perror(path); rc = -1; }
        remove_empty_dirs(prefix, line);
    }
    fclose(f); return rc;
}

static int installed_marker(const ForgeConfig *cfg, const char *name, char *out, size_t outsz)
{
    char dir[PATH_MAX];
    if (forge_path_join(dir, sizeof(dir), cfg->data_dir, "installed")) return -1;
    if (forge_mkdir_p(dir, 0700)) return -1;
    return forge_path_join(out, outsz, dir, name);
}

static int ask_install(const ForgePackage *pkg, int assume_yes, const char *message)
{
    printf("\nPackage: %s %s\n%s\n", pkg->name, pkg->version, pkg->description);
    if (message && *message) { puts("\n----- Package MESSAGE.md -----"); fputs(message, stdout); if (message[strlen(message)-1] != '\n') putchar('\n'); puts("----- End MESSAGE.md -----"); }
    puts("WARNING: Forge will execute this repository's build script as your user.");
    puts("Review the repository and build script before installing untrusted packages.");
    if (assume_yes) return 1;
    printf("Continue? [y/N] "); fflush(stdout);
    char answer[32];
    if (!fgets(answer, sizeof(answer), stdin)) return 0;
    return answer[0] == 'y' || answer[0] == 'Y';
}

static int extract_archive(const char *archive, const char *source)
{
    if (forge_mkdir_p(source, 0700)) return -1;
    char *args[] = {"tar", "--extract", "--file", (char *)archive, "--directory", (char *)source,
                    "--no-same-owner", "--no-same-permissions", NULL};
    return run_argv(args);
}

static int command_exists(const char *name)
{
    char *const args[] = {"sh", "-c", "command -v \"$1\" >/dev/null 2>&1", "forge-check", (char *)name, NULL};
    return run_argv(args) == 0;
}

static int package_installed(const char *manager, const char *name)
{
    char command[512];
    if (!strcmp(manager, "apt")) snprintf(command, sizeof(command), "dpkg-query -W -f='${Status}' -- '%s' 2>/dev/null | grep -q 'install ok installed'", name);
    else if (!strcmp(manager, "pacman")) snprintf(command, sizeof(command), "pacman -Q -- '%s' >/dev/null 2>&1", name);
    else snprintf(command, sizeof(command), "rpm -q -- '%s' >/dev/null 2>&1", name);
    char *args[] = {"sh", "-c", command, NULL};
    return run_argv(args) == 0;
}

static int install_system_dependencies(const ForgePackage *pkg)
{
    const char *manager = NULL;
    char (*deps)[FORGE_MAX_NAME] = NULL; size_t count = 0;
    if (command_exists("apt-get") && command_exists("dpkg-query")) { manager = "apt"; deps = (char (*)[FORGE_MAX_NAME])pkg->apt_deps; count = pkg->apt_dep_count; }
    else if (command_exists("pacman")) { manager = "pacman"; deps = (char (*)[FORGE_MAX_NAME])pkg->pacman_deps; count = pkg->pacman_dep_count; }
    else if (command_exists("dnf") && command_exists("rpm")) { manager = "dnf"; deps = (char (*)[FORGE_MAX_NAME])pkg->dnf_deps; count = pkg->dnf_dep_count; }
    else if (command_exists("zypper") && command_exists("rpm")) { manager = "zypper"; deps = (char (*)[FORGE_MAX_NAME])pkg->zypper_deps; count = pkg->zypper_dep_count; }
    else {
        if (pkg->apt_dep_count || pkg->pacman_dep_count || pkg->dnf_dep_count || pkg->zypper_dep_count) {
            fprintf(stderr, "Forge: no supported system package manager detected (apt, pacman, dnf, zypper).\n"); return -1;
        }
        return 0;
    }
    printf("\nSystem package manager: %s\n", manager);
    if (!count) { puts("No system packages declared for this distribution."); return 0; }
    char missing[FORGE_MAX_DEPS][FORGE_MAX_NAME]; size_t nmissing = 0;
    for (size_t i = 0; i < count; i++) {
        if (package_installed(manager, deps[i])) printf("  [ready] %s\n", deps[i]);
        else { printf("  [needed] %s\n", deps[i]); snprintf(missing[nmissing++], FORGE_MAX_NAME, "%s", deps[i]); }
    }
    if (!nmissing) { puts("All declared system dependencies are already installed."); return 0; }
    puts("Forge needs to install the missing system packages. This requires administrator authentication.");
    printf("Run: sudo %s", manager);
    if (!strcmp(manager, "apt")) printf(" install -y");
    else if (!strcmp(manager, "pacman")) printf(" -S --needed");
    else if (!strcmp(manager, "dnf")) printf(" install -y");
    else printf(" --non-interactive install -y");
    for (size_t i = 0; i < nmissing; i++) printf(" %s", missing[i]);
    puts("");
    printf("Continue with system dependency installation? [y/N] "); fflush(stdout);
    char answer[32]; if (!fgets(answer, sizeof(answer), stdin) || (answer[0] != 'y' && answer[0] != 'Y')) { puts("Dependency installation cancelled."); return -1; }
    char *args[FORGE_MAX_DEPS + 8]; size_t a = 0;
    args[a++] = "sudo";
    if (!strcmp(manager, "apt")) { args[a++] = "apt-get"; args[a++] = "install"; args[a++] = "-y"; }
    else if (!strcmp(manager, "pacman")) { args[a++] = "pacman"; args[a++] = "-S"; args[a++] = "--needed"; args[a++] = "--noconfirm"; }
    else if (!strcmp(manager, "dnf")) { args[a++] = "dnf"; args[a++] = "install"; args[a++] = "-y"; }
    else { args[a++] = "zypper"; args[a++] = "--non-interactive"; args[a++] = "install"; args[a++] = "-y"; }
    for (size_t i = 0; i < nmissing; i++) args[a++] = missing[i];
    args[a] = NULL;
    if (run_argv(args) != 0) { fprintf(stderr, "Forge: system dependency installation failed.\n"); return -1; }
    puts("System dependencies installed successfully."); return 0;
}

static int check_compilers(const ForgePackage *pkg)
{
    if (!pkg->compiler_count) { puts("Compiler check: no specific compiler requirements declared."); return 0; }
    puts("Checking required build tools:");
    for (size_t i = 0; i < pkg->compiler_count; i++) {
        if (command_exists(pkg->compilers[i])) printf("  [ready] %s\n", pkg->compilers[i]);
        else { fprintf(stderr, "  [missing] %s\nForge: required build tool '%s' is unavailable. Add its system package to [dependencies.system] for each supported distribution.\n", pkg->compilers[i], pkg->compilers[i]); return -1; }
    }
    return 0;
}

static int install_recursive(const ForgeConfig *cfg, const char *name, int yes,
                             char stack[][FORGE_MAX_NAME], size_t depth)
{
    if (depth >= FORGE_MAX_DEPTH) { fprintf(stderr, "Forge: dependency depth limit reached.\n"); return 1; }
    const char *at = strchr(name, '@');
    char base_name[FORGE_MAX_NAME]; size_t base_len = at ? (size_t)(at - name) : strlen(name);
    if (!base_len || base_len >= sizeof(base_name)) { fprintf(stderr, "Forge: invalid package specifier.\n"); return 1; }
    memcpy(base_name, name, base_len); base_name[base_len] = '\0';
    for (size_t i = 0; i < depth; i++) if (!strcmp(stack[i], base_name)) {
        fprintf(stderr, "Forge: dependency cycle detected at '%s'.\n", base_name); return 1;
    }
    snprintf(stack[depth], FORGE_MAX_NAME, "%s", base_name);
    char marker[PATH_MAX];
    if (installed_marker(cfg, base_name, marker, sizeof(marker))) return 1;
    char repo_name[FORGE_MAX_NAME], repo_url[FORGE_MAX_URL], manifest_path[2048], commit[128];
    ForgePackage pkg;
    if (forge_resolve_package(cfg, name, repo_name, sizeof(repo_name), repo_url, sizeof(repo_url),
                              manifest_path, sizeof(manifest_path), commit, sizeof(commit), &pkg)) return 1;
    if (access(marker, F_OK) == 0) {
        FILE *existing = fopen(marker, "r"); char first[512] = ""; char oldname[FORGE_MAX_NAME], oldver[128] = "";
        if (existing) { if (fgets(first, sizeof(first), existing)) (void)sscanf(first, "#package\t%63s %127s", oldname, oldver); fclose(existing); }
        if (!strcmp(oldver, pkg.version)) { printf("Already installed: %s %s\n", base_name, oldver); return 0; }
        fprintf(stderr, "Forge: %s %s is already installed. Remove it with 'forge uninstall %s' before installing %s.\n", base_name, oldver[0] ? oldver : "(unknown version)", base_name, pkg.version);
        return 1;
    }
    if (!pkg.checksum[0]) {
        fprintf(stderr, "Forge: refusing to install '%s': source.checksum is empty. Add a SHA-256 checksum to forge.pkg.\n", name); return 1;
    }
    /* MESSAGE.md is optional, fetched from the same version directory as forge.pkg. */
    char owner_msg[128], repo_msg[128], message_path[2300], message_url[8192];
    char message_text[8192] = "";
    if (!forge_github_parts(repo_url, owner_msg, sizeof(owner_msg), repo_msg, sizeof(repo_msg))) {
        const char *last = strrchr(manifest_path, '/');
        if (last) {
            size_t dirlen = (size_t)(last - manifest_path);
            if (dirlen + sizeof("/MESSAGE.md") < sizeof(message_path)) {
                memcpy(message_path, manifest_path, dirlen);
                strcpy(message_path + dirlen, "/MESSAGE.md");
                int n = snprintf(message_url, sizeof(message_url), "https://raw.githubusercontent.com/%s/%s/%s/%s", owner_msg, repo_msg, commit, message_path);
                if (n > 0 && (size_t)n < sizeof(message_url)) {
                    ForgeBuffer mb;
                    if (forge_http_get(message_url, &mb, "text/markdown") == 0) {
                        size_t copylen = mb.len < sizeof(message_text)-1 ? mb.len : sizeof(message_text)-1;
                        memcpy(message_text, mb.data, copylen); message_text[copylen] = '\0'; forge_buffer_free(&mb);
                    }
                }
            }
        }
    }
    if (!ask_install(&pkg, yes, message_text)) { puts("Installation cancelled."); return 1; }
    /* Install declared Forge-package dependencies only after the user approves this package. */
    for (size_t i = 0; i < pkg.runtime_dep_count; i++)
        if (install_recursive(cfg, pkg.runtime_deps[i], yes, stack, depth + 1)) return 1;
    for (size_t i = 0; i < pkg.build_dep_count; i++)
        if (install_recursive(cfg, pkg.build_deps[i], yes, stack, depth + 1)) return 1;
    char owner[128], repo[128];
    if (forge_github_parts(repo_url, owner, sizeof(owner), repo, sizeof(repo))) return 1;
    char build_rel[2300];
    const char *slash = strrchr(manifest_path, '/');
    if (!slash) return 1;
    size_t base = (size_t)(slash - manifest_path);
    if (base + 1 + strlen(pkg.build_script) >= sizeof(build_rel)) return 1;
    memcpy(build_rel, manifest_path, base); build_rel[base] = '/';
    strcpy(build_rel + base + 1, pkg.build_script);
    char build_url[8192];
    int build_url_len = snprintf(build_url, sizeof(build_url), "https://raw.githubusercontent.com/%s/%s/%s/%s", owner, repo, commit, build_rel);
    if (build_url_len < 0 || (size_t)build_url_len >= sizeof(build_url)) { fprintf(stderr, "Forge: build script URL is too long.\n"); return 1; }
    char builds_dir[PATH_MAX]; forge_path_join(builds_dir, sizeof(builds_dir), cfg->cache_dir, "builds");
    if (forge_mkdir_p(builds_dir, 0700)) return 1;
    char templ[PATH_MAX];
    size_t builds_len = strlen(builds_dir), name_len = strlen(name);
    if (builds_len + 1 + name_len + 7 + 1 > sizeof(templ)) { fprintf(stderr, "Forge: build cache path is too long.\n"); return 1; }
    memcpy(templ, builds_dir, builds_len); templ[builds_len] = '/';
    memcpy(templ + builds_len + 1, name, name_len);
    memcpy(templ + builds_len + 1 + name_len, "-XXXXXX", 8);
    char *work = mkdtemp(templ); if (!work) { perror("mkdtemp"); return 1; }
    char archive[PATH_MAX], source[PATH_MAX], stage[PATH_MAX], script[PATH_MAX], stage_prefix[PATH_MAX];
    snprintf(archive, sizeof(archive), "%s/source.tar.gz", work);
    snprintf(source, sizeof(source), "%s/source", work);
    snprintf(stage, sizeof(stage), "%s/stage", work);
    int script_len = snprintf(script, sizeof(script), "%s/%s", source, pkg.build_script);
    if (script_len < 0 || (size_t)script_len >= sizeof(script)) { fprintf(stderr, "Forge: build script path is too long.\n"); goto fail; }
    printf("\nForge installation plan\n  Package: %s %s\n  Source archive: %s\n  Build command: %s\n", pkg.name, pkg.version, pkg.source_url, pkg.build_command);
    if (install_system_dependencies(&pkg) != 0) goto fail;
    if (check_compilers(&pkg) != 0) goto fail;
    if (download_file(pkg.source_url, archive, "source archive") != 0) goto fail;
    if (!checksum_ok(pkg.checksum, archive)) { fprintf(stderr, "Forge: source checksum verification failed.\n"); goto fail; }
    puts("Source checksum verified (SHA-256).");
    if (extract_archive(archive, source) != 0) { fprintf(stderr, "Forge: could not safely extract source archive.\n"); goto fail; }
    char script_parent[PATH_MAX]; snprintf(script_parent, sizeof(script_parent), "%s", script);
    char *last_slash = strrchr(script_parent, '/'); if (last_slash) { *last_slash = '\0'; if (forge_mkdir_p(script_parent, 0700)) goto fail; }
    if (download_file(build_url, script, "build script") != 0) goto fail;
    if (chmod(script, 0755) != 0) { perror(script); goto fail; }
    if (forge_mkdir_p(stage, 0700)) goto fail;
    /* DESTDIR + absolute PREFIX produces stage/<prefix-with-leading-slash-removed>. */
    int stage_prefix_len = snprintf(stage_prefix, sizeof(stage_prefix), "%s%s", stage, cfg->prefix);
    if (stage_prefix_len < 0 || (size_t)stage_prefix_len >= sizeof(stage_prefix) ||
        forge_mkdir_p(stage_prefix, 0700)) goto fail;
    printf("\n[build] %s %s\n[build] Working directory: %s\n[build] Command: %s\n", pkg.name, pkg.version, source, pkg.build_command);
    if (forge_run_build(pkg.build_command, work, stage, cfg->prefix, source) != 0) {
        fprintf(stderr, "Forge: build command failed for '%s'.\n", base_name); goto fail;
    }
    struct stat st;
    if (stat(stage_prefix, &st) != 0 || !S_ISDIR(st.st_mode)) {
        fprintf(stderr, "Forge: build produced no staged files under %s.\n", stage_prefix);
        fprintf(stderr, "Use install/make with DESTDIR=\"$FORGE_STAGE_DIR\" and PREFIX=\"$FORGE_PREFIX\".\n"); goto fail;
    }
    FILE *mf = fopen(marker, "w"); if (!mf) { perror(marker); goto fail; }
    fprintf(mf, "#package\t%s\t%s\n#repository\t%s\n#manifest\t%s\n", pkg.name, pkg.version, repo_name, manifest_path);
    if (forge_copy_tree(stage_prefix, cfg->prefix, "", mf, 1) != 0 || fflush(mf) != 0 || fsync(fileno(mf)) != 0) {
        fclose(mf); forge_remove_tree_files(cfg->prefix, marker); unlink(marker); goto fail;
    }
    if (fclose(mf) != 0) { forge_remove_tree_files(cfg->prefix, marker); unlink(marker); goto fail; }
    printf("\n[done] Installed %s %s into %s\n", pkg.name, pkg.version, cfg->prefix);
    /* Keep no build trees after a successful install. */
    {
        char *rm[] = {"rm", "-rf", "--", work, NULL}; (void)run_argv(rm);
    }
    return 0;
fail:
    {
        char *rm[] = {"rm", "-rf", "--", work, NULL}; (void)run_argv(rm);
    }
    return 1;
}

int forge_install(const ForgeConfig *cfg, const char *name, int assume_yes)
{
    char stack[FORGE_MAX_DEPTH][FORGE_MAX_NAME] = {{0}};
    return install_recursive(cfg, name, assume_yes, stack, 0);
}

int forge_uninstall(const ForgeConfig *cfg, const char *name)
{
    char base_name[FORGE_MAX_NAME]; const char *at = strchr(name, '@'); size_t base_len = at ? (size_t)(at - name) : strlen(name);
    if (!base_len || base_len >= sizeof(base_name)) { fprintf(stderr, "Forge: invalid package name.\n"); return 1; }
    memcpy(base_name, name, base_len); base_name[base_len] = '\0';
    if (!forge_valid_name(base_name)) { fprintf(stderr, "Forge: invalid package name.\n"); return 1; }
    char marker[PATH_MAX]; if (installed_marker(cfg, base_name, marker, sizeof(marker))) return 1;
    if (access(marker, F_OK) != 0) { fprintf(stderr, "Forge: '%s' is not installed.\n", base_name); return 1; }
    printf("Uninstall %s? [y/N] ", base_name); fflush(stdout);
    char answer[32]; if (!fgets(answer, sizeof(answer), stdin) || (answer[0] != 'y' && answer[0] != 'Y')) { puts("Cancelled."); return 1; }
    int rc = forge_remove_tree_files(cfg->prefix, marker);
    if (unlink(marker) != 0) { perror(marker); rc = -1; }
    if (!rc) printf("Removed %s.\n", base_name);
    return rc ? 1 : 0;
}

int forge_list_installed(const ForgeConfig *cfg)
{
    char dir[PATH_MAX]; forge_path_join(dir, sizeof(dir), cfg->data_dir, "installed");
    DIR *d = opendir(dir);
    if (!d) { puts("No packages installed."); return 0; }
    printf("%-22s %-14s %s\n", "PACKAGE", "VERSION", "REPOSITORY");
    struct dirent *e; size_t count = 0;
    while ((e = readdir(d)) != NULL) {
        if (e->d_name[0] == '.') continue;
        char path[PATH_MAX]; forge_path_join(path, sizeof(path), dir, e->d_name);
        FILE *f = fopen(path, "r"); if (!f) continue;
        char line[PATH_MAX], name[FORGE_MAX_NAME] = "", version[128] = "", repository[FORGE_MAX_NAME] = "unknown";
        while (fgets(line, sizeof(line), f)) {
            if (strncmp(line, "#package\t", 9) == 0) (void)sscanf(line + 9, "%63s %127s", name, version);
            else if (strncmp(line, "#repository\t", 12) == 0) { line[strcspn(line, "\r\n")] = '\0'; snprintf(repository, sizeof(repository), "%.*s", (int)sizeof(repository) - 1, line + 12); }
        }
        fclose(f);
        if (name[0] && version[0]) { printf("%-22s %-14s %s\n", name, version, repository); count++; }
    }
    closedir(d);
    if (!count) puts("No packages installed.");
    return 0;
}

static int installed_version_compare(const char *a, const char *b)
{
    if (*a == 'v' || *a == 'V') a++;
    if (*b == 'v' || *b == 'V') b++;
    const char *ap = a, *bp = b;
    while (*ap && *bp) {
        if (isdigit((unsigned char)*ap) && isdigit((unsigned char)*bp)) {
            char *ae = NULL, *be = NULL;
            unsigned long av = strtoul(ap, &ae, 10), bv = strtoul(bp, &be, 10);
            ap = ae; bp = be;
            if (av != bv) return av > bv ? 1 : -1;
        } else if (*ap == '.' && *bp == '.') { ap++; bp++; }
        else break;
    }
    int apre = strchr(a, '-') != NULL, bpre = strchr(b, '-') != NULL;
    if (apre != bpre) return apre ? -1 : 1;
    int c = strcasecmp(a, b);
    return c > 0 ? 1 : c < 0 ? -1 : 0;
}

static int installed_version_repo(const char *marker, char *name, size_t namesz,
                                  char *version, size_t versz, char *repo, size_t reposz)
{
    (void)namesz; (void)versz;
    FILE *f = fopen(marker, "r"); if (!f) return -1;
    char line[PATH_MAX]; name[0] = version[0] = repo[0] = '\0';
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "#package\t", 9) == 0) (void)sscanf(line + 9, "%63s %127s", name, version);
        else if (strncmp(line, "#repository\t", 12) == 0) { line[strcspn(line, "\r\n")] = '\0'; snprintf(repo, reposz, "%.*s", (int)(reposz > 0 ? reposz - 1 : 0), line + 12); }
    }
    fclose(f); return name[0] && version[0] ? 0 : -1;
}

int forge_upgrade(const ForgeConfig *cfg, const char *requested)
{
    char dir[PATH_MAX]; forge_path_join(dir, sizeof(dir), cfg->data_dir, "installed");
    DIR *d = opendir(dir);
    if (!d) { puts("No installed packages to upgrade."); return 0; }
    char targets[256][FORGE_MAX_NAME]; size_t count = 0; int found_requested = 0;
    struct dirent *e;
    while ((e = readdir(d)) != NULL && count < 256) {
        if (e->d_name[0] == '.') continue;
        if (requested && strcmp(e->d_name, requested)) continue;
        char marker[PATH_MAX]; forge_path_join(marker, sizeof(marker), dir, e->d_name);
        char name[FORGE_MAX_NAME], oldver[128], oldrepo[FORGE_MAX_NAME];
        if (installed_version_repo(marker, name, sizeof(name), oldver, sizeof(oldver), oldrepo, sizeof(oldrepo))) continue;
        if (requested && strcmp(name, requested)) continue;
        if (requested) found_requested = 1;
        char repo_name[FORGE_MAX_NAME], repo_url[FORGE_MAX_URL], manifest[2048], commit[128]; ForgePackage pkg;
        if (forge_resolve_package(cfg, name, repo_name, sizeof(repo_name), repo_url, sizeof(repo_url), manifest, sizeof(manifest), commit, sizeof(commit), &pkg)) {
            printf("[unknown] %s: unable to resolve latest version from synced catalogs.\n", name); continue;
        }
        int order = installed_version_compare(pkg.version, oldver);
        if (order == 0) { printf("[current] %s %s (%s)\n", name, oldver, oldrepo); continue; }
        if (order < 0) { printf("[newer]   %s %s is newer than catalog version %s; not downgrading.\n", name, oldver, pkg.version); continue; }
        printf("[update]  %s: %s -> %s (repository: %s)\n", name, oldver, pkg.version, repo_name);
        snprintf(targets[count++], FORGE_MAX_NAME, "%s", name);
    }
    closedir(d);
    if (requested && !found_requested) { fprintf(stderr, "Forge: '%s' is not installed.\n", requested); return 1; }
    if (!count) { puts("No available upgrades found."); return 0; }
    for (size_t i = 0; i < count; i++) {
        char marker[PATH_MAX], name[FORGE_MAX_NAME], oldver[128], oldrepo[FORGE_MAX_NAME];
        if (installed_marker(cfg, targets[i], marker, sizeof(marker)) || installed_version_repo(marker, name, sizeof(name), oldver, sizeof(oldver), oldrepo, sizeof(oldrepo))) continue;
        printf("Upgrade %s from %s? This replaces the currently installed version. [y/N] ", name, oldver); fflush(stdout);
        char answer[32]; if (!fgets(answer, sizeof(answer), stdin) || (answer[0] != 'y' && answer[0] != 'Y')) { puts("Skipped."); continue; }
        /* Current installer has no in-place replacement transaction; remove the old recorded files first. */
        if (forge_remove_tree_files(cfg->prefix, marker) != 0 || unlink(marker) != 0) { fprintf(stderr, "Forge: could not remove old files for %s; upgrade skipped.\n", name); continue; }
        if (forge_install(cfg, name, 0) != 0) fprintf(stderr, "Forge: upgrade failed for %s; the old version was removed. Reinstall it with 'forge install %s@%s'.\n", name, name, oldver);
    }
    return 0;
}

int forge_clean_cache(const ForgeConfig *cfg)
{
    printf("This removes Forge's cache, including downloaded files and temporary build trees. Installed packages and repository catalogs are kept.\nProceed? [y/N] "); fflush(stdout);
    char answer[32]; if (!fgets(answer, sizeof(answer), stdin) || (answer[0] != 'y' && answer[0] != 'Y')) { puts("Cancelled."); return 1; }
    char *args[] = {"rm", "-rf", "--", (char *)cfg->cache_dir, NULL};
    if (run_argv(args) != 0 || forge_mkdir_p(cfg->cache_dir, 0700) != 0) { fprintf(stderr, "Forge: failed to clean cache directory %s.\n", cfg->cache_dir); return 1; }
    printf("Forge cache cleaned: %s\n", cfg->cache_dir); return 0;
}
