#define _GNU_SOURCE

#define _POSIX_C_SOURCE 200809L

#include "forge.h"

#include <ctype.h>

#include <stdlib.h>

#include <string.h>

#include <strings.h>

#include <sys/stat.h>

#include <unistd.h>



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

    if (forge_http_get(url, &b, "text/plain", false) != 0) return -1;

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



typedef struct {
    char name[FORGE_MAX_NAME];
    char repo[FORGE_MAX_NAME];
    char **versions;
    size_t version_count;
    size_t version_capacity;
} SearchGroup;

static void free_search_groups(SearchGroup *groups, size_t count)
{
    for (size_t i = 0; i < count; i++) {
        for (size_t j = 0; j < groups[i].version_count; j++)
            free(groups[i].versions[j]);
        free(groups[i].versions);
    }
    free(groups);
}

static int add_search_version(SearchGroup *group, const char *version)
{
    for (size_t i = 0; i < group->version_count; i++)
        if (strcmp(group->versions[i], version) == 0) return 0;

    if (group->version_count == group->version_capacity) {
        size_t capacity = group->version_capacity ? group->version_capacity * 2 : 8;
        char **next = realloc(group->versions, capacity * sizeof(*next));
        if (!next) return -1;
        group->versions = next;
        group->version_capacity = capacity;
    }

    group->versions[group->version_count] = strdup(version);
    if (!group->versions[group->version_count]) return -1;
    group->version_count++;
    return 0;
}

static int search_version_desc(const void *left, const void *right)
{
    const char *const *a = left;
    const char *const *b = right;
    return -version_compare(*a, *b);
}

int forge_search(const ForgeConfig *cfg, const char *query)
{
    if (!query || !*query) {
        fprintf(stderr, "Usage: forge search <name-fragment>\n");
        return 1;
    }

    ForgeRepo repos[FORGE_MAX_REPOS];
    size_t repo_count = 0;
    if (forge_load_repos(cfg, repos, &repo_count)) return 1;

    SearchGroup *groups = NULL;
    size_t group_count = 0, group_capacity = 0;
    size_t total_versions = 0;

    for (size_t r = 0; r < repo_count; r++) {
        char catdir[PATH_MAX], catpath[PATH_MAX];
        if (forge_path_join(catdir, sizeof(catdir), cfg->data_dir, "catalog") ||
            forge_path_join(catpath, sizeof(catpath), catdir, repos[r].name))
            continue;

        FILE *f = fopen(catpath, "r");
        if (!f) continue;

        char line[LINE_CAP];
        if (fgets(line, sizeof(line), f) && line[0] != '#') rewind(f);

        while (fgets(line, sizeof(line), f)) {
            if (line[0] == '#') continue;
            char *tab = strchr(line, '\t');
            if (!tab) continue;
            *tab = '\0';

            if (strcasestr(line, query) == NULL) continue;

            char *at = strchr(line, '@');
            size_t name_len = at ? (size_t)(at - line) : strlen(line);
            if (!name_len || name_len >= FORGE_MAX_NAME) continue;

            char name[FORGE_MAX_NAME];
            memcpy(name, line, name_len);
            name[name_len] = '\0';

            size_t g = 0;
            while (g < group_count &&
                   (strcmp(groups[g].name, name) != 0 ||
                    strcmp(groups[g].repo, repos[r].name) != 0))
                g++;

            if (g == group_count) {
                if (group_count == group_capacity) {
                    size_t capacity = group_capacity ? group_capacity * 2 : 16;
                    SearchGroup *next = realloc(groups, capacity * sizeof(*next));
                    if (!next) {
                        fclose(f);
                        free_search_groups(groups, group_count);
                        fprintf(stderr, "Forge: out of memory while searching catalogs.\n");
                        return 1;
                    }
                    groups = next;
                    group_capacity = capacity;
                }
                memset(&groups[group_count], 0, sizeof(groups[group_count]));
                snprintf(groups[group_count].name, sizeof(groups[group_count].name), "%s", name);
                snprintf(groups[group_count].repo, sizeof(groups[group_count].repo), "%s", repos[r].name);
                g = group_count++;
            }

            const char *version = at && at[1] ? at + 1 : "(unversioned)";
            if (add_search_version(&groups[g], version) != 0) {
                fclose(f);
                free_search_groups(groups, group_count);
                fprintf(stderr, "Forge: out of memory while searching catalogs.\n");
                return 1;
            }
            total_versions++;
        }
        fclose(f);
    }

    if (!group_count) {
        free_search_groups(groups, group_count);
        puts("No packages found in the local catalogs.");
        puts("Try 'forge repo sync' to refresh catalogs; matching is case-insensitive.");
        return 1;
    }

    /* Respect NO_COLOR and avoid escape sequences when output is redirected. */
    int use_color = isatty(STDOUT_FILENO) && getenv("NO_COLOR") == NULL;
    const char *bold = use_color ? "\033[1m" : "";
    const char *cyan = use_color ? "\033[36m" : "";
    const char *green = use_color ? "\033[32m" : "";
    const char *blue = use_color ? "\033[34m" : "";
    const char *yellow = use_color ? "\033[33m" : "";
    const char *dim = use_color ? "\033[2m" : "";
    const char *reset = use_color ? "\033[0m" : "";

    printf("%s%s%-22s %-14s %s%s\n", bold, cyan,
           "PACKAGE", "REPOSITORY", "AVAILABLE VERSIONS", reset);
    for (size_t i = 0; i < group_count; i++) {
        SearchGroup *group = &groups[i];
        qsort(group->versions, group->version_count, sizeof(*group->versions), search_version_desc);

        printf("%s%-22s%s %s%-14s%s ", green, group->name, reset,
               blue, group->repo, reset);
        size_t shown = group->version_count < 2 ? group->version_count : 2;
        for (size_t j = 0; j < shown; j++)
            printf("%s%s%s%s", j ? ", " : "", yellow, group->versions[j], reset);

        if (group->version_count > shown)
            printf(" %s(+%zu more)%s", dim, group->version_count - shown, reset);
        putchar('\n');
    }

    printf("%s%s%zu package(s) matched%s '%s' (%zu version entries).%s\n",
           bold, cyan, group_count, reset, query, total_versions, reset);
    free_search_groups(groups, group_count);
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

    /* Colour only interactive terminal output; honour the NO_COLOR convention. */
    int use_color = isatty(STDOUT_FILENO) && getenv("NO_COLOR") == NULL;
    const char *bold = use_color ? "\033[1m" : "";
    const char *cyan = use_color ? "\033[36m" : "";
    const char *green = use_color ? "\033[32m" : "";
    const char *blue = use_color ? "\033[34m" : "";
    const char *yellow = use_color ? "\033[33m" : "";
    const char *magenta = use_color ? "\033[35m" : "";
    const char *dim = use_color ? "\033[2m" : "";
    const char *reset = use_color ? "\033[0m" : "";

#define INFO_FIELD(label, color, value) \
    printf("%s%-13s%s %s%s%s\n", bold, label, reset, color, value, reset)

    INFO_FIELD("Name:", green, pkg.name);
    INFO_FIELD("Version:", yellow, pkg.version);
    INFO_FIELD("Description:", cyan, pkg.description);
    INFO_FIELD("License:", magenta, pkg.license);
    INFO_FIELD("Repository:", blue, ref.repo_name);
    INFO_FIELD("Source:", dim, pkg.source_url);
    INFO_FIELD("Checksum:", pkg.checksum[0] ? green : yellow,
                pkg.checksum[0] ? pkg.checksum : "(missing; installation disabled)");
    INFO_FIELD("Build script:", cyan, pkg.build_script);
    INFO_FIELD("Build command:", cyan, pkg.build_command);

    if (pkg.compiler_count) {
        printf("%s%-13s%s ", bold, "Compilers:", reset);
        for (size_t i = 0; i < pkg.compiler_count; i++)
            printf("%s%s%s%s", i ? ", " : "", yellow, pkg.compilers[i], reset);
        putchar('\n');
    }

    if (pkg.apt_dep_count || pkg.pacman_dep_count || pkg.dnf_dep_count || pkg.zypper_dep_count) {
        printf("%sSystem dependencies by package manager:%s\n", bold, reset);
        if (pkg.apt_dep_count) {
            printf("  %s%-7s%s ", cyan, "apt:", reset);
            for (size_t i = 0; i < pkg.apt_dep_count; i++) printf("%s%s%s%s", i ? ", " : "", yellow, pkg.apt_deps[i], reset);
            putchar('\n');
        }
        if (pkg.pacman_dep_count) {
            printf("  %s%-7s%s ", cyan, "pacman:", reset);
            for (size_t i = 0; i < pkg.pacman_dep_count; i++) printf("%s%s%s%s", i ? ", " : "", yellow, pkg.pacman_deps[i], reset);
            putchar('\n');
        }
        if (pkg.dnf_dep_count) {
            printf("  %s%-7s%s ", cyan, "dnf:", reset);
            for (size_t i = 0; i < pkg.dnf_dep_count; i++) printf("%s%s%s%s", i ? ", " : "", yellow, pkg.dnf_deps[i], reset);
            putchar('\n');
        }
        if (pkg.zypper_dep_count) {
            printf("  %s%-7s%s ", cyan, "zypper:", reset);
            for (size_t i = 0; i < pkg.zypper_dep_count; i++) printf("%s%s%s%s", i ? ", " : "", yellow, pkg.zypper_deps[i], reset);
            putchar('\n');
        }
    }

    if (pkg.runtime_dep_count) {
        printf("%s%-13s%s", bold, "Runtime deps:", reset);
        for (size_t i = 0; i < pkg.runtime_dep_count; i++)
            printf(" %s%s%s%s", i ? "," : "", yellow, pkg.runtime_deps[i], reset);
        putchar('\n');
    }

    if (pkg.build_dep_count) {
        printf("%s%-13s%s", bold, "Build deps:", reset);
        for (size_t i = 0; i < pkg.build_dep_count; i++)
            printf(" %s%s%s%s", i ? "," : "", yellow, pkg.build_deps[i], reset);
        putchar('\n');
    }

    INFO_FIELD("Manifest:", dim, ref.path);
    INFO_FIELD("Commit:", blue, ref.commit);
#undef INFO_FIELD

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
