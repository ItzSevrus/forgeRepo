#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200809L
#include "forge.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>

#define LINE_CAP 4096

typedef struct {
    char repo_name[FORGE_MAX_NAME];
    char repo_url[FORGE_MAX_URL];
    char package_name[FORGE_MAX_NAME];
    char path[2048];
    char commit[128];
} PackageRef;

static int get_catalog(const ForgeConfig *cfg, const ForgeRepo *repo,
                       const char *query, int exact, PackageRef *found, size_t *matches,
                       int print_matches)
{
    char catdir[PATH_MAX], path[PATH_MAX];
    forge_path_join(catdir, sizeof(catdir), cfg->data_dir, "catalog");
    forge_path_join(path, sizeof(path), catdir, repo->name);
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    char line[LINE_CAP], commit[128] = "";
    if (fgets(line, sizeof(line), f) && line[0] == '#') {
        if (sscanf(line, "#commit\t%127s", commit) != 1) commit[0] = '\0';
    } else rewind(f);
    while (fgets(line, sizeof(line), f)) {
        if (line[0] == '#') continue;
        char *tab = strchr(line, '\t'); if (!tab) continue;
        *tab++ = '\0'; tab[strcspn(tab, "\r\n")] = '\0';
        if (query && (exact ? strcasecmp(line, query) != 0 : strcasestr(line, query) == NULL)) continue;
        if (print_matches) printf("%-22s %-14s %s\n", line, repo->name, tab);
        if (found && *matches == 0) {
            snprintf(found->repo_name, sizeof(found->repo_name), "%s", repo->name);
            snprintf(found->repo_url, sizeof(found->repo_url), "%s", repo->url);
            if (strlen(line) >= sizeof(found->package_name)) continue;
            memcpy(found->package_name, line, strlen(line) + 1);
            snprintf(found->path, sizeof(found->path), "%s", tab);
            snprintf(found->commit, sizeof(found->commit), "%s", commit);
        }
        (*matches)++;
    }
    fclose(f);
    return 1;
}

static int version_compare(const char *a, const char *b)
{
    /* Small, deterministic version ordering: numeric dot components, then prerelease below release. */
    if (*a == 'v' || *a == 'V') a++;
    if (*b == 'v' || *b == 'V') b++;
    const char *ap = a, *bp = b;
    while (*ap && *bp) {
        if (isdigit((unsigned char)*ap) && isdigit((unsigned char)*bp)) {
            char *ae = NULL, *be = NULL;
            unsigned long av = strtoul(ap, &ae, 10);
            unsigned long bv = strtoul(bp, &be, 10);
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

static int find_package(const ForgeConfig *cfg, const char *request, PackageRef *ref, size_t *matches)
{
    char base[FORGE_MAX_NAME], selector[128] = "latest";
    const char *at = strchr(request, '@');
    size_t base_len = at ? (size_t)(at - request) : strlen(request);
    if (!base_len || base_len >= sizeof(base) || (at && (!at[1] || strchr(at + 1, '@')))) {
        fprintf(stderr, "Forge: package must be name, name@latest, or name@v1.0.0.\n"); return -1;
    }
    memcpy(base, request, base_len); base[base_len] = '\0';
    if (!forge_valid_name(base)) { fprintf(stderr, "Forge: invalid package name.\n"); return -1; }
    if (at) {
        if (strlen(at + 1) >= sizeof(selector)) return -1;
        snprintf(selector, sizeof(selector), "%s", at + 1);
    }
    int latest = !strcmp(selector, "latest");
    ForgeRepo repos[FORGE_MAX_REPOS]; size_t count = 0;
    if (forge_load_repos(cfg, repos, &count)) return -1;
    *matches = 0;
    for (size_t i = 0; i < count; i++) {
        char catdir[PATH_MAX], catpath[PATH_MAX];
        if (forge_path_join(catdir, sizeof(catdir), cfg->data_dir, "catalog") ||
            forge_path_join(catpath, sizeof(catpath), catdir, repos[i].name)) continue;
        FILE *f = fopen(catpath, "r"); if (!f) continue;
        char line[LINE_CAP], commit[128] = "";
        if (fgets(line, sizeof(line), f) && line[0] == '#') (void)sscanf(line, "#commit\t%127s", commit); else rewind(f);
        PackageRef candidate = {0}; char bestver[128] = ""; int have = 0;
        while (fgets(line, sizeof(line), f)) {
            if (line[0] == '#') continue;
            char *tab = strchr(line, '\t'); if (!tab) continue;
            *tab++ = '\0'; tab[strcspn(tab, "\r\n")] = '\0';
            char *keyat = strchr(line, '@');
            char keybase[FORGE_MAX_NAME]; size_t klen = keyat ? (size_t)(keyat - line) : strlen(line);
            if (!klen || klen >= sizeof(keybase)) continue;
            memcpy(keybase, line, klen); keybase[klen] = '\0';
            if (strcmp(keybase, base)) continue;
            const char *ver = keyat ? keyat + 1 : "";
            if (!latest && (!keyat || strcmp(ver, selector))) continue;
            if (latest && keyat && have && version_compare(ver, bestver) <= 0) continue;
            if (latest && !keyat && have) continue; /* versioned entries outrank legacy */
            have = 1; snprintf(bestver, sizeof(bestver), "%s", ver);
            memcpy(candidate.repo_name, repos[i].name, sizeof(candidate.repo_name)); candidate.repo_name[sizeof(candidate.repo_name) - 1] = '\0';
            memcpy(candidate.repo_url, repos[i].url, sizeof(candidate.repo_url)); candidate.repo_url[sizeof(candidate.repo_url) - 1] = '\0';
            snprintf(candidate.package_name, sizeof(candidate.package_name), "%s", base);
            snprintf(candidate.path, sizeof(candidate.path), "%s", tab);
            snprintf(candidate.commit, sizeof(candidate.commit), "%s", commit);
        }
        fclose(f);
        if (have) {
            (*matches)++;
            if (*matches == 1) *ref = candidate;
        }
    }
    return 0;
}

static int raw_url(const PackageRef *ref, const char *path, char *out, size_t outsz)
{
    char owner[128], repo[128];
    if (forge_github_parts(ref->repo_url, owner, sizeof(owner), repo, sizeof(repo))) return -1;
    int n = snprintf(out, outsz, "https://raw.githubusercontent.com/%s/%s/%s/%s", owner, repo,
                     ref->commit, path);
    return n < 0 || (size_t)n >= outsz ? -1 : 0;
}

static int fetch_manifest(const PackageRef *ref, ForgePackage *pkg)
{
    char url[FORGE_MAX_URL];
    if (raw_url(ref, ref->path, url, sizeof(url))) return -1;
    ForgeBuffer b;
    if (forge_http_get(url, &b, "text/plain") != 0) return -1;
    char err[256];
    int rc = forge_manifest_parse(b.data, pkg, err, sizeof(err));
    if (rc) fprintf(stderr, "Forge: invalid manifest for %s: %s\n", ref->package_name, err);
    forge_buffer_free(&b);
    if (!rc && strcmp(pkg->name, ref->package_name) != 0) {
        fprintf(stderr, "Forge: manifest name mismatch for %s.\n", ref->package_name); return -1;
    }
    if (!rc) {
        const char *last = strrchr(ref->path, '/');
        if (last && last > ref->path) {
            const char *prev = last - 1;
            while (prev > ref->path && *prev != '/') prev--;
            if (*prev == '/') {
                const char *dirver = prev + 1;
                size_t dirlen = (size_t)(last - dirver);
                if (dirlen > 1 && dirver[0] == 'v') {
                    char expected[128];
                    if (dirlen >= sizeof(expected)) return -1;
                    memcpy(expected, dirver + 1, dirlen - 1); expected[dirlen - 1] = '\0';
                    if (strcmp(expected, pkg->version) != 0) {
                        fprintf(stderr, "Forge: version directory '%.*s' does not match manifest version '%s'.\n", (int)dirlen, dirver, pkg->version);
                        return -1;
                    }
                }
            }
        }
    }
    return rc;
}

int forge_search(const ForgeConfig *cfg, const char *query)
{
    if (!query || !*query) { fprintf(stderr, "Usage: forge search <name-fragment>\n"); return 1; }
    ForgeRepo repos[FORGE_MAX_REPOS]; size_t count = 0;
    if (forge_load_repos(cfg, repos, &count)) return 1;
    printf("%-22s %-14s %s\n", "PACKAGE", "REPOSITORY", "MANIFEST");
    size_t matches = 0;
    for (size_t i = 0; i < count; i++) get_catalog(cfg, &repos[i], query, 0, NULL, &matches, 1);
    if (!matches) {
        puts("No packages found in the local catalogs.");
        puts("Try 'forge repo sync' to refresh catalogs; matching is case-insensitive.");
        return 1;
    }
    printf("%zu package(s) matched '%s'.\n", matches, query);
    return 0;
}

int forge_info(const ForgeConfig *cfg, const char *name)
{
    PackageRef ref; size_t matches;
    if (find_package(cfg, name, &ref, &matches)) return 1;
    if (!matches) { fprintf(stderr, "Forge: package '%s' not found. Run 'forge repo sync'.\n", name); return 1; }
    if (matches > 1) { fprintf(stderr, "Forge: '%s' exists in multiple repositories; remove duplicates or use a unique package name.\n", name); return 1; }
    ForgePackage pkg;
    if (fetch_manifest(&ref, &pkg)) return 1;
    printf("Name:        %s\nVersion:     %s\nDescription: %s\nLicense:     %s\nRepository:  %s\nSource:      %s\nChecksum:    %s\nBuild script: %s\nBuild command: %s\n",
           pkg.name, pkg.version, pkg.description, pkg.license, ref.repo_name, pkg.source_url,
           pkg.checksum[0] ? pkg.checksum : "(missing; installation disabled)", pkg.build_script, pkg.build_command);
    if (pkg.compiler_count) { printf("Compilers:   "); for (size_t i = 0; i < pkg.compiler_count; i++) printf("%s%s", i ? ", " : "", pkg.compilers[i]); puts(""); }
    if (pkg.apt_dep_count || pkg.pacman_dep_count || pkg.dnf_dep_count || pkg.zypper_dep_count) {
        puts("System dependencies by package manager:");
        if (pkg.apt_dep_count) { printf("  apt:    "); for (size_t i = 0; i < pkg.apt_dep_count; i++) printf("%s%s", i ? ", " : "", pkg.apt_deps[i]); puts(""); }
        if (pkg.pacman_dep_count) { printf("  pacman: "); for (size_t i = 0; i < pkg.pacman_dep_count; i++) printf("%s%s", i ? ", " : "", pkg.pacman_deps[i]); puts(""); }
        if (pkg.dnf_dep_count) { printf("  dnf:    "); for (size_t i = 0; i < pkg.dnf_dep_count; i++) printf("%s%s", i ? ", " : "", pkg.dnf_deps[i]); puts(""); }
        if (pkg.zypper_dep_count) { printf("  zypper: "); for (size_t i = 0; i < pkg.zypper_dep_count; i++) printf("%s%s", i ? ", " : "", pkg.zypper_deps[i]); puts(""); }
    }
    if (pkg.runtime_dep_count) {
        printf("Runtime deps:"); for (size_t i = 0; i < pkg.runtime_dep_count; i++) printf(" %s%s", i ? "," : "", pkg.runtime_deps[i]); puts("");
    }
    if (pkg.build_dep_count) {
        printf("Build deps:  "); for (size_t i = 0; i < pkg.build_dep_count; i++) printf(" %s%s", i ? "," : "", pkg.build_deps[i]); puts("");
    }
    printf("Manifest:    %s\nCommit:      %.12s\n", ref.path, ref.commit);
    return 0;
}

/* Used by installer: resolve a unique package from the synced catalog. */
int forge_resolve_package(const ForgeConfig *cfg, const char *name, char *repo_name,
                          size_t repo_sz, char *repo_url, size_t url_sz,
                          char *path, size_t path_sz, char *commit, size_t commit_sz,
                          ForgePackage *pkg)
{
    PackageRef ref; size_t matches;
    if (find_package(cfg, name, &ref, &matches)) return -1;
    if (!matches) { fprintf(stderr, "Forge: package '%s' not found; run 'forge repo sync'.\n", name); return -1; }
    if (matches > 1) { fprintf(stderr, "Forge: package '%s' is present in multiple repositories; refusing ambiguous install.\n", name); return -1; }
    if (fetch_manifest(&ref, pkg)) return -1;
    snprintf(repo_name, repo_sz, "%s", ref.repo_name);
    snprintf(repo_url, url_sz, "%s", ref.repo_url);
    snprintf(path, path_sz, "%s", ref.path);
    snprintf(commit, commit_sz, "%s", ref.commit);
    return 0;
}
