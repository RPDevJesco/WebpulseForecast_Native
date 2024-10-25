#include "web_resource_analyzer.h"

static void cache_dependency(const char* name, const char* version) {
    for (int i = 0; i < dep_cache.count; i++) {
        if (strcmp(dep_cache.items[i].name, name) == 0) {
            dep_cache.items[i].count++;
            return;
        }
    }

    if (dep_cache.count < MAX_CACHED_DEPS) {
        strncpy(dep_cache.items[dep_cache.count].name, name, 99);
        strncpy(dep_cache.items[dep_cache.count].version, version, 19);
        dep_cache.items[dep_cache.count].count = 1;
        dep_cache.count++;
    }
}

void generate_dependency_statistics(ProjectType* project) {
    if (!project) return;

    // Reset any existing dependency counts
    project->total_dependencies = 0;
    project->dev_dependencies = 0;
    project->prod_dependencies = 0;
    project->framework_dependencies = 0;

    // Use the cache to compute statistics
    for (int i = 0; i < dep_cache.count; i++) {
        project->total_dependencies++;

        // Check for framework-related dependencies
        if (strstr(dep_cache.items[i].name, "react") ||
            strstr(dep_cache.items[i].name, "vue") ||
            strstr(dep_cache.items[i].name, "angular") ||
            strstr(dep_cache.items[i].name, "svelte")) {
            project->framework_dependencies++;
        }

        // Additional statistics could be added here
        // For example, counting dependencies by type, checking versions, etc.
    }

    // Add dependency-related issues if needed
    if (project->total_dependencies > 100) {
        snprintf(project->potential_issues[project->potential_issue_count].description, 255,
                 "High number of dependencies (%d) may impact maintenance and security",
                 project->total_dependencies);
        project->potential_issue_count++;
    }

    if (project->framework_dependencies > 1) {
        snprintf(project->potential_issues[project->potential_issue_count].description, 255,
                 "Multiple framework dependencies detected (%d) - consider consolidating",
                 project->framework_dependencies);
        project->potential_issue_count++;
    }
}

static void parse_workspace_globs(const char* content, ProjectType* project) {
    const char* ptr = strstr(content, "\"workspaces\"");
    if (!ptr) return;

    // Skip to array start
    ptr = strchr(ptr, '[');
    if (!ptr) return;
    ptr++;

    while (*ptr && project->workspace.workspace_count < MAX_WORKSPACES) {
        // Skip whitespace
        while (*ptr && isspace(*ptr)) ptr++;

        if (*ptr == '"') {
            ptr++; // Skip opening quote
            char* glob = project->workspace.workspace_globs[project->workspace.workspace_count];
            int i = 0;

            // Copy until closing quote or max length
            while (*ptr && *ptr != '"' && i < 99) {
                glob[i++] = *ptr++;
            }
            glob[i] = '\0';

            if (*ptr == '"') {
                project->workspace.workspace_count++;
                ptr++; // Skip closing quote
            }
        }

        // Move to next item
        while (*ptr && *ptr != ',' && *ptr != ']') ptr++;
        if (*ptr == ',') ptr++;
        if (*ptr == ']') break;
    }
}

static void parse_json_array(const char* json, char globs[][100], int* count, int max_count) {
    const char* ptr = strchr(json, '[');
    if (!ptr) return;

    ptr++;
    while (*ptr && *count < max_count) {
        while (*ptr && isspace(*ptr)) ptr++;
        if (*ptr != '"') break;

        ptr++;
        int i = 0;
        while (*ptr && *ptr != '"' && i < 99) {
            globs[*count][i++] = *ptr++;
        }
        globs[*count][i] = '\0';
        (*count)++;

        while (*ptr && *ptr != ',' && *ptr != ']') ptr++;
        if (*ptr == ',') ptr++;
    }
}

static void extract_module_name(const char* ptr, char* module_name, size_t size) {
    while (*ptr && *ptr != '(') ptr++;
    if (!*ptr) return;

    ptr++;
    while (*ptr && isspace(*ptr)) ptr++;
    if (*ptr != '"' && *ptr != '\'') return;

    char quote = *ptr++;
    size_t i = 0;
    while (*ptr && *ptr != quote && i < size - 1) {
        module_name[i++] = *ptr++;
    }
    module_name[i] = '\0';
}

static void extract_import_path(const char* ptr, char* module_name, size_t size) {
    // Skip "import" keyword
    ptr += 6;
    while (*ptr && isspace(*ptr)) ptr++;

    // Handle different import syntaxes
    if (*ptr == '{' || *ptr == '*') {
        // Skip to "from" keyword
        while (*ptr && strncmp(ptr, "from", 4) != 0) ptr++;
        if (!*ptr) return;
        ptr += 4;
    }

    while (*ptr && isspace(*ptr)) ptr++;
    if (*ptr != '"' && *ptr != '\'') return;

    char quote = *ptr++;
    size_t i = 0;
    while (*ptr && *ptr != quote && i < size - 1) {
        module_name[i++] = *ptr++;
    }
    module_name[i] = '\0';
}

static char* parse_version(const char* content, const char* package_name) {
    static char version[20];
    version[0] = '\0';

    char search_pattern[256];
    snprintf(search_pattern, sizeof(search_pattern), "\"%s\": \"", package_name);

    const char* ptr = strstr(content, search_pattern);
    if (ptr) {
        ptr += strlen(search_pattern);
        int i = 0;
        while (*ptr && *ptr != '"' && i < sizeof(version) - 1) {
            version[i++] = *ptr++;
        }
        version[i] = '\0';
    }

    return version;
}

static void update_shared_dependency_count(const char* name, ProjectType* project) {
    int count = 0;
    for (int i = 0; i < project->workspace.package_count; i++) {
        Package* pkg = &project->workspace.packages[i];
        for (int j = 0; j < pkg->dependencies.count; j++) {
            if (strcmp(pkg->dependencies.items[j].name, name) == 0) {
                count++;
                break;
            }
        }
    }

    // If dependency is used in multiple packages, add to shared dependencies
    if (count > 1) {
        for (int i = 0; i < project->workspace.shared_dependencies.count; i++) {
            if (strcmp(project->workspace.shared_dependencies.items[i].name, name) == 0) {
                return;
            }
        }

        if (project->workspace.shared_dependencies.count < MAX_DEPENDENCIES) {
            Dependency* dep = &project->workspace.shared_dependencies.items[
                    project->workspace.shared_dependencies.count++];
            strncpy(dep->name, name, sizeof(dep->name) - 1);
            // Version will be added when processing package.json
        }
    }
}

static void analyze_workspace_package_json(const char* content, ProjectType* project) {
    // Check for Yarn workspaces
    if (strstr(content, "\"workspaces\"")) {
        project->workspace.is_yarn_workspace = 1;
        project->is_monorepo = 1;
        parse_workspace_globs(content, project);
    }

    // Parse shared dependencies
    parse_dependencies_section(content, "dependencies", &project->workspace.shared_dependencies, 0);
    parse_dependencies_section(content, "devDependencies", &project->workspace.shared_dependencies, 1);
}

// Helper function for sorting dependencies
static int compare_dependencies(const void* a, const void* b) {
    return strcmp(((const Dependency*)a)->name, ((const Dependency*)b)->name);
}

static void parse_global_deps(const char* deps_json, ProjectType* project) {
    if (!deps_json || !project) return;

    const char* ptr = deps_json;

    // Find the opening of globalDependencies object
    ptr = strchr(ptr, '{');
    if (!ptr) return;

    ptr++; // Skip opening brace

    while (*ptr && *ptr != '}') {
        // Skip whitespace
        while (*ptr && isspace(*ptr)) ptr++;

        if (*ptr == '"') {
            ptr++; // Skip opening quote

            // Parse dependency name
            char dep_name[256] = {0};
            int i = 0;
            while (*ptr && *ptr != '"' && i < sizeof(dep_name) - 1) {
                dep_name[i++] = *ptr++;
            }
            dep_name[i] = '\0';

            if (*ptr == '"') {
                ptr++; // Skip closing quote

                // Look for value
                while (*ptr && *ptr != ':') ptr++;
                if (*ptr == ':') {
                    ptr++; // Skip colon

                    // Skip whitespace
                    while (*ptr && isspace(*ptr)) ptr++;

                    if (*ptr == '"') {
                        ptr++; // Skip opening quote

                        // Parse dependency value/version
                        char dep_value[256] = {0};
                        i = 0;
                        while (*ptr && *ptr != '"' && i < sizeof(dep_value) - 1) {
                            dep_value[i++] = *ptr++;
                        }
                        dep_value[i] = '\0';

                        // Process the global dependency
                        if (strlen(dep_name) > 0) {
                            // Add to shared dependencies if not already present
                            int found = 0;
                            for (i = 0; i < project->workspace.shared_dependencies.count; i++) {
                                if (strcmp(project->workspace.shared_dependencies.items[i].name, dep_name) == 0) {
                                    found = 1;
                                    break;
                                }
                            }

                            if (!found && project->workspace.shared_dependencies.count < MAX_DEPENDENCIES) {
                                Dependency* dep = &project->workspace.shared_dependencies.items[
                                    project->workspace.shared_dependencies.count++];

                                strncpy(dep->name, dep_name, sizeof(dep->name) - 1);
                                strncpy(dep->version, dep_value, sizeof(dep->version) - 1);
                                dep->is_dev_dependency = 0; // Global deps are typically not dev deps
                            }

                            // Handle special global dependencies
                            if (strcmp(dep_name, "tsconfig.json") == 0) {
                                project->workspace.has_shared_configs = 1;
                                strncpy(project->workspace.tsconfig_path, dep_value,
                                        sizeof(project->workspace.tsconfig_path) - 1);
                            }
                            else if (strcmp(dep_name, ".eslintrc") == 0) {
                                project->workspace.has_shared_configs = 1;
                                strncpy(project->workspace.eslint_config_path, dep_value,
                                        sizeof(project->workspace.eslint_config_path) - 1);
                            }
                            else if (strcmp(dep_name, ".prettierrc") == 0) {
                                project->workspace.has_shared_configs = 1;
                                strncpy(project->workspace.prettier_config_path, dep_value,
                                        sizeof(project->workspace.prettier_config_path) - 1);
                            }
                            else if (strstr(dep_name, "jest.config") != NULL) {
                                project->workspace.has_shared_configs = 1;
                                strncpy(project->workspace.jest_config_path, dep_value,
                                        sizeof(project->workspace.jest_config_path) - 1);
                            }
                            else if (strcmp(dep_name, "package.json") == 0) {
                                // Parse root package.json for workspace configuration
                                char full_path[MAX_PATH];
                                snprintf(full_path, sizeof(full_path), "%s/%s",
                                        project->workspace.root_path, dep_value);

                                FILE* f = fopen(full_path, "r");
                                if (f) {
                                    fseek(f, 0, SEEK_END);
                                    long fsize = ftell(f);
                                    fseek(f, 0, SEEK_SET);

                                    if (fsize > 0 && fsize < 1024 * 1024) { // 1MB limit
                                        char* content = (char*)malloc(fsize + 1);
                                        if (content) {
                                            if (fread(content, 1, fsize, f) == fsize) {
                                                content[fsize] = '\0';
                                                analyze_workspace_package_json(content, project);
                                            }
                                            free(content);
                                        }
                                    }
                                    fclose(f);
                                }
                            }
                            // Handle build tool configurations
                            else if (strstr(dep_name, "webpack.config") != NULL) {
                                project->has_webpack = 1;
                            }
                            else if (strstr(dep_name, "vite.config") != NULL) {
                                project->has_vite = 1;
                            }
                            else if (strstr(dep_name, "babel.config") != NULL) {
                                project->has_babel = 1;
                            }
                            // Handle CI/CD configurations
                            else if (strstr(dep_name, ".github/workflows") != NULL) {
                                project->has_ci = 1;
                            }
                            else if (strcmp(dep_name, ".env") == 0) {
                                project->has_env_config = 1;
                            }
                        }
                    }
                }
            }
        }

        // Move to next entry
        ptr = strchr(ptr, ',');
        if (!ptr) break;
        ptr++; // Skip comma
    }

    // Process collected information
    if (project->workspace.shared_dependencies.count > 0) {
        // Sort shared dependencies by name for easier lookup
        qsort(project->workspace.shared_dependencies.items,
              project->workspace.shared_dependencies.count,
              sizeof(Dependency),
              compare_dependencies);
    }
}

static void parse_lerna_config(const char* root_path, ProjectType* project) {
    char config_path[MAX_PATH];
    snprintf(config_path, MAX_PATH, "%s/lerna.json", root_path);

    FILE* f = fopen(config_path, "r");
    if (!f) return;

    // Get file size and read content
    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);

    char* content = malloc(fsize + 1);
    if (!content) {
        fclose(f);
        return;
    }

    if (fread(content, 1, fsize, f) == fsize) {
        content[fsize] = 0;

        // Parse version management
        if (strstr(content, "\"version\": \"independent\"")) {
            strcpy(project->workspace.version_strategy, "independent");
        } else {
            strcpy(project->workspace.version_strategy, "fixed");
        }

        // Parse package locations
        const char* packages = strstr(content, "\"packages\"");
        if (packages) {
            parse_json_array(packages, project->workspace.workspace_globs,
                             &project->workspace.workspace_count, MAX_WORKSPACES);
        }

        // Parse npm client preference
        if (strstr(content, "\"npmClient\": \"yarn\"")) {
            project->workspace.is_yarn_workspace = 1;
        } else if (strstr(content, "\"npmClient\": \"pnpm\"")) {
            project->workspace.is_pnpm_workspace = 1;
        }

        // Parse useWorkspaces flag
        if (strstr(content, "\"useWorkspaces\": true")) {
            project->workspace.uses_npm_workspaces = 1;
        }
    }

    free(content);
    fclose(f);
}

static void parse_pnpm_workspace(const char* root_path, ProjectType* project) {
    char config_path[MAX_PATH];
    snprintf(config_path, MAX_PATH, "%s/pnpm-workspace.yaml", root_path);

    FILE* f = fopen(config_path, "r");
    if (!f) return;

    char line[1024];
    while (fgets(line, sizeof(line), f)) {
        // Parse package patterns
        if (strstr(line, "packages:")) {
            while (fgets(line, sizeof(line), f) && strstr(line, "- ")) {
                char* pattern = line + 2;
                // Trim whitespace and newlines
                char* end = pattern + strlen(pattern) - 1;
                while (end > pattern && isspace(*end)) *end-- = '\0';

                if (project->workspace.workspace_count < MAX_WORKSPACES) {
                    strncpy(project->workspace.workspace_globs[project->workspace.workspace_count++],
                            pattern, 99);
                }
            }
        }
    }

    fclose(f);
}

static void analyze_turbo_config(const char* root_path, ProjectType* project) {
    char config_path[MAX_PATH];
    snprintf(config_path, MAX_PATH, "%s/turbo.json", root_path);

    FILE* f = fopen(config_path, "r");
    if (!f) return;

    project->workspace.uses_turborepo = 1;

    // Read and parse turbo.json
    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);

    char* content = malloc(fsize + 1);
    if (!content) {
        fclose(f);
        return;
    }

    if (fread(content, 1, fsize, f) == fsize) {
        content[fsize] = 0;

        // Parse pipeline configuration
        const char* pipeline = strstr(content, "\"pipeline\"");
        if (pipeline) {
            parse_turbo_pipeline(pipeline, project);
        }

        // Parse global dependencies
        const char* globalDeps = strstr(content, "\"globalDependencies\"");
        if (globalDeps) {
            parse_global_deps(globalDeps, project);
        }
    }

    free(content);
    fclose(f);
}

static void analyze_package_interdependencies(ProjectType* project) {
    for (int i = 0; i < project->workspace.package_count; i++) {
        Package* pkg = &project->workspace.packages[i];

        // Check each dependency to see if it's another workspace package
        for (int j = 0; j < pkg->dependencies.count; j++) {
            Dependency* dep = &pkg->dependencies.items[j];

            // Look for internal dependencies
            for (int k = 0; k < project->workspace.package_count; k++) {
                if (strcmp(dep->name, project->workspace.packages[k].name) == 0) {
                    // Add package reference
                    PackageReference* ref = &pkg->config.refs[pkg->config.ref_count++];
                    strncpy(ref->source, pkg->name, sizeof(ref->source) - 1);
                    strncpy(ref->target, dep->name, sizeof(ref->target) - 1);
                    break;
                }
            }
        }
    }
}

static void detect_build_order(ProjectType* project) {
    // Create task groups based on dependencies
    char (*build_order)[MAX_PACKAGES][100] = NULL;
    build_order = malloc(sizeof(*build_order));
    if (!build_order) {
        fprintf(stderr, "Failed to allocate memory for build order\n");
        return;
    }
    memset(build_order, 0, sizeof(*build_order));
    int order_count = 0;

    // Initialize build order with independent packages
    for (int i = 0; i < project->workspace.package_count; i++) {
        Package* pkg = &project->workspace.packages[i];
        if (pkg->config.ref_count == 0 && order_count < MAX_PACKAGES) {
            strncpy(&(*build_order)[0][order_count * 100], pkg->name, 99);
            (*build_order)[0][order_count * 100 + 99] = '\0';
            order_count++;
        }
    }

    // Add dependent packages in correct order
    int level = 1;
    while (order_count < project->workspace.package_count && level < MAX_PACKAGES) {
        for (int i = 0; i < project->workspace.package_count; i++) {
            Package* pkg = &project->workspace.packages[i];

            // Check if package is already in build order
            int already_added = 0;
            for (int k = 0; k < level; k++) {
                for (int l = 0; l < MAX_PACKAGES; l++) {
                    const char* current_pkg = &(*build_order)[k][l * 100];
                    if (current_pkg[0] && strcmp(pkg->name, current_pkg) == 0) {
                        already_added = 1;
                        break;
                    }
                }
                if (already_added) break;
            }
            if (already_added) continue;

            // Check if all dependencies are in previous levels
            int can_build = 1;
            for (int j = 0; j < pkg->config.ref_count; j++) {
                int dep_found = 0;
                for (int k = 0; k < level; k++) {
                    for (int l = 0; l < MAX_PACKAGES; l++) {
                        const char* current_pkg = &(*build_order)[k][l * 100];
                        if (current_pkg[0] && strcmp(pkg->config.refs[j].target, current_pkg) == 0) {
                            dep_found = 1;
                            break;
                        }
                    }
                    if (dep_found) break;
                }
                if (!dep_found) {
                    can_build = 0;
                    break;
                }
            }

            if (can_build && (order_count % MAX_PACKAGES) < MAX_PACKAGES) {
                strncpy(&(*build_order)[level][(order_count % MAX_PACKAGES) * 100],
                        pkg->name, 99);
                (*build_order)[level][(order_count % MAX_PACKAGES) * 100 + 99] = '\0';
                order_count++;
            }
        }
        level++;
    }

    // Create task groups from build order
    for (int i = 0; i < level && i < MAX_PACKAGES; i++) {
        if (project->workspace.task_group_count >= sizeof(project->workspace.task_groups) / sizeof(TaskGroup)) {
            break;
        }

        TaskGroup* group = &project->workspace.task_groups[project->workspace.task_group_count++];
        snprintf(group->name, sizeof(group->name), "build-level-%d", i + 1);
        strncpy(group->type, "build", sizeof(group->type) - 1);
        group->type[sizeof(group->type) - 1] = '\0';

        for (int j = 0; j < MAX_PACKAGES; j++) {
            const char* current_pkg = &(*build_order)[i][j * 100];
            if (!current_pkg[0] || group->package_count >= MAX_PACKAGES) break;
            strncpy(group->packages[group->package_count++], current_pkg, 99);
        }
    }

    free(build_order);
}

static void clean_version_string(char* version) {
    if (!version) return;

    // Handle caret and tilde
    if (version[0] == '^' || version[0] == '~') {
        // Shift everything left
        memmove(version, version + 1, strlen(version));
    }

    // Handle version ranges
    char* range_separator = strstr(version, " - ");
    if (range_separator) {
        // Just keep the first version
        *range_separator = '\0';
    }

    // Handle >=, >, <=, < prefixes
    if (version[0] == '>' || version[0] == '<') {
        char* space = strchr(version, ' ');
        if (space) {
            // Shift everything after the space to the beginning
            memmove(version, space + 1, strlen(space + 1) + 1);
        }
    }

    // Handle x and * wildcards
    char* wildcard = strpbrk(version, "x*");
    if (wildcard) {
        // If it's a wildcard, just truncate at that point
        *wildcard = '\0';

        // Remove trailing dot if exists
        if (wildcard > version && *(wildcard - 1) == '.') {
            *(wildcard - 1) = '\0';
        }
    }

    // Handle || operator
    char* or_operator = strstr(version, "||");
    if (or_operator) {
        // Just keep the first version
        *or_operator = '\0';
    }

    // Trim whitespace
    char* end = version + strlen(version) - 1;
    while (end > version && isspace((unsigned char)*end)) {
        *end = '\0';
        end--;
    }
}

static void extract_dependency_version(const char* content, const char* package_name, char* version_out, size_t version_size) {
    if (!content || !package_name || !version_out || version_size == 0) return;

    // Initialize output
    version_out[0] = '\0';

    // Create search patterns for different locations
    char deps_pattern[256];
    char dev_deps_pattern[256];
    char engines_pattern[256];

    snprintf(deps_pattern, sizeof(deps_pattern), "\"%s\": \"", package_name);
    snprintf(dev_deps_pattern, sizeof(dev_deps_pattern), "devDependencies\".*\"%s\": \"", package_name);
    snprintf(engines_pattern, sizeof(engines_pattern), "\"engines\".*\"%s\": \"", package_name);

    const char* version_start = NULL;

    // Try dependencies first
    version_start = strstr(content, deps_pattern);

    // If not found, try devDependencies
    if (!version_start) {
        version_start = strstr(content, dev_deps_pattern);
    }

    // If still not found and it's node/npm, try engines
    if (!version_start && (strcmp(package_name, "node") == 0 || strcmp(package_name, "npm") == 0)) {
        version_start = strstr(content, engines_pattern);
    }

    if (version_start) {
        // Move to the version string start
        version_start = strchr(version_start, '"');
        if (version_start) {
            version_start++; // Skip the opening quote

            // Find the end of the version string
            const char* version_end = strchr(version_start, '"');
            if (version_end) {
                size_t version_length = version_end - version_start;
                if (version_length < version_size) {
                    // Copy the version string
                    strncpy(version_out, version_start, version_length);
                    version_out[version_length] = '\0';

                    // Clean up the version string
                    clean_version_string(version_out);
                }
            }
        }
    }
}

static int is_valid_version(const char* version) {
    if (!version || !*version) return 0;

    // Check if it starts with a digit or v followed by a digit
    if (!isdigit((unsigned char)version[0]) &&
        !(version[0] == 'v' && isdigit((unsigned char)version[1]))) {
        return 0;
    }

    // Count the number of dots (should be 0, 1, or 2 for valid semver)
    int dot_count = 0;
    for (const char* p = version; *p; p++) {
        if (*p == '.') dot_count++;
        else if (!isdigit((unsigned char)*p) && *p != 'v') return 0;
    }

    return dot_count <= 2;
}

static void detect_framework_usage(const char* content, FrameworkInfo* framework) {
    if (!content || !framework) return;

    // React Detection
    if (strstr(content, "\"react\"") || strstr(content, "\"react-dom\"")) {
        framework->has_react = 1;

        // Detect React Hooks
        const char* hooks[] = {
                "useState", "useEffect", "useContext", "useReducer",
                "useCallback", "useMemo", "useRef", "useLayoutEffect"
        };

        for (size_t i = 0; i < sizeof(hooks)/sizeof(hooks[0]); i++) {
            const char* ptr = content;
            while ((ptr = strstr(ptr, hooks[i])) != NULL) {
                // Verify it's a hook call and not just a string containing the name
                char prev = (ptr > content) ? *(ptr - 1) : '\0';
                char next = *(ptr + strlen(hooks[i]));
                if ((!isalnum(prev) && prev != '_') &&
                    (!isalnum(next) && next != '_')) {
                    framework->react_hooks_count++;
                }
                ptr++;
            }
        }

        // Check for Next.js
        if (strstr(content, "\"next\"") ||
            strstr(content, "\"next.config.js\"") ||
            strstr(content, "pages/_app") ||
            strstr(content, "pages/api/")) {
            framework->has_nextjs = 1;
        }
    }

    // Vue.js Detection
    if (strstr(content, "\"vue\"")) {
        framework->has_vue = 1;

        // Check for Composition API
        if (strstr(content, "setup()") ||
            strstr(content, "<script setup>") ||
            strstr(content, "@vue/composition-api") ||
            strstr(content, "vue@3")) {
            framework->vue_composition_api = 1;
        }

        // Check for Nuxt.js
        if (strstr(content, "\"nuxt\"") ||
            strstr(content, "\"@nuxt/") ||
            strstr(content, "nuxt.config.js")) {
            framework->has_nuxtjs = 1;
        }
    }

    // Angular Detection
    if (strstr(content, "\"@angular/core\"") ||
        strstr(content, "@Component") ||
        strstr(content, "@Injectable")) {
        framework->has_angular = 1;
    }

    // Svelte Detection
    if (strstr(content, "\"svelte\"") ||
        strstr(content, "<script context=\"module\">") ||
        strstr(content, "export let")) {
        framework->has_svelte = 1;
    }

    // Node.js Detection
    if (strstr(content, "require(") ||
        strstr(content, "module.exports") ||
        strstr(content, "process.env") ||
        strstr(content, "\"express\"") ||
        strstr(content, "\"koa\"") ||
        strstr(content, "\"fastify\"") ||
        strstr(content, "\"nest\"")) {
        framework->has_nodejs = 1;
    }

    // Check for TypeScript usage
    if (strstr(content, "\"typescript\"") ||
        strstr(content, "tsconfig.json") ||
        strstr(content, ".ts\"") ||
        strstr(content, ".tsx\"")) {
        framework->uses_typescript = 1;
    }

    // Check for test frameworks
    if (strstr(content, "\"jest\"") ||
        strstr(content, "\"@testing-library/react\"") ||
        strstr(content, "\"@vue/test-utils\"") ||
        strstr(content, "\"@angular/testing\"")) {
        framework->has_testing = 1;
    }

    // Check for build tools
    if (strstr(content, "\"webpack\"") ||
        strstr(content, "\"vite\"") ||
        strstr(content, "\"rollup\"") ||
        strstr(content, "\"parcel\"")) {
        framework->has_bundler = 1;
    }

    // Check for state management
    if (strstr(content, "\"redux\"") ||
        strstr(content, "\"@reduxjs/toolkit\"") ||
        strstr(content, "\"vuex\"") ||
        strstr(content, "\"pinia\"") ||
        strstr(content, "\"mobx\"") ||
        strstr(content, "\"recoil\"")) {
        framework->has_state_management = 1;
    }

    // Check for styling solutions
    if (strstr(content, "\"styled-components\"") ||
        strstr(content, "\"@emotion/") ||
        strstr(content, "\"tailwindcss\"") ||
        strstr(content, "\"sass\"") ||
        strstr(content, "\"less\"")) {
        framework->has_css_framework = 1;
    }

    // Additional framework features
    if (strstr(content, "\"react-router\"") ||
        strstr(content, "\"vue-router\"") ||
        strstr(content, "\"@angular/router\"")) {
        framework->has_routing = 1;
    }

    // Form libraries
    if (strstr(content, "\"formik\"") ||
        strstr(content, "\"react-hook-form\"") ||
        strstr(content, "\"@angular/forms\"") ||
        strstr(content, "\"vee-validate\"")) {
        framework->has_form_library = 1;
    }

    // UI component libraries
    if (strstr(content, "\"@mui/") ||
        strstr(content, "\"antd\"") ||
        strstr(content, "\"@chakra-ui/") ||
        strstr(content, "\"vuetify\"") ||
        strstr(content, "\"@angular/material\"")) {
        framework->has_ui_library = 1;
    }

    // CSS and Styling Detection
    if (strstr(content, "\"styled-components\"")) {
        framework->has_css_framework = 1;
        framework->uses_css_in_js = 1;
        strncpy(framework->css_solution, "styled-components", sizeof(framework->css_solution) - 1);
    } else if (strstr(content, "\"tailwindcss\"")) {
        framework->has_css_framework = 1;
        framework->uses_tailwind = 1;
        strncpy(framework->css_solution, "tailwind", sizeof(framework->css_solution) - 1);
    } else if (strstr(content, "\"sass\"")) {
        framework->uses_sass = 1;
        strncpy(framework->css_solution, "sass", sizeof(framework->css_solution) - 1);
    }

    // Testing Framework Detection
    if (strstr(content, "\"jest\"")) {
        framework->has_testing = 1;
        framework->has_unit_testing = 1;
    }
    if (strstr(content, "\"cypress\"") || strstr(content, "\"playwright\"")) {
        framework->has_testing = 1;
        framework->has_e2e_testing = 1;
    }
    if (strstr(content, "\"@testing-library/")) {
        framework->has_testing = 1;
        framework->has_component_testing = 1;
    }

    // Development Tools
    if (strstr(content, "\"webpack-dev-server\"") || strstr(content, "\"vite\"")) {
        framework->has_dev_server = 1;
        framework->has_hot_reload = 1;
    }

    // Package Manager Detection
    if (strstr(content, "package-lock.json")) {
        framework->uses_npm = 1;
    } else if (strstr(content, "yarn.lock")) {
        framework->uses_yarn = 1;
    } else if (strstr(content, "pnpm-lock.yaml")) {
        framework->uses_pnpm = 1;
    }

    // CI/CD and Deployment
    if (strstr(content, ".github/workflows") || strstr(content, ".travis.yml")) {
        framework->has_ci_cd = 1;
    }
    if (strstr(content, "Dockerfile") || strstr(content, "docker-compose")) {
        framework->has_docker = 1;
    }
    if (strstr(content, "vercel.json") || strstr(content, "netlify.toml")) {
        framework->has_deployment_config = 1;
    }

    // Code Quality Tools
    if (strstr(content, "\"eslint\"")) {
        framework->has_linting = 1;
    }
    if (strstr(content, "\"prettier\"")) {
        framework->has_formatting = 1;
    }

    // Extract versions
    if (framework->uses_typescript) {
        extract_dependency_version(content, "typescript",
                                   framework->typescript_version,
                                   sizeof(framework->typescript_version));
    }

    // Extract Node.js version from engines field
    extract_dependency_version(content, "node",
                               framework->node_version,
                               sizeof(framework->node_version));

    // Extract bundler version
    if (strstr(content, "\"webpack\"")) {
        strncpy(framework->primary_bundler, "webpack", sizeof(framework->primary_bundler) - 1);
        extract_dependency_version(content, "webpack",
                                   framework->primary_bundler + strlen("webpack") + 1,
                                   sizeof(framework->primary_bundler) - strlen("webpack") - 2);
    } else if (strstr(content, "\"vite\"")) {
        strncpy(framework->primary_bundler, "vite", sizeof(framework->primary_bundler) - 1);
        extract_dependency_version(content, "vite",
                                   framework->primary_bundler + strlen("vite") + 1,
                                   sizeof(framework->primary_bundler) - strlen("vite") - 2);
    }
}

static void parse_turbo_pipeline(const char* pipeline_json, ProjectType* project) {
    if (!pipeline_json || !project) return;

    const char* ptr = pipeline_json;

    // Skip to the first opening brace
    while (*ptr && *ptr != '{') ptr++;
    if (!*ptr) return;

    ptr++; // Skip the opening brace

    while (*ptr) {
        // Skip whitespace
        while (*ptr && isspace(*ptr)) ptr++;

        if (*ptr == '"') {
            // Found a task name
            ptr++; // Skip the quote
            char task_name[50] = {0};
            int i = 0;

            // Extract task name
            while (*ptr && *ptr != '"' && i < 49) {
                task_name[i++] = *ptr++;
            }

            if (*ptr == '"') {
                ptr++; // Skip closing quote

                // Look for dependencies array
                const char* deps = strstr(ptr, "\"dependsOn\"");
                if (deps) {
                    deps = strchr(deps, '[');
                    if (deps) {
                        // Create a new task group
                        if (project->workspace.task_group_count < 20) {
                            TaskGroup* group = &project->workspace.task_groups[
                                    project->workspace.task_group_count++];
                            strncpy(group->name, task_name, sizeof(group->name) - 1);
                            strncpy(group->type, "build", sizeof(group->type) - 1);

                            // Parse dependencies
                            deps++; // Skip the opening bracket
                            while (*deps && *deps != ']') {
                                if (*deps == '"') {
                                    deps++; // Skip opening quote
                                    char dep_name[100] = {0};
                                    i = 0;

                                    while (*deps && *deps != '"' && i < 99) {
                                        dep_name[i++] = *deps++;
                                    }

                                    if (group->package_count < MAX_PACKAGES) {
                                        strncpy(group->packages[group->package_count++],
                                                dep_name, 99);
                                    }
                                }
                                deps++;
                            }
                        }
                    }
                }
            }
        }

        // Move to next character
        ptr++;

        // Check for end of pipeline section
        if (*ptr == '}') break;
    }
}

static void parse_package_info(const char* content, Package* pkg) {
    if (!content || !pkg) return;

    // Parse package name
    const char* name_start = strstr(content, "\"name\"");
    if (name_start) {
        name_start = strchr(name_start, ':');
        if (name_start) {
            name_start = strchr(name_start, '"');
            if (name_start) {
                name_start++; // Skip the quote
                int i = 0;
                while (*name_start && *name_start != '"' && i < sizeof(pkg->name) - 1) {
                    pkg->name[i++] = *name_start++;
                }
                pkg->name[i] = '\0';
            }
        }
    }

    // Parse version
    const char* version_start = strstr(content, "\"version\"");
    if (version_start) {
        version_start = strchr(version_start, ':');
        if (version_start) {
            version_start = strchr(version_start, '"');
            if (version_start) {
                version_start++; // Skip the quote
                int i = 0;
                while (*version_start && *version_start != '"' && i < sizeof(pkg->version) - 1) {
                    pkg->version[i++] = *version_start++;
                }
                pkg->version[i] = '\0';
            }
        }
    }

    // Check for build configuration
    if (strstr(content, "\"build\"")) {
        const char* build_start = strstr(content, "\"outDir\"");
        if (build_start) {
            build_start = strchr(build_start, ':');
            if (build_start) {
                build_start = strchr(build_start, '"');
                if (build_start) {
                    build_start++; // Skip the quote
                    int i = 0;
                    while (*build_start && *build_start != '"' &&
                           i < sizeof(pkg->config.build_output_path) - 1) {
                        pkg->config.build_output_path[i++] = *build_start++;
                    }
                    pkg->config.build_output_path[i] = '\0';
                }
            }
        }
    }

    // Check for TypeScript configuration
    pkg->config.uses_typescript = (strstr(content, "\"typescript\"") != NULL) ||
                                (strstr(content, "\"@types/") != NULL);

    // Check for testing frameworks
    pkg->config.uses_jest = (strstr(content, "\"jest\"") != NULL);

    // Check for linting
    pkg->config.uses_eslint = (strstr(content, "\"eslint\"") != NULL);

    // Parse scripts
    const char* scripts = strstr(content, "\"scripts\"");
    if (scripts) {
        scripts = strchr(scripts, '{');
        if (scripts) {
            scripts++; // Skip the opening brace
            while (*scripts && *scripts != '}' &&
                   pkg->config.script_count < sizeof(pkg->config.scripts)/sizeof(PackageScript)) {

                if (*scripts == '"') {
                    scripts++; // Skip the quote
                    PackageScript* script = &pkg->config.scripts[pkg->config.script_count];

                    // Parse script name
                    int i = 0;
                    while (*scripts && *scripts != '"' && i < sizeof(script->script_name) - 1) {
                        script->script_name[i++] = *scripts++;
                    }
                    script->script_name[i] = '\0';

                    // Skip to command
                    scripts = strchr(scripts, ':');
                    if (!scripts) break;
                    scripts = strchr(scripts, '"');
                    if (!scripts) break;
                    scripts++; // Skip the quote

                    // Parse command
                    i = 0;
                    while (*scripts && *scripts != '"' && i < sizeof(script->command) - 1) {
                        script->command[i++] = *scripts++;
                    }
                    script->command[i] = '\0';

                    pkg->config.script_count++;
                }
                scripts++;
            }
        }
    }
}

static void parse_dependencies_section(const char* content, const char* section_name, DependencyList* deps, int is_dev) {
    if (!content || !section_name || !deps) {
        TRACE("Null pointer passed to parse_dependencies_section");
        return;
    }

    // Create the section key with quotes and bounds checking
    char section_key[256];
    int key_len = snprintf(section_key, sizeof(section_key), "\"%s\"", section_name);
    if (key_len < 0 || key_len >= sizeof(section_key)) {
        TRACE("Section key too long: %s", section_name);
        return;
    }

    // Find the section
    const char* section = strstr(content, section_key);
    if (!section) {
        TRACE("Section not found: %s", section_name);
        return;
    }

    // Find the opening brace
    const char* content_end = content + strlen(content);
    section = strchr(section, '{');
    if (!section || section >= content_end) {
        TRACE("Opening brace not found for section: %s", section_name);
        return;
    }

    section++; // Skip the opening brace

    while (section && section < content_end && *section && *section != '}' &&
           deps->count < MAX_DEPENDENCIES) {

        // Skip whitespace
        while (section < content_end && *section && isspace(*section)) {
            section++;
        }

        if (section >= content_end) break;

        if (*section == '"') {
            section++; // Skip the opening quote
            if (section >= content_end) break;

            // Parse package name
            char name[100] = {0};
            int i = 0;
            while (section < content_end && *section && *section != '"' &&
                   i < sizeof(name) - 1) {
                name[i++] = *section++;
            }
            name[i] = '\0';

            if (section < content_end && *section == '"') {
                section++; // Skip closing quote

                // Find the version
                const char* version_start = strchr(section, ':');
                if (version_start && version_start < content_end) {
                    version_start = strchr(version_start, '"');
                    if (version_start && version_start < content_end) {
                        version_start++; // Skip opening quote

                        // Parse version
                        char version[20] = {0};
                        i = 0;
                        while (version_start < content_end && *version_start &&
                               *version_start != '"' && i < sizeof(version) - 1) {
                            version[i++] = *version_start++;
                        }
                        version[i] = '\0';

                        // Validate dependency name and version
                        if (strlen(name) > 0 && strlen(version) > 0) {
                            // Cache the dependency before adding to the list
                            cache_dependency(name, version);

                            // Add to dependency list
                            if (deps->count < MAX_DEPENDENCIES) {
                                Dependency* dep = &deps->items[deps->count];
                                strncpy(dep->name, name, sizeof(dep->name) - 1);
                                dep->name[sizeof(dep->name) - 1] = '\0';
                                strncpy(dep->version, version, sizeof(dep->version) - 1);
                                dep->version[sizeof(dep->version) - 1] = '\0';
                                dep->is_dev_dependency = is_dev;
                                deps->count++;

                                TRACE("Added dependency: %s@%s (%s)",
                                      dep->name, dep->version,
                                      is_dev ? "dev" : "prod");
                            } else {
                                TRACE("Max dependencies reached (%d)", MAX_DEPENDENCIES);
                                return;
                            }
                        }
                    }
                }
            }
        }

        // Move to next entry
        section = strchr(section, ',');
        if (!section || section >= content_end) break;
        section++;
    }
}

// Helper function to safely get a JSON value
static char* get_json_string_value(const char* content, const char* key, char* buffer, size_t buffer_size) {
    if (!content || !key || !buffer || buffer_size == 0) return NULL;

    char search_key[256];
    snprintf(search_key, sizeof(search_key), "\"%s\"", key);

    const char* key_pos = strstr(content, search_key);
    if (!key_pos) return NULL;

    const char* value_start = strchr(key_pos, ':');
    if (!value_start) return NULL;

    // Skip whitespace after colon
    value_start++;
    while (*value_start && isspace(*value_start)) value_start++;

    if (*value_start != '"') return NULL;
    value_start++; // Skip opening quote

    size_t i = 0;
    while (*value_start && *value_start != '"' && i < buffer_size - 1) {
        buffer[i++] = *value_start++;
    }
    buffer[i] = '\0';

    return (*value_start == '"') ? buffer : NULL;
}

static void analyze_package_dependencies(const char* content, Package* pkg) {
    if (!content || !pkg) return;

    // Parse regular dependencies
    parse_dependencies_section(content, "dependencies", &pkg->dependencies, 0);

    // Parse dev dependencies
    DependencyList dev_deps = {0};
    parse_dependencies_section(content, "devDependencies", &dev_deps, 1);

    // Merge dev dependencies into main dependencies list
    for (int i = 0; i < dev_deps.count && pkg->dependencies.count < MAX_DEPENDENCIES; i++) {
        pkg->dependencies.items[pkg->dependencies.count] = dev_deps.items[i];
        pkg->dependencies.count++;
    }

    // Analyze for framework dependencies
    for (int i = 0; i < pkg->dependencies.count; i++) {
        const char* dep_name = pkg->dependencies.items[i].name;

        // React detection
        if (strcmp(dep_name, "react") == 0 ||
            strcmp(dep_name, "react-dom") == 0) {
            pkg->framework_info.has_react = 1;
        }
            // Vue detection
        else if (strcmp(dep_name, "vue") == 0) {
            pkg->framework_info.has_vue = 1;
            // Check for Composition API
            if (strstr(content, "@vue/composition-api") ||
                strstr(content, "vue@3")) {
                pkg->framework_info.vue_composition_api = 1;
            }
        }
            // Angular detection
        else if (strncmp(dep_name, "@angular/", 9) == 0) {
            pkg->framework_info.has_angular = 1;
        }
            // Svelte detection
        else if (strcmp(dep_name, "svelte") == 0) {
            pkg->framework_info.has_svelte = 1;
        }
            // Node.js specific packages
        else if (strcmp(dep_name, "express") == 0 ||
                 strcmp(dep_name, "koa") == 0 ||
                 strcmp(dep_name, "fastify") == 0) {
            pkg->framework_info.has_nodejs = 1;
        }
    }

    // Check for build tools and testing frameworks
    const char* test_frameworks[] = {"jest", "mocha", "jasmine", "karma"};
    const char* build_tools[] = {"webpack", "rollup", "parcel", "esbuild"};

    for (int i = 0; i < pkg->dependencies.count; i++) {
        const char* dep_name = pkg->dependencies.items[i].name;

        // Check for testing frameworks
        for (size_t j = 0; j < sizeof(test_frameworks)/sizeof(test_frameworks[0]); j++) {
            if (strstr(dep_name, test_frameworks[j])) {
                pkg->config.uses_jest = 1;  // Using jest as a general testing flag
                break;
            }
        }

        // Check for build tools
        for (size_t j = 0; j < sizeof(build_tools)/sizeof(build_tools[0]); j++) {
            if (strstr(dep_name, build_tools[j])) {
                pkg->config.uses_typescript |= (strcmp(dep_name, "typescript") == 0);
                break;
            }
        }
    }
}

static void analyze_package_workspace(const char* path, ProjectType* project) {
    if (project->workspace.package_count >= MAX_PACKAGES) return;

    Package* pkg = &project->workspace.packages[project->workspace.package_count];
    strncpy(pkg->path, path, MAX_PATH - 1);

    // Read package.json for this package
    char package_json_path[MAX_PATH];
    snprintf(package_json_path, MAX_PATH, "%s/package.json", path);

    FILE* f = fopen(package_json_path, "r");
    if (!f) return;

    // Get file size
    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (fsize > 0 && fsize < 1024 * 1024) { // 1MB limit for package.json
        char* content = malloc(fsize + 1);
        if (content) {
            if (fread(content, 1, fsize, f) == fsize) {
                content[fsize] = 0;
                parse_package_info(content, pkg);
                analyze_package_dependencies(content, pkg);
                detect_framework_usage(content, &pkg->framework_info);
            }
            free(content);
        }
    }
    fclose(f);

    project->workspace.package_count++;
}

static void scan_workspace_glob(const char* root_path, const char* glob_pattern, ProjectType* project) {
    char full_path[MAX_PATH];
    tinydir_dir dir;

    // Handle glob patterns
    // Currently supporting basic patterns like "packages/*" or "apps/*"
    char base_dir[MAX_PATH] = {0};
    const char* wildcard = strchr(glob_pattern, '*');

    if (wildcard) {
        // Copy everything before the wildcard
        size_t prefix_len = wildcard - glob_pattern;
        strncpy(base_dir, glob_pattern, prefix_len);
        base_dir[prefix_len] = '\0';
    } else {
        strncpy(base_dir, glob_pattern, sizeof(base_dir) - 1);
    }

    // Construct full path
    snprintf(full_path, sizeof(full_path), "%s/%s", root_path, base_dir);

    if (tinydir_open(&dir, full_path) == -1) {
        fprintf(stderr, "Error opening workspace directory: %s\n", full_path);
        return;
    }

    while (dir.has_next) {
        tinydir_file file;
        if (tinydir_readfile(&dir, &file) == -1) {
            fprintf(stderr, "Error reading file in workspace: %s\n", full_path);
            break;
        }

        if (file.is_dir && strcmp(file.name, ".") != 0 && strcmp(file.name, "..") != 0) {
            // If there's a wildcard, check if this directory matches the pattern
            int should_process = 1;
            if (wildcard && *(wildcard + 1) != '\0') {
                const char* pattern = wildcard + 1;
                const char* name_end = file.name + strlen(file.name);
                should_process = 0;

                // Check if the file name ends with the pattern
                if (strlen(pattern) <= strlen(file.name)) {
                    const char* name_suffix = name_end - strlen(pattern);
                    if (strcmp(name_suffix, pattern) == 0) {
                        should_process = 1;
                    }
                }
            }

            if (should_process) {
                // Check for package.json
                char package_path[MAX_PATH];
                snprintf(package_path, sizeof(package_path), "%s/package.json", file.path);

                FILE* f = fopen(package_path, "r");
                if (f) {
                    fseek(f, 0, SEEK_END);
                    long fsize = ftell(f);
                    fseek(f, 0, SEEK_SET);

                    if (fsize > 0 && fsize < 1024 * 1024) { // 1MB limit
                        char* content = (char*)malloc(fsize + 1);
                        if (content) {
                            if (fread(content, 1, fsize, f) == fsize) {
                                content[fsize] = 0;
                                analyze_package_workspace(file.path, project);
                            }
                            free(content);
                        }
                    }
                    fclose(f);
                }
            }
        }

        if (tinydir_next(&dir) == -1) break;
    }

    tinydir_close(&dir);
}

static void parse_nx_workspace(const char* root_path, ProjectType* project) {
    char workspace_path[MAX_PATH];
    char nx_json_path[MAX_PATH];

    // Check for nx.json
    snprintf(nx_json_path, sizeof(nx_json_path), "%s/nx.json", root_path);
    FILE* f = fopen(nx_json_path, "r");
    if (!f) return;

    // Get file size
    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (fsize > 0 && fsize < 1024 * 1024) { // 1MB limit
        char* content = (char*)malloc(fsize + 1);
        if (content) {
            if (fread(content, 1, fsize, f) == fsize) {
                content[fsize] = 0;

                // Parse projects configuration
                const char* projects = strstr(content, "\"projects\"");
                if (projects) {
                    projects = strchr(projects, '{');
                    if (projects) {
                        projects++; // Skip opening brace

                        while (*projects && *projects != '}') {
                            if (*projects == '"') {
                                projects++; // Skip opening quote

                                // Get project path
                                char project_path[MAX_PATH] = {0};
                                int i = 0;
                                while (*projects && *projects != '"' && i < sizeof(project_path) - 1) {
                                    project_path[i++] = *projects++;
                                }

                                if (*projects == '"') {
                                    // Found a valid project path
                                    snprintf(workspace_path, sizeof(workspace_path),
                                             "%s/%s", root_path, project_path);
                                    analyze_package_workspace(workspace_path, project);
                                }
                            }
                            projects++;
                        }
                    }
                }

                // Parse workspace configuration
                if (strstr(content, "\"npmScope\"")) {
                    project->workspace.is_nx_workspace = 1;
                }

                // Parse task configuration
                const char* targets = strstr(content, "\"targetDefaults\"");
                if (targets) {
                    targets = strchr(targets, '{');
                    if (targets) {
                        targets++;

                        while (*targets && *targets != '}') {
                            if (*targets == '"') {
                                targets++; // Skip quote

                                // Get target name
                                char target_name[50] = {0};
                                int i = 0;
                                while (*targets && *targets != '"' && i < sizeof(target_name) - 1) {
                                    target_name[i++] = *targets++;
                                }

                                if (*targets == '"' &&
                                    project->workspace.task_group_count < sizeof(project->workspace.task_groups)/sizeof(TaskGroup)) {
                                    // Create new task group
                                    TaskGroup* group = &project->workspace.task_groups[project->workspace.task_group_count++];
                                    strncpy(group->name, target_name, sizeof(group->name) - 1);
                                    strncpy(group->type, "nx-target", sizeof(group->type) - 1);
                                }
                            }
                            targets++;
                        }
                    }
                }
            }
            free(content);
        }
    }
    fclose(f);

    // Check for workspace.json
    snprintf(workspace_path, sizeof(workspace_path), "%s/workspace.json", root_path);
    f = fopen(workspace_path, "r");
    if (f) {
        // Similar parsing for workspace.json
        fseek(f, 0, SEEK_END);
        long wsize = ftell(f);
        fseek(f, 0, SEEK_SET);

        if (wsize > 0 && wsize < 1024 * 1024) {
            char* content = (char*)malloc(wsize + 1);
            if (content) {
                if (fread(content, 1, wsize, f) == wsize) {
                    content[wsize] = 0;

                    // Parse additional workspace configuration
                    if (strstr(content, "\"version\"")) {
                        // Extract version management strategy
                        const char* version = strstr(content, "\"version\"");
                        if (version) {
                            version = strchr(version, ':');
                            if (version) {
                                version = strchr(version, '"');
                                if (version) {
                                    version++;
                                    char version_str[20] = {0};
                                    int i = 0;
                                    while (*version && *version != '"' && i < sizeof(version_str) - 1) {
                                        version_str[i++] = *version++;
                                    }
                                    strncpy(project->workspace.version_strategy, version_str,
                                            sizeof(project->workspace.version_strategy) - 1);
                                }
                            }
                        }
                    }
                }
                free(content);
            }
        }
        fclose(f);
    }
}

static void parse_rush_config(const char* root_path, ProjectType* project) {
    if (!root_path || !project) return;

    char rush_json_path[MAX_PATH];
    snprintf(rush_json_path, sizeof(rush_json_path), "%s/rush.json", root_path);

    FILE* f = fopen(rush_json_path, "r");
    if (!f) return;

    // Read the rush.json file
    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (fsize <= 0 || fsize > 10 * 1024 * 1024) { // Size limit: 10MB
        fclose(f);
        return;
    }

    char* content = (char*)malloc(fsize + 1);
    if (!content) {
        fclose(f);
        return;
    }

    size_t read_size = fread(content, 1, fsize, f);
    fclose(f);

    if (read_size != fsize) {
        free(content);
        return;
    }
    content[fsize] = '\0';

    // Parse Rush configuration
    const char* projects = strstr(content, "\"projects\"");
    if (projects) {
        projects = strchr(projects, '[');
        if (projects) {
            projects++; // Skip opening bracket

            while (*projects && *projects != ']') {
                if (*projects == '{') {
                    // Parse project entry
                    const char* packageName = strstr(projects, "\"packageName\"");
                    const char* projectFolder = strstr(projects, "\"projectFolder\"");

                    if (packageName && projectFolder) {
                        // Extract package name
                        packageName = strchr(packageName, ':');
                        if (packageName) {
                            packageName = strchr(packageName, '"');
                            if (packageName) {
                                packageName++; // Skip opening quote
                                char name[100] = {0};
                                int i = 0;
                                while (*packageName && *packageName != '"' && i < 99) {
                                    name[i++] = *packageName++;
                                }

                                // Extract project folder
                                projectFolder = strchr(projectFolder, ':');
                                if (projectFolder) {
                                    projectFolder = strchr(projectFolder, '"');
                                    if (projectFolder) {
                                        projectFolder++; // Skip opening quote
                                        char folder[MAX_PATH] = {0};
                                        i = 0;
                                        while (*projectFolder && *projectFolder != '"' && i < MAX_PATH - 1) {
                                            folder[i++] = *projectFolder++;
                                        }

                                        // Construct full path and analyze package
                                        char full_path[MAX_PATH];
                                        snprintf(full_path, sizeof(full_path), "%s/%s", root_path, folder);
                                        analyze_package_workspace(full_path, project);

                                        // Store in workspace projects if space available
                                        if (project->workspace.package_count < MAX_PACKAGES) {
                                            Package* pkg = &project->workspace.packages[project->workspace.package_count];
                                            strncpy(pkg->name, name, sizeof(pkg->name) - 1);
                                            strncpy(pkg->path, full_path, sizeof(pkg->path) - 1);
                                            project->workspace.package_count++;
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
                projects++;
            }
        }
    }

    // Parse Rush specific configurations
    project->workspace.is_rush = 1;

    // Parse version policy
    const char* version_policy = strstr(content, "\"versionPolicyName\"");
    if (version_policy) {
        version_policy = strchr(version_policy, ':');
        if (version_policy) {
            version_policy = strchr(version_policy, '"');
            if (version_policy) {
                version_policy++;
                if (strncmp(version_policy, "lock-step", 9) == 0) {
                    strncpy(project->workspace.version_strategy, "fixed",
                            sizeof(project->workspace.version_strategy) - 1);
                } else {
                    strncpy(project->workspace.version_strategy, "independent",
                            sizeof(project->workspace.version_strategy) - 1);
                }
            }
        }
    }

    // Check for common Rush features
    if (strstr(content, "\"rushVersion\"")) {
        // Extract Rush version
        const char* rush_version = strstr(content, "\"rushVersion\"");
        if (rush_version) {
            rush_version = strchr(rush_version, ':');
            if (rush_version) {
                rush_version = strchr(rush_version, '"');
                if (rush_version) {
                    rush_version++;
                    char version[20] = {0};
                    int i = 0;
                    while (*rush_version && *rush_version != '"' && i < 19) {
                        version[i++] = *rush_version++;
                    }
                    // Store Rush version if needed
                }
            }
        }
    }

    // Parse build cache configuration
    if (strstr(content, "\"buildCacheEnabled\"")) {
        const char* cache_folder = strstr(content, "\"cacheFolder\"");
        if (cache_folder) {
            cache_folder = strchr(cache_folder, ':');
            if (cache_folder) {
                cache_folder = strchr(cache_folder, '"');
                if (cache_folder) {
                    cache_folder++;
                    int i = 0;
                    while (*cache_folder && *cache_folder != '"' &&
                           i < sizeof(project->workspace.build_cache_path) - 1) {
                        project->workspace.build_cache_path[i++] = *cache_folder++;
                    }
                    project->workspace.build_cache_path[i] = '\0';
                }
            }
        }
    }

    // Check for common config files
    const char* common_configs[] = {
            "common/config/rush/.pnpmfile.cjs",
            "common/config/rush/command-line.json",
            "common/config/rush/version-policies.json"
    };

    for (size_t i = 0; i < sizeof(common_configs)/sizeof(common_configs[0]); i++) {
        char config_path[MAX_PATH];
        snprintf(config_path, sizeof(config_path), "%s/%s", root_path, common_configs[i]);
        FILE* cf = fopen(config_path, "r");
        if (cf) {
            // Config file exists
            fclose(cf);
            // Handle specific config if needed
        }
    }

    free(content);
}

// Helper function to check file existence
static int file_exists_in_root(const char* root_path, const char* filename) {
    char full_path[MAX_PATH];
    snprintf(full_path, sizeof(full_path), "%s/%s", root_path, filename);

    FILE* f = fopen(full_path, "r");
    if (f) {
        fclose(f);
        return 1;
    }
    return 0;
}

static void parse_lerna_packages(const char* root_path, ProjectType* project) {
    if (!root_path || !project) return;

    char lerna_json_path[MAX_PATH];
    snprintf(lerna_json_path, sizeof(lerna_json_path), "%s/lerna.json", root_path);

    FILE* f = fopen(lerna_json_path, "r");
    if (!f) return;

    // Read lerna.json
    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (fsize <= 0 || fsize > 1024 * 1024) { // 1MB limit
        fclose(f);
        return;
    }

    char* content = (char*)malloc(fsize + 1);
    if (!content) {
        fclose(f);
        return;
    }

    size_t read_size = fread(content, 1, fsize, f);
    fclose(f);

    if (read_size != fsize) {
        free(content);
        return;
    }
    content[fsize] = '\0';

    // Parse Lerna configuration
    project->is_monorepo = 1;
    project->workspace.is_lerna = 1;

    // Parse version management strategy
    const char* version = strstr(content, "\"version\"");
    if (version) {
        version = strchr(version, ':');
        if (version) {
            version = strchr(version, '"');
            if (version) {
                version++; // Skip opening quote
                if (strncmp(version, "independent", 11) == 0) {
                    strncpy(project->workspace.version_strategy, "independent",
                            sizeof(project->workspace.version_strategy) - 1);
                } else {
                    strncpy(project->workspace.version_strategy, "fixed",
                            sizeof(project->workspace.version_strategy) - 1);

                    // Store fixed version if needed
                    char fixed_version[20] = {0};
                    int i = 0;
                    while (*version && *version != '"' && i < sizeof(fixed_version) - 1) {
                        fixed_version[i++] = *version++;
                    }
                }
            }
        }
    }

    // Parse package locations
    const char* packages = strstr(content, "\"packages\"");
    if (packages) {
        packages = strchr(packages, '[');
        if (packages) {
            packages++; // Skip opening bracket

            // Store package globs for scanning
            while (*packages && *packages != ']') {
                if (*packages == '"') {
                    packages++; // Skip opening quote

                    if (project->workspace.workspace_count < MAX_WORKSPACES) {
                        char* glob = project->workspace.workspace_globs[project->workspace.workspace_count];
                        int i = 0;

                        while (*packages && *packages != '"' && i < 99) {
                            glob[i++] = *packages++;
                        }
                        glob[i] = '\0';

                        // Process the glob pattern
                        if (strlen(glob) > 0) {
                            scan_workspace_glob(root_path, glob, project);
                            project->workspace.workspace_count++;
                        }
                    }
                }
                packages++;
            }
        }
    }

    // Parse npm client configuration
    const char* npm_client = strstr(content, "\"npmClient\"");
    if (npm_client) {
        npm_client = strchr(npm_client, ':');
        if (npm_client) {
            npm_client = strchr(npm_client, '"');
            if (npm_client) {
                npm_client++; // Skip opening quote
                if (strncmp(npm_client, "yarn", 4) == 0) {
                    project->workspace.is_yarn_workspace = 1;
                } else if (strncmp(npm_client, "pnpm", 4) == 0) {
                    project->workspace.is_pnpm_workspace = 1;
                }
            }
        }
    }

    // Check for useWorkspaces flag
    if (strstr(content, "\"useWorkspaces\": true")) {
        project->workspace.uses_npm_workspaces = 1;
    }

    // Parse command configuration
    const char* command = strstr(content, "\"command\"");
    if (command) {
        command = strchr(command, '{');
        if (command) {
            // Parse publish configuration
            const char* publish = strstr(command, "\"publish\"");
            if (publish) {
                // Check for message format
                if (strstr(publish, "\"conventionalCommits\": true")) {
                    project->workspace.uses_conventional_commits = 1;
                }

                // Check for version tags
                if (strstr(publish, "\"createRelease\"")) {
                    project->workspace.uses_git_tags = 1;
                }
            }

            // Parse bootstrap configuration
            const char* bootstrap = strstr(command, "\"bootstrap\"");
            if (bootstrap) {
                if (strstr(bootstrap, "\"hoist\": true")) {
                    project->workspace.has_hoisting = 1;
                }
            }
        }
    }

    // Look for additional tooling
    if (file_exists_in_root(root_path, "commitlint.config.js")) {
        project->workspace.uses_conventional_commits = 1;
    }
    if (file_exists_in_root(root_path, ".changeset")) {
        project->workspace.uses_changesets = 1;
    }

    free(content);

    // After parsing all packages, analyze interdependencies
    analyze_package_interdependencies(project);
}

// Helper functions for monorepo detection and analysis
static void detect_workspace_type(const char* root_path, ProjectType* project) {
    char lerna_path[MAX_PATH];
    char pnpm_path[MAX_PATH];
    char rush_path[MAX_PATH];
    char nx_path[MAX_PATH];

    snprintf(lerna_path, MAX_PATH, "%s/lerna.json", root_path);
    snprintf(pnpm_path, MAX_PATH, "%s/pnpm-workspace.yaml", root_path);
    snprintf(rush_path, MAX_PATH, "%s/rush.json", root_path);
    snprintf(nx_path, MAX_PATH, "%s/nx.json", root_path);

    // Check for different workspace types
    FILE* f;
    if ((f = fopen(lerna_path, "r"))) {
        project->workspace.is_lerna = 1;
        project->is_monorepo = 1;
        fclose(f);
    }
    if ((f = fopen(pnpm_path, "r"))) {
        project->workspace.is_pnpm_workspace = 1;
        project->is_monorepo = 1;
        fclose(f);
    }
    if ((f = fopen(rush_path, "r"))) {
        project->workspace.is_rush = 1;
        project->is_monorepo = 1;
        fclose(f);
    }
    if ((f = fopen(nx_path, "r"))) {
        project->workspace.is_nx_workspace = 1;
        project->is_monorepo = 1;
        fclose(f);
    }
}

static void find_workspace_packages(const char* root_path, ProjectType* project) {
    // Handle different workspace types
    if (project->workspace.is_lerna) {
        parse_lerna_packages(root_path, project);
    }
    if (project->workspace.is_pnpm_workspace) {
        parse_pnpm_workspace(root_path, project);
    }
    if (project->workspace.is_yarn_workspace) {
        for (int i = 0; i < project->workspace.workspace_count; i++) {
            scan_workspace_glob(root_path, project->workspace.workspace_globs[i], project);
        }
    }
    if (project->workspace.is_nx_workspace) {
        parse_nx_workspace(root_path, project);
    }
    if (project->workspace.is_rush) {
        parse_rush_config(root_path, project);
    }
}

static void analyze_shared_dependencies(ProjectType* project) {
    // Identify dependencies used by multiple packages
    for (int i = 0; i < project->workspace.package_count; i++) {
        Package* pkg = &project->workspace.packages[i];
        for (int j = 0; j < pkg->dependencies.count; j++) {
            Dependency* dep = &pkg->dependencies.items[j];
            update_shared_dependency_count(dep->name, project);
        }
    }
}

static void add_dependency(ProjectType* project, const char* name, const char* version) {
    if (project->dependencies.count >= MAX_DEPENDENCIES) return;

    // Check if dependency already exists
    for (int i = 0; i < project->dependencies.count; i++) {
        if (strcmp(project->dependencies.items[i].name, name) == 0) {
            return;
        }
    }

    Dependency* dep = &project->dependencies.items[project->dependencies.count];
    strncpy(dep->name, name, sizeof(dep->name) - 1);
    strncpy(dep->version, version, sizeof(dep->version) - 1);
    project->dependencies.count++;
}

static void analyze_package_json(const char* content, ProjectType* project) {
    if (strstr(content, "\"type\": \"module\"")) {
        project->uses_esmodules = 1;
    }

    // Check for framework dependencies
    if (strstr(content, "\"react\"") || strstr(content, "\"react-dom\"")) {
        project->framework_info.has_react = 1;
        add_dependency(project, "react", parse_version(content, "react"));
    }
    if (strstr(content, "\"@angular/core\"")) {
        project->framework_info.has_angular = 1;
        add_dependency(project, "@angular/core", parse_version(content, "@angular/core"));
    }
    if (strstr(content, "\"vue\"")) {
        project->framework_info.has_vue = 1;
        add_dependency(project, "vue", parse_version(content, "vue"));
    }
    if (strstr(content, "\"svelte\"")) {
        project->framework_info.has_svelte = 1;
        add_dependency(project, "svelte", parse_version(content, "svelte"));
    }

    // Check for build tools and transpilers
    if (strstr(content, "\"webpack\"")) {
        project->has_webpack = 1;
        add_dependency(project, "webpack", parse_version(content, "webpack"));
    }
    if (strstr(content, "\"babel\"") || strstr(content, "\"@babel/core\"")) {
        project->has_babel = 1;
    }
    if (strstr(content, "\"typescript\"")) {
        project->has_typescript = 1;
    }

    // Parse all dependencies
    parse_dependencies_section(content, "dependencies", project, 0);
    parse_dependencies_section(content, "devDependencies", project, 1);
}

// Helper function to get all detected frameworks
static void get_framework_names(const ProjectType* project, char* frameworks, size_t size) {
    frameworks[0] = '\0';
    int count = 0;

    if (project->framework_info.has_react) {
        strncat(frameworks, count++ ? ", React" : "React", size - strlen(frameworks) - 1);
        if (project->framework_info.has_nextjs) {
            strncat(frameworks, " (Next.js)", size - strlen(frameworks) - 1);
        }
    }
    if (project->framework_info.has_vue) {
        strncat(frameworks, count++ ? ", Vue.js" : "Vue.js", size - strlen(frameworks) - 1);
        if (project->framework_info.has_nuxtjs) {
            strncat(frameworks, " (Nuxt.js)", size - strlen(frameworks) - 1);
        }
    }
    if (project->framework_info.has_angular) {
        strncat(frameworks, count++ ? ", Angular" : "Angular", size - strlen(frameworks) - 1);
    }
    if (project->framework_info.has_svelte) {
        strncat(frameworks, count++ ? ", Svelte" : "Svelte", size - strlen(frameworks) - 1);
    }

    if (frameworks[0] == '\0') {
        strncpy(frameworks, "No framework detected", size - 1);
    }
}

int is_image_file(const char* filename) {
    const char* image_extensions[] = {".jpg", ".jpeg", ".png", ".gif", ".webp", ".svg", ".bmp", ".ico"};
    int num_extensions = sizeof(image_extensions) / sizeof(image_extensions[0]);

    for (int i = 0; i < num_extensions; i++) {
        if (strstr(filename, image_extensions[i]) != NULL) {
            return 1;
        }
    }
    return 0;
}

EXPORT ProjectType* analyze_project_type(const char* project_path) {
    TRACE("Entering analyze_project_type");
    ProjectType *project = (ProjectType *)calloc(1, sizeof(ProjectType));
    if (!project) {
        fprintf(stderr, "Memory allocation failed for ProjectType\n");
        return NULL;
    }
    TRACE("Memory allocation for analyze_project_type complete");
    // Traverse directory and analyze files
    int result = traverse_directory(project_path, project);
    if (result != 0) {
        fprintf(stderr, "Error traversing directory: %s\n", project_path);
        free(project);
        return NULL;
    }
    TRACE("traverse_directory for analyze_project_type complete");
    // Generate dependency statistics using the cached data
    generate_dependency_statistics(project);
    TRACE("generate_dependency_statistics for analyze_project_type complete");
    // Determine the primary framework based on dependencies and file analysis
    if (project->framework_info.has_react) {
        strncpy(project->framework, "React", sizeof(project->framework) - 1);
    }
    else if (project->framework_info.has_vue) {
        strncpy(project->framework, "Vue.js", sizeof(project->framework) - 1);
    }
    else if (project->framework_info.has_angular) {
        strncpy(project->framework, "Angular", sizeof(project->framework) - 1);
    }
    else if (project->framework_info.has_svelte) {
        strncpy(project->framework, "Svelte", sizeof(project->framework) - 1);
    }
    else if (project->framework_info.has_nodejs) {
        strncpy(project->framework, "Node.js", sizeof(project->framework) - 1);
    }
    project->framework[sizeof(project->framework) - 1] = '\0';

    // Analyze external resources
    analyze_external_resources(project);

    TRACE("Exiting analyze_project_type");
    return project;
}

EXPORT ResourceEstimation estimate_resources(const ProjectType* project) {
    TRACE("Entering estimate_resources");
    ResourceEstimation estimation = {0};

    // Determine if a framework is present
    int has_framework = (strcmp(project->framework, "") != 0);

    // JS Heap Size Estimation
    double base_heap_size = has_framework ? 1000000 : 2000000;
    double html_memory = project->total_html_info.tag_count * 60;
    double css_memory = project->total_css_info.rule_count * 30;
    double js_memory = (project->total_js_info.variable_count * 30) + (project->total_js_info.function_count * 120);
    double json_memory = project->total_json_info.object_count * 12 + project->total_json_info.array_count * 6;
    double image_memory = project->image_file_count * 12000;
    double custom_element_memory = project->custom_element_count * 4000;
    double framework_memory = has_framework ? 600000 : 0;

    estimation.js_heap_size = base_heap_size + html_memory + css_memory + js_memory + json_memory +
                              image_memory + custom_element_memory + framework_memory;

    // Transferred Data Estimation
    estimation.transferred_data = (project->html_file_count * 1200) +
                                  (project->css_file_count * 3000) +
                                  (project->js_file_count * 3000) +
                                  (project->json_file_count * 600) +
                                  (project->image_file_count * 300) +
                                  (project->external_resource_count * 1500) +
                                  (has_framework ? 2000 : 0);

    // Resource Size Estimation
    estimation.resource_size = (project->html_file_count * 4500) +
                               (project->css_file_count * 45000) +
                               (project->js_file_count * 90000) +
                               (project->json_file_count * 1800) +
                               (project->image_file_count * 75000) +
                               (project->external_resource_count * 35000) +
                               (has_framework ? 70000 : 0);

    // DOMContentLoaded Estimation
    estimation.dom_content_loaded = 7 +
                                    (project->total_html_info.tag_count * 0.07) +
                                    (project->total_css_info.rule_count * 0.03) +
                                    (project->total_js_info.function_count * 0.14) +
                                    (project->custom_element_count * 0.4) +
                                    (project->external_resource_count * 0.8) +
                                    (has_framework ? 4 : 0);

    // Largest Contentful Paint Estimation
    estimation.largest_contentful_paint = estimation.dom_content_loaded +
                                          (project->image_file_count * 0.35) +
                                          (project->total_css_info.rule_count * 0.07) +
                                          (project->external_resource_count * 1.7) +
                                          (has_framework ? 8 : 0);

    TRACE("Exiting estimate_resources");
    return estimation;
}

EXPORT double calculate_performance_impact(const ProjectType* project) {
    TRACE("Entering calculate_performance_impact");
    double impact = 0;

    impact += (project->total_js_info.function_count * 0.1);
    impact += (project->total_css_info.rule_count * 0.05);
    impact += (project->total_html_info.tag_count * 0.02);
    impact += (project->custom_element_count * 0.5);
    impact += (project->external_resource_count * 1.0);
    impact += (project->salesforce_metadata_count * 0.2);
    impact += (project->total_json_info.object_count * 0.01);
    impact += (project->total_json_info.array_count * 0.005);
    impact += (project->image_file_count * 0.2);


    // Additional impact for using a framework
    if (strcmp(project->framework, "ZephyrJS") == 0 ||
        strcmp(project->framework, "React") == 0 ||
        strcmp(project->framework, "Vue") == 0 ||
        strcmp(project->framework, "Angular") == 0) {
        impact += 10; // Base impact for using a framework
    }

    // Normalize the impact score
    impact = (impact / 150) * 5; // Scale to a 0-5 range, increased denominator for framework impact
    TRACE("Exiting calculate_performance_impact");
    return impact;
}

EXPORT void display_resource_usage(const ResourceEstimation* estimation) {
    TRACE("Entering display_resource_usage");
    printf("Estimated Resource Usage:\n");
    printf("Total JS Heap Size: %.2f MB (%.0f bytes)\n", estimation->js_heap_size / 1000000.0, (double)estimation->js_heap_size);
    printf("Transferred Data: %.2f KB (%.0f bytes)\n", estimation->transferred_data / 1000.0, (double)estimation->transferred_data);
    printf("Resource Size: %.2f KB (%.0f bytes)\n", estimation->resource_size / 1000.0, (double)estimation->resource_size);
    printf("DOMContentLoaded: %d ms\n", estimation->dom_content_loaded);
    printf("Largest Contentful Paint (LCP): %d ms\n", estimation->largest_contentful_paint);
    TRACE("Exiting display_resource_usage");
}

EXPORT void display_specific_value(const char* value_name, double value, const char* format) {
    TRACE("Entering display_specific_value");

    // Use the provided format to display the value
    if (strcmp(format, "MB") == 0) {
        printf("%s: %.2f MB (%.0f bytes)\n", value_name, value / 1000000.0, value);
    } else if (strcmp(format, "KB") == 0) {
        printf("%s: %.2f KB (%.0f bytes)\n", value_name, value / 1000.0, value);
    } else if (strcmp(format, "ms") == 0) {
        printf("%s: %d ms\n", value_name, (int)value);
    } else {
        // Default format for raw values
        printf("%s: %.2f\n", value_name, value);
    }

    TRACE("Exiting display_specific_value");
}

static void add_module_path(ProjectType* project, const char* path) {
    if (project->module_path_count >= MAX_IMPORT_PATHS) return;

    // Skip node_modules paths
    if (strstr(path, "node_modules")) return;

    // Check if path already exists
    for (int i = 0; i < project->module_path_count; i++) {
        if (strcmp(project->module_paths[i], path) == 0) {
            return;
        }
    }

    strncpy(project->module_paths[project->module_path_count],
            path,
            MAX_PATH - 1);
    project->module_path_count++;
}

static void analyze_js_imports(const char* content, ProjectType* project) {
    const char* ptr = content;

    // Check for CommonJS requires
    while ((ptr = strstr(ptr, "require("))) {
        project->uses_commonjs = 1;
        char module_name[256] = {0};
        extract_module_name(ptr, module_name, sizeof(module_name));
        add_module_path(project, module_name);
        ptr++;
    }

    // Check for ES Module imports
    ptr = content;
    while ((ptr = strstr(ptr, "import "))) {
        project->uses_esmodules = 1;
        char module_name[256] = {0};
        extract_import_path(ptr, module_name, sizeof(module_name));
        add_module_path(project, module_name);
        ptr++;
    }

    // Check for dynamic imports
    ptr = content;
    while ((ptr = strstr(ptr, "import("))) {
        project->uses_esmodules = 1;
        char module_name[256] = {0};
        extract_module_name(ptr, module_name, sizeof(module_name));
        add_module_path(project, module_name);
        ptr++;
    }
}

// Queue operations
void init_queue(DirQueue* q) {
    q->front = 0;
    q->rear = -1;
    q->size = 0;
}

int is_queue_empty(DirQueue* q) {
    return q->size == 0;
}

int is_queue_full(DirQueue* q) {
    return q->size >= MAX_QUEUE;
}

void enqueue(DirQueue* q, const char* path) {
    if (is_queue_full(q)) {
        fprintf(stderr, "Directory queue is full, skipping: %s\n", path);
        return;
    }
    q->rear = (q->rear + 1) % MAX_QUEUE;
    strncpy(q->entries[q->rear].path, path, MAX_PATH - 1);
    q->entries[q->rear].path[MAX_PATH - 1] = '\0';
    q->size++;
}

QueueEntry dequeue(DirQueue* q) {
    QueueEntry entry = {0};
    if (!is_queue_empty(q)) {
        entry = q->entries[q->front];
        q->front = (q->front + 1) % MAX_QUEUE;
        q->size--;
    }
    return entry;
}

static DirectoryStack* create_directory_stack(size_t initial_capacity) {
    DirectoryStack* stack = (DirectoryStack*)malloc(sizeof(DirectoryStack));
    if (!stack) return NULL;

    stack->entries = (DirectoryEntry*)malloc(initial_capacity * sizeof(DirectoryEntry));
    if (!stack->entries) {
        free(stack);
        return NULL;
    }

    stack->capacity = initial_capacity;
    stack->size = 0;
    return stack;
}

static void destroy_directory_stack(DirectoryStack* stack) {
    if (stack) {
        free(stack->entries);
        free(stack);
    }
}

static int push_directory(DirectoryStack* stack, const char* path, int depth) {
    if (stack->size >= stack->capacity) {
        size_t new_capacity = stack->capacity * 2;
        DirectoryEntry* new_entries = (DirectoryEntry*)realloc(stack->entries,
                                                               new_capacity * sizeof(DirectoryEntry));
        if (!new_entries) return 0;

        stack->entries = new_entries;
        stack->capacity = new_capacity;
    }

    if (strlen(path) >= MAX_PATH_LENGTH) return 0;

    strncpy(stack->entries[stack->size].path, path, MAX_PATH_LENGTH - 1);
    stack->entries[stack->size].path[MAX_PATH_LENGTH - 1] = '\0';
    stack->entries[stack->size].depth = depth;
    stack->size++;
    return 1;
}

static DirectoryEntry* pop_directory(DirectoryStack* stack) {
    if (stack->size == 0) return NULL;
    stack->size--;
    return &stack->entries[stack->size];
}

// First, add this at the top of the file, before traverse_directory
static void process_file_content(const char* filename, const char* content, long size, ProjectType* project) {
    if (strstr(filename, "package.json")) {
        analyze_package_json(content, project);
    }
    else if (strstr(filename, ".html") || strstr(filename, ".htm")) {
        project->html_file_count++;
        HTMLInfo info = parse_html(content);
        project->total_html_info.tag_count += info.tag_count;
        project->total_html_info.script_count += info.script_count;
        project->total_html_info.style_count += info.style_count;
        project->total_html_info.link_count += info.link_count;
        project->custom_element_count += info.custom_element_count;
        project->external_resource_count += info.external_resource_count;
        project->framework_info.has_react |= info.is_react;
        project->framework_info.has_vue |= info.is_vue;
        project->framework_info.has_angular |= info.is_angular;
        project->framework_info.has_svelte |= info.is_svelte;
    }
    else if (strstr(filename, ".css")) {
        project->css_file_count++;
        CSSInfo info = parse_css(content);
        project->total_css_info.rule_count += info.rule_count;
        project->total_css_info.selector_count += info.selector_count;
        project->total_css_info.property_count += info.property_count;
    }
    else if (strstr(filename, ".jsx")) {
        project->jsx_file_count++;
        JSXInfo info = parse_jsx(content);
        project->total_jsx_info = info;
        project->framework_info.has_react = 1;
        project->react_component_count += info.custom_component_count;
        project->framework_info.react_hooks_count += info.hook_count;
    }
    else if (strstr(filename, ".ts")) {
        project->ts_file_count++;
        TSInfo info = parse_typescript(content);
        project->total_ts_info = info;
        // Merge framework information
        project->framework_info.has_react |= info.framework.has_react;
        project->framework_info.has_vue |= info.framework.has_vue;
        project->framework_info.has_angular |= info.framework.has_angular;
        project->framework_info.has_svelte |= info.framework.has_svelte;
        project->framework_info.has_nodejs |= info.framework.has_nodejs;
        analyze_js_imports(content, project);
    }
    else if (strstr(filename, ".vue")) {
        project->vue_file_count++;
        VueInfo info = parse_vue(content);
        project->total_vue_info = info;
        project->framework_info.has_vue = 1;
        project->framework_info.vue_composition_api |= info.uses_script_setup;
    }
    else if (strstr(filename, ".js") || strstr(filename, ".mjs")) {
        project->js_file_count++;
        JSInfo info = parse_javascript(content);
        project->total_js_info.function_count += info.function_count;
        project->total_js_info.variable_count += info.variable_count;
        project->total_js_info.react_component_count += info.react_component_count;
        project->react_component_count += info.react_component_count;
        project->total_js_info.vue_instance_count += info.vue_instance_count;
        project->total_js_info.angular_module_count += info.angular_module_count;
        // Merge framework information
        project->framework_info.has_react |= info.framework.has_react;
        project->framework_info.has_vue |= info.framework.has_vue;
        project->framework_info.has_angular |= info.framework.has_angular;
        project->framework_info.has_nodejs |= info.framework.has_nodejs;
        analyze_js_imports(content, project);
    }
    else if (strstr(filename, ".xml") || strstr(filename, ".object")) {
        project->xml_file_count++;
        XMLInfo info = parse_xml(content);
        project->total_xml_info = info;
    }
    else if (strstr(filename, ".json")) {
        project->json_file_count++;
        JSONInfo info = parse_json(content);
        project->total_json_info.object_count += info.object_count;
        project->total_json_info.array_count += info.array_count;
        project->total_json_info.key_count += info.key_count;
        project->total_json_info.max_nesting_level =
                (info.max_nesting_level > project->total_json_info.max_nesting_level) ?
                info.max_nesting_level : project->total_json_info.max_nesting_level;
    }
}

static char* read_file_content(const char* filepath, long* size_out) {
    FILE* f = fopen(filepath, "rb");
    if (!f) return NULL;

    // Get file size
    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (fsize <= 0 || fsize > 10 * 1024 * 1024) {  // 10MB limit
        fclose(f);
        return NULL;
    }

    // Allocate memory
    char* content = malloc(fsize + 1);
    if (!content) {
        fclose(f);
        return NULL;
    }

    // Read file in larger chunks
    char buffer[OPTIMAL_BUFFER_SIZE];
    size_t total_read = 0;
    size_t bytes_read;

    while ((bytes_read = fread(buffer, 1, sizeof(buffer), f)) > 0) {
        if (total_read + bytes_read > fsize) break;
        memcpy(content + total_read, buffer, bytes_read);
        total_read += bytes_read;
    }

    fclose(f);

    if (total_read != fsize) {
        free(content);
        return NULL;
    }

    content[fsize] = '\0';
    if (size_out) *size_out = fsize;
    return content;
}

static FastDirQueue* create_queue(size_t initial_capacity) {
    FastDirQueue* queue = malloc(sizeof(FastDirQueue));
    if (!queue) return NULL;

    queue->paths = malloc(initial_capacity * sizeof(char*));
    if (!queue->paths) {
        free(queue);
        return NULL;
    }

    queue->capacity = initial_capacity;
    queue->size = 0;
    queue->front = 0;
    queue->rear = 0;
    return queue;
}

static void destroy_queue(FastDirQueue* queue) {
    if (!queue) return;
    for (size_t i = 0; i < queue->size; i++) {
        size_t idx = (queue->front + i) % queue->capacity;
        free(queue->paths[idx]);
    }
    free(queue->paths);
    free(queue);
}

static int enqueue_path(FastDirQueue* queue, const char* path) {
    if (queue->size == queue->capacity) {
        size_t new_capacity = queue->capacity * 2;
        char** new_paths = realloc(queue->paths, new_capacity * sizeof(char*));
        if (!new_paths) return 0;

        // Rearrange elements if wrapped around
        if (queue->front > queue->rear) {
            if (queue->front > 0) {
                size_t count = queue->capacity - queue->front;
                memmove(new_paths + new_capacity - count,
                        queue->paths + queue->front,
                        count * sizeof(char*));
                queue->front = new_capacity - count;
            }
        }

        queue->paths = new_paths;
        queue->capacity = new_capacity;
    }

    char* path_copy = strdup(path);
    if (!path_copy) return 0;

    queue->paths[queue->rear] = path_copy;
    queue->rear = (queue->rear + 1) % queue->capacity;
    queue->size++;
    return 1;
}

static char* dequeue_path(FastDirQueue* queue) {
    if (queue->size == 0) return NULL;

    char* path = queue->paths[queue->front];
    queue->front = (queue->front + 1) % queue->capacity;
    queue->size--;
    return path;
}

static void init_file_type_table(void) {
    // Initialize with null entries
    for (int i = 0; i < FILE_TYPE_HASH_SIZE; i++) {
        file_type_table[i].extension = NULL;
        file_type_table[i].type = 0;
    }

    // Add common file extensions
    const char* code_exts[] = {".js", ".jsx", ".ts", ".tsx", ".vue", ".html", ".css"};
    const char* resource_exts[] = {".jpg", ".png", ".gif", ".svg", ".ico"};
    const char* config_exts[] = {".json", ".xml", ".yaml", ".yml"};

    for (size_t i = 0; i < sizeof(code_exts)/sizeof(code_exts[0]); i++) {
        size_t hash = (size_t)code_exts[i][1] % FILE_TYPE_HASH_SIZE;
        file_type_table[hash].extension = code_exts[i];
        file_type_table[hash].type = 1; // Code file
    }

    for (size_t i = 0; i < sizeof(resource_exts)/sizeof(resource_exts[0]); i++) {
        size_t hash = (size_t)resource_exts[i][1] % FILE_TYPE_HASH_SIZE;
        file_type_table[hash].extension = resource_exts[i];
        file_type_table[hash].type = 2; // Resource file
    }

    for (size_t i = 0; i < sizeof(config_exts)/sizeof(config_exts[0]); i++) {
        size_t hash = (size_t)config_exts[i][1] % FILE_TYPE_HASH_SIZE;
        file_type_table[hash].extension = config_exts[i];
        file_type_table[hash].type = 3; // Config file
    }
}

static int get_file_type(const char* filename) {
    const char* ext = strrchr(filename, '.');
    if (!ext) return 0;

    size_t hash = (size_t)ext[1] % FILE_TYPE_HASH_SIZE;
    if (file_type_table[hash].extension && strcmp(ext, file_type_table[hash].extension) == 0) {
        return file_type_table[hash].type;
    }
    return 0;
}

static inline int should_process_directory(const char* name) {
    if (!name || !*name) return 0;
    if (name[0] == '.' && (!name[1] || (name[1] == '.' && !name[2]))) return 0;

    static const char* skip_dirs[] = {
              "node_modules", ".git", ".github","dist", "build",
              "coverage", "bin",".next", ".nuxt", ".cache", "docs", ".vscode", ".idea"
    };

    for (size_t i = 0; i < sizeof(skip_dirs)/sizeof(skip_dirs[0]); i++) {
        if (strcmp(name, skip_dirs[i]) == 0) return 0;
    }
    return 1;
}

static inline int should_process_file(const char* name) {
    static const char* exts[] = {
            ".js", ".jsx", ".ts", ".tsx", ".vue",
            ".html", ".htm", ".css", ".json", ".xml"
    };

    const char* ext = strrchr(name, '.');
    if (!ext) return 0;

    for (size_t i = 0; i < sizeof(exts)/sizeof(exts[0]); i++) {
        if (strcmp(ext, exts[i]) == 0) return 1;
    }
    return 0;
}

EXPORT int traverse_directory(const char* root_path, ProjectType* project) {
    // Initialize directory stack
    DirStack stack = { .top = 0 };
    strncpy(stack.entries[0].path, root_path, MAX_PATH_LENGTH - 1);
    stack.entries[0].depth = 0;
    stack.top = 1;

    // Allocate file reading buffer
    char* file_buffer = malloc(BUFFER_SIZE);
    if (!file_buffer) return -1;

    size_t files_processed = 0;
    size_t dirs_processed = 0;
    clock_t start = clock();
    clock_t last_report = start;

    // Process directories
    while (stack.top > 0) {
        // Pop current directory
        stack.top--;
        DirEntry current = stack.entries[stack.top];

        tinydir_dir dir;
        if (tinydir_open(&dir, current.path) != -1) {
            dirs_processed++;

            while (dir.has_next) {
                tinydir_file file;
                if (tinydir_readfile(&dir, &file) == -1) break;

                if (file.is_dir) {
                    if (should_process_directory(file.name) && stack.top < STACK_SIZE) {
                        strncpy(stack.entries[stack.top].path, file.path, MAX_PATH_LENGTH - 1);
                        stack.entries[stack.top].depth = current.depth + 1;
                        stack.top++;
                    }
                } else {
                    if (should_process_file(file.name)) {
                        FILE* f = fopen(file.path, "rb");
                        if (f) {
                            size_t bytes_read = fread(file_buffer, 1, BUFFER_SIZE - 1, f);
                            if (bytes_read > 0) {
                                file_buffer[bytes_read] = '\0';
                                process_file_content(file.name, file_buffer, bytes_read, project);

                                // Update file type counts
                                if (strstr(file.name, ".html") || strstr(file.name, ".htm")) {
                                    project->html_file_count++;
                                } else if (strstr(file.name, ".css")) {
                                    project->css_file_count++;
                                } else if (strstr(file.name, ".js")) {
                                    project->js_file_count++;
                                } else if (strstr(file.name, ".jsx")) {
                                    project->jsx_file_count++;
                                } else if (strstr(file.name, ".ts") || strstr(file.name, ".tsx")) {
                                    project->ts_file_count++;
                                } else if (strstr(file.name, ".vue")) {
                                    project->vue_file_count++;
                                } else if (strstr(file.name, ".json")) {
                                    project->json_file_count++;
                                }

                                files_processed++;
                            }
                            fclose(f);
                        }
                    } else if (is_image_file(file.name)) {
                        project->image_file_count++;
                        files_processed++;
                    }
                }

                tinydir_next(&dir);
            }
            tinydir_close(&dir);

            // Progress reporting
            clock_t current = clock();
            if ((double)(current - last_report) / CLOCKS_PER_SEC >= 1.0) {
                double elapsed = (double)(current - start) / CLOCKS_PER_SEC;
                printf("\rProcessed: %zu files, %zu dirs (%.1f items/sec)     ",
                       files_processed, dirs_processed,
                       (files_processed + dirs_processed) / elapsed);
                fflush(stdout);
                last_report = current;
            }
        }
    }

    // Final statistics
    double total_time = (double)(clock() - start) / CLOCKS_PER_SEC;
    printf("\n\nTraversal completed:\n");
    printf("- Processed %zu files in %zu directories\n", files_processed, dirs_processed);
    printf("- Total time: %.2f seconds\n", total_time);
    printf("- Average speed: %.1f items/second\n",
           (files_processed + dirs_processed) / total_time);

    free(file_buffer);
    return 0;
}

void process_file(const char* file_path, const char* file_name, ProjectType* project) {
    if (is_image_file(file_name)) {
        project->image_file_count++;
        return;
    }

    FILE* f = fopen(file_path, "rb");
    if (f == NULL) {
        fprintf(stderr, "Error opening file: %s\n", file_path);
        return;
    }

    // Get file size and validate
    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (fsize > 10 * 1024 * 1024) {  // 10 MB limit
        fprintf(stderr, "File too large (>10MB), skipping: %s\n", file_path);
        fclose(f);
        return;
    }

    // Read file content
    char* content = malloc(fsize + 1);
    if (!content) {
        fprintf(stderr, "Memory allocation failed for file: %s\n", file_path);
        fclose(f);
        return;
    }

    size_t read_size = fread(content, 1, fsize, f);
    fclose(f);

    if (read_size != fsize) {
        fprintf(stderr, "Error reading file: %s\n", file_path);
        free(content);
        return;
    }
    content[fsize] = 0;

    // Parse file based on extension
    if (strstr(file_name, ".html") || strstr(file_name, ".htm")) {
        project->html_file_count++;
        HTMLInfo info = parse_html(content);
        project->total_html_info.tag_count += info.tag_count;
        project->total_html_info.script_count += info.script_count;
        project->total_html_info.style_count += info.style_count;
        project->total_html_info.link_count += info.link_count;
        project->custom_element_count += info.custom_element_count;
        project->external_resource_count += info.external_resource_count;
        project->framework_info.has_react |= info.is_react;
        project->framework_info.has_vue |= info.is_vue;
        project->framework_info.has_angular |= info.is_angular;
        project->framework_info.has_svelte |= info.is_svelte;
    }
    else if (strstr(file_name, ".css")) {
        project->css_file_count++;
        CSSInfo info = parse_css(content);
        project->total_css_info.rule_count += info.rule_count;
        project->total_css_info.selector_count += info.selector_count;
        project->total_css_info.property_count += info.property_count;
    }
    else if (strstr(file_name, ".js")) {
        project->js_file_count++;
        JSInfo info = parse_javascript(content);
        project->total_js_info.function_count += info.function_count;
        project->total_js_info.variable_count += info.variable_count;
        project->total_js_info.react_component_count += info.react_component_count;
        project->total_js_info.vue_instance_count += info.vue_instance_count;
        project->total_js_info.angular_module_count += info.angular_module_count;
        project->framework_info.has_react |= (info.react_component_count > 0);
        project->framework_info.has_vue |= (info.vue_instance_count > 0);
        project->framework_info.has_angular |= (info.angular_module_count > 0);
    }
    else if (strstr(file_name, ".ts")) {
        project->ts_file_count++;
        TSInfo info = parse_typescript(content);
        project->total_ts_info = info;
        project->framework_info = info.framework;
    }
    else if (strstr(file_name, ".jsx")) {
        project->jsx_file_count++;
        JSXInfo info = parse_jsx(content);
        project->total_jsx_info = info;
        project->framework_info.has_react = 1;
        project->react_component_count += info.custom_component_count;
    }
    else if (strstr(file_name, ".vue")) {
        project->vue_file_count++;
        VueInfo info = parse_vue(content);
        project->total_vue_info = info;
        project->framework_info.has_vue = 1;
    }
    else if (strstr(file_name, ".xml")) {
        project->xml_file_count++;
        XMLInfo info = parse_xml(content);
        project->total_xml_info = info;
    }
    else if (strstr(file_name, ".json")) {
        project->json_file_count++;
        JSONInfo info = parse_json(content);
        project->total_json_info.object_count += info.object_count;
        project->total_json_info.array_count += info.array_count;
    }

    free(content);
}

EXPORT int analyze_salesforce_metadata(const char* path, ProjectType* project) {
    TRACE("Entering analyze_salesforce_metadata");
    // This is a placeholder function. You'll need to implement the actual
    // Salesforce metadata analysis logic here.
    // For now, we'll just check if the file contains "<CustomObject" as a simple heuristic
    FILE *f = fopen(path, "r");
    if (f == NULL) {
        fprintf(stderr, "Error opening file: %s\n", path);
        return 0;
    }

    char buffer[1024];
    int is_salesforce_metadata = 0;

    while (fgets(buffer, sizeof(buffer), f)) {
        if (strstr(buffer, "<CustomObject") != NULL) {
            is_salesforce_metadata = 1;
            if (project->salesforce_metadata_count < MAX_SALESFORCE_METADATA) {
                strncpy(project->salesforce_metadata[project->salesforce_metadata_count], path, 99);
                project->salesforce_metadata[project->salesforce_metadata_count][99] = '\0';
                project->salesforce_metadata_count++;
            }
            break;
        }
    }

    fclose(f);
    TRACE("Exiting analyze_salesforce_metadata");
    return is_salesforce_metadata;
}

EXPORT void display_potential_issues(const ProjectType* project) {
    TRACE("Entering display_potential_issues");
    printf("\nPotential Issues:\n");
    for (int i = 0; i < project->potential_issue_count; i++) {
        printf("%d. %s\n", i + 1, project->potential_issues[i].description);
        if (project->potential_issues[i].location[0] != '\0') {
            printf("   Location: %s\n", project->potential_issues[i].location);
        }
    }
    TRACE("Exiting display_potential_issues");
}

EXPORT void analyze_external_resources(ProjectType* project) {
    TRACE("Entering analyze_external_resources");
    int js_resources = 0;
    int css_resources = 0;
    size_t total_size = 0;

    for (int i = 0; i < project->external_resource_count; i++) {
        if (strcmp(project->external_resources[i].type, "JS") == 0) {
            js_resources++;
        } else if (strcmp(project->external_resources[i].type, "CSS") == 0) {
            css_resources++;
        }
        total_size += project->external_resources[i].size;
    }

    if (js_resources > 10) {
        snprintf(project->potential_issues[project->potential_issue_count].description, 255,
                 "High number of external JavaScript resources (%d) may impact load time", js_resources);
        project->potential_issue_count++;
    }

    if (css_resources > 5) {
        snprintf(project->potential_issues[project->potential_issue_count].description, 255,
                 "High number of external CSS resources (%d) may impact load time", css_resources);
        project->potential_issue_count++;
    }

    if (total_size > 5000000) { // 5 MB
        snprintf(project->potential_issues[project->potential_issue_count].description, 255,
                 "Large total size of external resources (%.2f MB) may slow down page load",
                 total_size / 1000000.0);
        project->potential_issue_count++;
    }
    TRACE("Exiting analyze_external_resources");
}