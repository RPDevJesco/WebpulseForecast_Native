#include <stdio.h>
#include <stdlib.h>
#include "web_resource_analyzer.h"

// Forward declarations for all print functions
static void print_framework_info(const FrameworkInfo* info);
static void print_typescript_info(const TSInfo* info);
static void print_jsx_info(const JSXInfo* info);
static void print_vue_info(const VueInfo* info);
static void print_node_analysis(const ProjectType* project);
static void print_workspace_analysis(const ProjectType* project);
static void print_workspace_analysis_detailed(const ProjectType* project);
static void print_resource_metrics(const ProjectType* project);
static void print_build_tools(const ProjectType* project);

static void print_typescript_info(const TSInfo* info) {
    if (!info) return;

    printf("\nTypeScript Analysis:\n");
    printf("------------------\n");

    // Type System Usage
    printf("Type Definitions:\n");
    printf("- Interfaces: %d\n", info->interface_count);
    printf("- Type Aliases: %d\n", info->type_alias_count);
    printf("- Type Definitions: %d\n", info->type_definition_count);
    printf("- Generic Types: %d\n", info->generic_type_count);
    printf("- Enums: %d\n", info->enum_count);

    // Framework Integration
    if (info->framework.has_react || info->framework.has_vue ||
        info->framework.has_angular || info->framework.has_nodejs) {
        printf("\nFramework Integration:\n");
        if (info->framework.has_react) {
            printf("- React with TypeScript\n");
            if (info->framework.react_hooks_count > 0) {
                printf("  • Typed Hooks Usage: %d\n", info->framework.react_hooks_count);
            }
        }
        if (info->framework.has_vue) {
            printf("- Vue with TypeScript\n");
            if (info->framework.vue_composition_api) {
                printf("  • Using Composition API with TypeScript\n");
            }
        }
        if (info->framework.has_angular) {
            printf("- Angular (TypeScript native)\n");
        }
        if (info->framework.has_nodejs) {
            printf("- Node.js with TypeScript\n");
        }
    }

    // Analysis and Recommendations
    printf("\nType System Analysis:\n");

    // Calculate type coverage metrics
    int total_types = info->interface_count + info->type_alias_count +
                      info->type_definition_count + info->enum_count;

    printf("Total Type Definitions: %d\n", total_types);

    if (total_types > 0) {
        // Provide specific recommendations based on the analysis
        if (info->generic_type_count > total_types / 2) {
            printf("\nNote: High usage of generic types detected\n");
            printf("- Consider reviewing for unnecessary complexity\n");
        }

        if (info->interface_count > 3 * info->type_alias_count) {
            printf("\nNote: Heavy reliance on interfaces\n");
            printf("- Consider using type aliases for simple types\n");
        }
    }

    // Potential Issues
    if (info->potential_issue_count > 0) {
        printf("\nPotential Issues:\n");
        for (int i = 0; i < info->potential_issue_count; i++) {
            printf("- %s\n", info->potential_issues[i].description);
            if (info->potential_issues[i].location[0]) {
                printf("  Location: %s\n", info->potential_issues[i].location);
            }
        }
    }

    // Best Practices Recommendations
    printf("\nBest Practices:\n");
    if (info->interface_count > 0 && info->type_alias_count == 0) {
        printf("- Consider using type aliases for simple types\n");
    }
    if (info->generic_type_count > 20) {
        printf("- Consider simplifying complex generic type usage\n");
    }
    if (total_types > 100) {
        printf("- Consider breaking down type definitions into smaller modules\n");
    }
}

static void print_jsx_info(const JSXInfo* info) {
    if (!info) return;

    printf("\nJSX Analysis:\n");
    printf("-----------\n");
    printf("Custom Components: %d\n", info->custom_component_count);
    printf("React Hooks Used: %d\n", info->hook_count);
    printf("Prop Spreading Found: %d instances\n", info->prop_spreading_count);
    printf("Maximum Component Nesting: %d levels\n", info->max_component_nesting);

    // Add warnings for potential issues
    if (info->max_component_nesting > 4) {
        printf("\nWarning: Deep component nesting detected (>4 levels)\n");
        printf("Consider refactoring to reduce component hierarchy depth\n");
    }

    if (info->prop_spreading_count > 5) {
        printf("\nWarning: Heavy use of prop spreading detected\n");
        printf("Consider explicit prop passing for better maintainability\n");
    }

    // Display hook usage patterns if hooks are used
    if (info->hook_count > 0) {
        printf("\nHook Usage:\n");
        printf("- Total hooks: %d\n", info->hook_count);

        // Add any hook-specific information from your analysis
        // This would require adding fields to JSXInfo to track specific hooks

        if (info->hook_count > 20) {
            printf("\nNote: High number of hooks detected\n");
            printf("Consider breaking down into smaller components\n");
        }
    }
}

static void print_node_analysis(const ProjectType* project) {
    printf("\nNode.js Environment Analysis:\n");
    printf("---------------------------\n");

    // Module System
    printf("Module System: ");
    if (project->uses_commonjs && project->uses_esmodules) {
        printf("Mixed (CommonJS and ES Modules)\n");
    } else if (project->uses_commonjs) {
        printf("CommonJS\n");
    } else if (project->uses_esmodules) {
        printf("ES Modules\n");
    } else {
        printf("Not detected\n");
    }

    // Build Tools
    if (project->has_webpack || project->has_babel || project->has_typescript) {
        printf("\nBuild Tools:\n");
        if (project->has_webpack) printf("- Webpack\n");
        if (project->has_babel) printf("- Babel\n");
        if (project->has_typescript) printf("- TypeScript\n");
    }

    // Dependencies
    if (project->dependencies.count > 0) {
        printf("\nDependencies Analysis:\n");
        printf("Total Dependencies: %d\n", project->dependencies.count);

        // Count and display dev dependencies
        int dev_count = 0;
        for (int i = 0; i < project->dependencies.count; i++) {
            if (project->dependencies.items[i].is_dev_dependency) {
                dev_count++;
            }
        }
        printf("Development Dependencies: %d\n", dev_count);
        printf("Production Dependencies: %d\n", project->dependencies.count - dev_count);

        // Display notable dependencies
        printf("\nNotable Dependencies:\n");
        for (int i = 0; i < project->dependencies.count; i++) {
            const Dependency* dep = &project->dependencies.items[i];
            // Only show significant dependencies
            if (!dep->is_dev_dependency &&
                (strstr(dep->name, "react") == dep->name ||    // Starts with "react"
                 strstr(dep->name, "vue") == dep->name ||      // Starts with "vue"
                 strstr(dep->name, "@angular") == dep->name || // Angular packages
                 strstr(dep->name, "express") == dep->name ||  // Express.js
                 strstr(dep->name, "next") == dep->name ||     // Next.js
                 strstr(dep->name, "nuxt") == dep->name)) {    // Nuxt.js
                printf("- %s@%s\n", dep->name, dep->version);
            }
        }
    }

    // Module Paths
    if (project->module_path_count > 0) {
        printf("\nLocal Modules:\n");
        for (int i = 0; i < project->module_path_count; i++) {
            printf("- %s\n", project->module_paths[i]);
        }
    }

    // Configuration Files
    if (project->workspace.tsconfig_path[0] ||
        project->workspace.babel_config_path[0] ||
        project->workspace.eslint_config_path[0]) {
        printf("\nConfiguration Files:\n");
        if (project->workspace.tsconfig_path[0])
            printf("- TypeScript: %s\n", project->workspace.tsconfig_path);
        if (project->workspace.babel_config_path[0])
            printf("- Babel: %s\n", project->workspace.babel_config_path);
        if (project->workspace.eslint_config_path[0])
            printf("- ESLint: %s\n", project->workspace.eslint_config_path);
    }
}

static void print_build_tools(const ProjectType* project) {
    printf("\nBuild Configuration:\n");
    printf("------------------\n");

    if (project->has_typescript) {
        printf("TypeScript:\n");
        printf("  - Enabled: Yes\n");
        if (project->workspace.tsconfig_path[0]) {
            printf("  - Config: %s\n", project->workspace.tsconfig_path);
        }
    }

    if (project->has_webpack) {
        printf("Webpack:\n");
        printf("  - Enabled: Yes\n");
    }

    if (project->has_babel) {
        printf("Babel:\n");
        printf("  - Enabled: Yes\n");
        if (project->workspace.babel_config_path[0]) {
            printf("  - Config: %s\n", project->workspace.babel_config_path);
        }
    }

    // Module System
    printf("\nModule System:\n");
    if (project->uses_commonjs) {
        printf("  - CommonJS\n");
    }
    if (project->uses_esmodules) {
        printf("  - ES Modules\n");
    }
    if (!project->uses_commonjs && !project->uses_esmodules) {
        printf("  - No module system detected\n");
    }

    // Code Quality Tools
    if (project->workspace.prettier_config_path[0] ||
        project->workspace.eslint_config_path[0]) {
        printf("\nCode Quality Tools:\n");
        if (project->workspace.prettier_config_path[0]) {
            printf("  - Prettier (Config: %s)\n", project->workspace.prettier_config_path);
        }
        if (project->workspace.eslint_config_path[0]) {
            printf("  - ESLint (Config: %s)\n", project->workspace.eslint_config_path);
        }
    }

    // Dependencies Summary
    if (project->dependencies.count > 0) {
        printf("\nDependencies:\n");
        printf("  Total: %d\n", project->dependencies.count);

        // Count development dependencies
        int dev_deps = 0;
        for (int i = 0; i < project->dependencies.count; i++) {
            if (project->dependencies.items[i].is_dev_dependency) {
                dev_deps++;
            }
        }
        printf("  Development: %d\n", dev_deps);
        printf("  Production: %d\n", project->dependencies.count - dev_deps);
    }
}

static void print_framework_info(const FrameworkInfo* info) {
    if (!info) return;

    printf("\nFramework Detection:\n");
    printf("------------------\n");

    int framework_count = 0;

    if (info->has_react) {
        printf("%sReact", framework_count++ ? ", " : "");
        if (info->react_hooks_count > 0) {
            printf(" (Hooks: %d)", info->react_hooks_count);
        }
    }

    if (info->has_vue) {
        printf("%sVue.js", framework_count++ ? ", " : "");
        if (info->vue_composition_api) {
            printf(" (Composition API)");
        }
    }

    if (info->has_angular) {
        printf("%sAngular", framework_count++ ? ", " : "");
    }

    if (info->has_svelte) {
        printf("%sSvelte", framework_count++ ? ", " : "");
    }

    if (info->has_nodejs) {
        printf("%sNode.js", framework_count++ ? ", " : "");
    }

    if (framework_count == 0) {
        printf("No major frameworks detected");
    }
    printf("\n");

    // Additional framework details
    if (info->has_react || info->has_vue || info->has_angular || info->has_svelte) {
        printf("\nFramework Details:\n");

        if (info->has_react) {
            printf("React:\n");
            printf("- Hooks Usage: %d\n", info->react_hooks_count);
        }

        if (info->has_vue) {
            printf("Vue.js:\n");
            printf("- API Style: %s\n",
                   info->vue_composition_api ? "Composition API" : "Options API");
        }

        if (info->has_angular) {
            printf("Angular:\n");
            printf("- Modern Angular (Ivy) detected\n");
        }

        if (info->has_svelte) {
            printf("Svelte:\n");
            printf("- Component-based architecture\n");
        }
    }
}

static void print_resource_metrics(const ProjectType* project) {
    printf("\nResource Metrics:\n");
    printf("---------------\n");

    // Add resource metrics that are relevant to the project type
    if (project->is_monorepo) {
        printf("Total Packages: %d\n", project->workspace.package_count);
        printf("Shared Dependencies: %d\n", project->workspace.shared_dependencies.count);
    }

    printf("Total Components: %d\n",
           project->framework_component_count + project->custom_element_count);
    printf("External Resources: %d\n", project->external_resource_count);
}

static void print_workspace_analysis(const ProjectType* project) {
    if (!project->is_monorepo) {
        return;
    }

    printf("\nMonorepo Analysis:\n");
    printf("=================\n");

    // Workspace Type
    printf("\nWorkspace Configuration:\n");
    printf("Type: ");
    int workspace_types = 0;
    if (project->workspace.is_lerna) {
        printf("%sLerna", workspace_types++ ? ", " : "");
    }
    if (project->workspace.is_yarn_workspace) {
        printf("%sYarn Workspaces", workspace_types++ ? ", " : "");
    }
    if (project->workspace.is_pnpm_workspace) {
        printf("%spnpm Workspaces", workspace_types++ ? ", " : "");
    }
    if (project->workspace.is_nx_workspace) {
        printf("%sNx", workspace_types++ ? ", " : "");
    }
    if (project->workspace.is_rush) {
        printf("%sRush", workspace_types++ ? ", " : "");
    }
    printf("\n");

    // Version Management
    printf("\nVersion Management:\n");
    printf("Strategy: %s\n", project->workspace.version_strategy);
    if (project->workspace.uses_changesets) {
        printf("Using Changesets\n");
    }
    if (project->workspace.uses_semantic_release) {
        printf("Using Semantic Release\n");
    }

    // Package Analysis
    printf("\nPackage Analysis:\n");
    printf("Total Packages: %d\n", project->workspace.package_count);

    // Framework Distribution
    int react_count = 0, vue_count = 0, angular_count = 0, node_count = 0;
    for (int i = 0; i < project->workspace.package_count; i++) {
        const Package* pkg = &project->workspace.packages[i];
        if (pkg->framework_info.has_react) react_count++;
        if (pkg->framework_info.has_vue) vue_count++;
        if (pkg->framework_info.has_angular) angular_count++;
        if (pkg->framework_info.has_nodejs) node_count++;
    }

    if (react_count + vue_count + angular_count + node_count > 0) {
        printf("\nFramework Distribution:\n");
        if (react_count) printf("- React: %d packages\n", react_count);
        if (vue_count) printf("- Vue.js: %d packages\n", vue_count);
        if (angular_count) printf("- Angular: %d packages\n", angular_count);
        if (node_count) printf("- Node.js: %d packages\n", node_count);
    }

    // Dependency Analysis
    printf("\nDependency Analysis:\n");
    printf("Shared Dependencies: %d\n", project->workspace.shared_dependencies.count);

    // Print significant shared dependencies
    if (project->workspace.shared_dependencies.count > 0) {
        printf("\nKey Shared Dependencies:\n");
        for (int i = 0; i < project->workspace.shared_dependencies.count; i++) {
            const Dependency* dep = &project->workspace.shared_dependencies.items[i];
            // Only show significant dependencies
            if (strstr(dep->name, "react") == dep->name ||
                strstr(dep->name, "vue") == dep->name ||
                strstr(dep->name, "@angular") == dep->name ||
                strstr(dep->name, "typescript") == dep->name ||
                strstr(dep->name, "webpack") == dep->name) {
                printf("- %s@%s\n", dep->name, dep->version);
            }
        }
    }

    // Build Configuration
    if (project->workspace.task_group_count > 0) {
        printf("\nBuild Configuration:\n");
        printf("Task Groups: %d\n", project->workspace.task_group_count);
        for (int i = 0; i < project->workspace.task_group_count; i++) {
            const TaskGroup* group = &project->workspace.task_groups[i];
            printf("\n%s (%s):\n", group->name, group->type);
            for (int j = 0; j < group->package_count; j++) {
                printf("  - %s\n", group->packages[j]);
            }
        }
    }

    // Shared Tooling
    printf("\nShared Tooling:\n");
    if (project->workspace.tsconfig_path[0])
        printf("- TypeScript Config: %s\n", project->workspace.tsconfig_path);
    if (project->workspace.eslint_config_path[0])
        printf("- ESLint Config: %s\n", project->workspace.eslint_config_path);
    if (project->workspace.prettier_config_path[0])
        printf("- Prettier Config: %s\n", project->workspace.prettier_config_path);
    if (project->workspace.jest_config_path[0])
        printf("- Jest Config: %s\n", project->workspace.jest_config_path);
}

static void print_workspace_analysis_detailed(const ProjectType* project) {
    if (!project->is_monorepo) {
        return;
    }

    printf("\nDetailed Monorepo Analysis\n");
    printf("========================\n");

    // 1. Workspace Configuration
    printf("\nWorkspace Configuration:\n");
    printf("----------------------\n");
    printf("Type: ");
    if (project->workspace.is_lerna) printf("Lerna ");
    if (project->workspace.is_yarn_workspace) printf("Yarn Workspaces ");
    if (project->workspace.is_pnpm_workspace) printf("pnpm Workspaces ");
    if (project->workspace.is_nx_workspace) printf("Nx ");
    if (project->workspace.is_rush) printf("Rush ");
    printf("\n");

    printf("Version Strategy: %s\n", project->workspace.version_strategy);
    if (project->workspace.uses_changesets) printf("Version Management: Changesets\n");
    if (project->workspace.uses_semantic_release) printf("Version Management: Semantic Release\n");
    if (project->workspace.uses_turborepo) printf("Build System: Turborepo\n");

    // 2. Package Structure
    printf("\nPackage Structure:\n");
    printf("----------------\n");
    for (int i = 0; i < project->workspace.package_count; i++) {
        const Package* pkg = &project->workspace.packages[i];
        printf("\nPackage #%d: %s\n", i + 1, pkg->name);
        printf("  Path: %s\n", pkg->path);
        printf("  Dependencies: %d\n", pkg->dependencies.count);

        // Framework information
        printf("  Technologies: ");
        if (pkg->framework_info.has_react) printf("React ");
        if (pkg->framework_info.has_vue) printf("Vue.js ");
        if (pkg->framework_info.has_angular) printf("Angular ");
        if (pkg->framework_info.has_nodejs) printf("Node.js ");
        printf("\n");

        // Package configuration
        if (pkg->config.uses_typescript) printf("  - Uses TypeScript\n");
        if (pkg->config.uses_eslint) printf("  - Uses ESLint\n");
        if (pkg->config.uses_jest) printf("  - Uses Jest\n");

        // Dependencies on other packages
        if (pkg->config.ref_count > 0) {
            printf("  Internal Dependencies:\n");
            for (int j = 0; j < pkg->config.ref_count; j++) {
                printf("    - %s\n", pkg->config.refs[j].target);
            }
        }

        // Build information
        if (pkg->config.build_output_path[0]) {
            printf("  Build Output: %s\n", pkg->config.build_output_path);
        }

        // Scripts
        if (pkg->config.script_count > 0) {
            printf("  Scripts:\n");
            for (int j = 0; j < pkg->config.script_count; j++) {
                printf("    - %s: %s\n",
                       pkg->config.scripts[j].script_name,
                       pkg->config.scripts[j].command);
            }
        }
    }

    // 3. Dependency Analysis
    printf("\nDependency Analysis:\n");
    printf("------------------\n");
    printf("Shared Dependencies: %d\n", project->workspace.shared_dependencies.count);
    for (int i = 0; i < project->workspace.shared_dependencies.count; i++) {
        printf("  - %s@%s\n",
               project->workspace.shared_dependencies.items[i].name,
               project->workspace.shared_dependencies.items[i].version);
    }

    // 4. Build Configuration
    if (project->workspace.task_group_count > 0) {
        printf("\nBuild Configuration:\n");
        printf("------------------\n");
        for (int i = 0; i < project->workspace.task_group_count; i++) {
            const TaskGroup* group = &project->workspace.task_groups[i];
            printf("\nTask Group: %s (%s)\n", group->name, group->type);
            printf("Packages:\n");
            for (int j = 0; j < group->package_count; j++) {
                printf("  - %s\n", group->packages[j]);
            }
        }
    }

    // 5. Shared Tools and Configs
    printf("\nShared Configuration:\n");
    printf("-------------------\n");
    if (project->workspace.tsconfig_path[0])
        printf("TypeScript: %s\n", project->workspace.tsconfig_path);
    if (project->workspace.eslint_config_path[0])
        printf("ESLint: %s\n", project->workspace.eslint_config_path);
    if (project->workspace.prettier_config_path[0])
        printf("Prettier: %s\n", project->workspace.prettier_config_path);
    if (project->workspace.jest_config_path[0])
        printf("Jest: %s\n", project->workspace.jest_config_path);
    if (project->workspace.babel_config_path[0])
        printf("Babel: %s\n", project->workspace.babel_config_path);
}

static void print_vue_info(const VueInfo* info) {
    if (!info) return;

    printf("\nVue.js Analysis\n");
    printf("==============\n");

    // Component Structure
    printf("\nComponent Structure:\n");
    printf("------------------\n");
    printf("Components with Template: %s\n", info->has_template ? "Yes" : "No");
    printf("Components with Script:   %s%s\n",
           info->has_script ? "Yes" : "No",
           info->uses_script_setup ? " (using <script setup>)" : "");
    printf("Components with Style:    %s%s\n",
           info->has_style ? "Yes" : "No",
           info->uses_scoped_styles ? " (scoped styles)" : "");

    // Component Features
    printf("\nComponent Features:\n");
    printf("-----------------\n");
    printf("Directives Used:       %d\n", info->directive_count);
    printf("Computed Properties:   %d\n", info->computed_property_count);
    printf("Watchers:             %d\n", info->watcher_count);
    printf("Event Bindings:       %d\n", info->event_binding_count);
    printf("Prop Bindings:        %d\n", info->prop_binding_count);
    printf("Emit Calls:           %d\n", info->emit_count);
    printf("Provide/Inject Uses:  %d\n", info->provide_inject_count);

    // Analysis and Recommendations
    printf("\nAnalysis:\n");
    printf("---------\n");

    // Composition API usage
    if (info->uses_script_setup) {
        printf("✓ Using modern Composition API with <script setup>\n");
    } else if (info->framework.vue_composition_api) {
        printf("✓ Using Composition API\n");
    } else {
        printf("! Consider migrating to Composition API for better TypeScript support\n");
    }

    // Component complexity analysis
    int complexity_score =
            info->computed_property_count +
            info->watcher_count +
            (info->event_binding_count / 2) +
            (info->prop_binding_count / 2);

    if (complexity_score > 20) {
        printf("! High component complexity detected (score: %d)\n", complexity_score);
        printf("  Consider breaking down into smaller components\n");
    }

    // Style management
    if (info->has_style && !info->uses_scoped_styles) {
        printf("! Unscoped styles detected - consider using scoped styles to prevent leakage\n");
    }

    // State management analysis
    if (info->provide_inject_count > 5) {
        printf("! Heavy use of Provide/Inject detected (%d uses)\n", info->provide_inject_count);
        printf("  Consider using a state management solution like Pinia or Vuex\n");
    }

    // Props and events analysis
    if (info->prop_binding_count > 15) {
        printf("! High number of props (%d) detected\n", info->prop_binding_count);
        printf("  Consider using a composite object prop\n");
    }

    // Best Practices
    printf("\nBest Practices:\n");
    printf("--------------\n");
    int best_practices_count = 0;

    if (info->uses_script_setup) {
        printf("✓ Using script setup syntax\n");
        best_practices_count++;
    }

    if (info->uses_scoped_styles) {
        printf("✓ Using scoped styles\n");
        best_practices_count++;
    }

    if (info->directive_count > 0 && info->directive_count < 15) {
        printf("✓ Reasonable directive usage\n");
        best_practices_count++;
    }

    if (info->computed_property_count > 0) {
        printf("✓ Using computed properties\n");
        best_practices_count++;
    }

    if (best_practices_count == 0) {
        printf("No best practices detected - consider reviewing Vue.js style guide\n");
    }

    // Performance Considerations
    if (info->watcher_count > 10 ||
        info->computed_property_count > 15 ||
        complexity_score > 25) {
        printf("\nPerformance Considerations:\n");
        printf("-------------------------\n");

        if (info->watcher_count > 10) {
            printf("! High number of watchers may impact performance\n");
            printf("  Consider using computed properties where possible\n");
        }

        if (info->computed_property_count > 15) {
            printf("! Many computed properties detected\n");
            printf("  Ensure they're not doing heavy calculations\n");
        }

        if (complexity_score > 25) {
            printf("! Component complexity may impact performance\n");
            printf("  Consider splitting into smaller components\n");
        }
    }

    // Component Architecture
    if (info->directive_count > 0 ||
        info->computed_property_count > 0 ||
        info->watcher_count > 0) {
        printf("\nComponent Architecture:\n");
        printf("---------------------\n");

        if (info->directive_count > 0) {
            printf("Custom Directives: %d\n", info->directive_count);
        }

        if (info->computed_property_count > 0) {
            printf("Computed Properties: %d\n", info->computed_property_count);
            if (info->computed_property_count > 10) {
                printf("  Consider breaking down complex computed properties\n");
            }
        }

        if (info->watcher_count > 0) {
            printf("Watchers: %d\n", info->watcher_count);
            if (info->watcher_count > 8) {
                printf("  High number of watchers - review for optimization\n");
            }
        }
    }
}

int main(int argc, char* argv[]) {
    const char* project_path = argc > 1 ? argv[1] : "E:\\Software Dev\\Others CodeBases\\codeclimberscli";

    printf("Analyzing project: %s\n\n", project_path);

    // Analyze the project type
    ProjectType* project = analyze_project_type(project_path);
    if (project == NULL) {
        fprintf(stderr, "Failed to analyze project type.\n");
        return EXIT_FAILURE;
    }

    // Project Overview
    printf("Project Analysis Summary\n");
    printf("=======================\n");
    printf("Primary Framework: %s\n", project->framework);

    // File Statistics
    printf("\nFile Statistics:\n");
    printf("---------------\n");
    printf("HTML Files: %d\n", project->html_file_count);
    printf("CSS Files: %d\n", project->css_file_count);
    printf("JavaScript Files: %d\n", project->js_file_count);
    printf("TypeScript Files: %d\n", project->ts_file_count);
    printf("JSX Files: %d\n", project->jsx_file_count);
    printf("Vue Files: %d\n", project->vue_file_count);
    printf("XML Files: %d\n", project->xml_file_count);
    printf("JSON Files: %d\n", project->json_file_count);
    printf("Image Files: %d\n", project->image_file_count);

    // Framework Detection
    print_framework_info(&project->framework_info);

    // Node.js and Build Tools Analysis
    if (project->uses_commonjs || project->uses_esmodules ||
        project->has_webpack || project->has_babel || project->has_typescript) {
        print_node_analysis(project);
        print_build_tools(project);
    }

    // Language-specific Analysis
    if (project->ts_file_count > 0) {
        print_typescript_info(&project->total_ts_info);
    }

    if (project->jsx_file_count > 0) {
        print_jsx_info(&project->total_jsx_info);
    }

    if (project->vue_file_count > 0) {
        print_vue_info(&project->total_vue_info);
    }

    // Monorepo Analysis
    if (project->is_monorepo) {
        print_workspace_analysis(project);
        print_workspace_analysis_detailed(project);
    }

    // Components and Resources
    printf("\nComponent Analysis:\n");
    printf("-----------------\n");
    if (project->framework_component_count > 0) {
        printf("Framework Components:\n");
        for (int i = 0; i < project->framework_component_count; i++) {
            printf("  - %s\n", project->framework_components[i]);
        }
    }

    if (project->custom_element_count > 0) {
        printf("\nCustom Elements:\n");
        for (int i = 0; i < project->custom_element_count; i++) {
            printf("  - %s (used %d times)\n",
                   project->custom_elements[i].name,
                   project->custom_elements[i].count);
        }
    }

    if (project->external_resource_count > 0) {
        printf("\nExternal Resources:\n");
        for (int i = 0; i < project->external_resource_count; i++) {
            printf("  - %s: %s\n  Size: %zu bytes\n",
                   project->external_resources[i].type,
                   project->external_resources[i].url,
                   project->external_resources[i].size);
        }
    }

    // JSON Analysis
    if (project->json_file_count > 0) {
        printf("\nJSON Analysis:\n");
        printf("-------------\n");
        printf("Total Objects: %d\n", project->total_json_info.object_count);
        printf("Total Arrays: %d\n", project->total_json_info.array_count);
        printf("Total Keys: %d\n", project->total_json_info.key_count);
        printf("Maximum Nesting Level: %d\n", project->total_json_info.max_nesting_level);
    }

    // Performance Metrics
    printf("\nPerformance Analysis\n");
    printf("===================\n");

    // Resource estimation
    ResourceEstimation estimation = estimate_resources(project);
    display_resource_usage(&estimation);

    // Performance impact score
    double performance_impact = calculate_performance_impact(project);
    printf("\nPerformance Impact Score: %.2f out of 5.0\n", performance_impact);
    if (performance_impact >= 4.0) {
        printf("Warning: High performance impact detected\n");
    }

    // Display potential issues
    printf("\nPotential Issues\n");
    printf("===============\n");
    display_potential_issues(project);

    // Cleanup
    free(project);

    printf("\nPress Enter to exit...");
    getchar();

    return 0;
}