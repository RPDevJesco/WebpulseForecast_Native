#ifndef WEB_RESOURCE_ANALYZER_H
#define WEB_RESOURCE_ANALYZER_H

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <sys/types.h>
#include <fcntl.h>
#include "web_parsers.h"
#include "tinydir.h"

#ifdef _WIN32
#define EXPORT __declspec(dllexport)
#include <windows.h>
#else
#define EXPORT __attribute__((visibility("default")))
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#pragma pack(push, 1)

#define MAX_POTENTIAL_ISSUES 100
#define MAX_SALESFORCE_METADATA 9999
#define MAX_PATH 1024
#define MAX_PATH_LENGTH 260
#define MAX_QUEUE 1000
#define MAX_DIRECTORY_DEPTH 50
#define IRRELEVANT_DIR_COUNT 8
#define MAX_DEPENDENCIES 1000
#define MAX_IMPORT_PATHS 50
#define MAX_WORKSPACES 50
#define MAX_PACKAGES 100
#define OPTIMAL_BUFFER_SIZE (64 * 1024)
#define MAX_CACHED_DEPS 1000
#define FILE_TYPE_HASH_SIZE 64
#define BATCH_SIZE 32
#define HASH_MULTIPLIER 31
#define DIR_STACK_SIZE 1024
#define PATH_BUFFER_SIZE 4096
#define MAX_PATH_LENGTH 260
#define STACK_SIZE 1024
#define BUFFER_SIZE (32 * 1024)


#define TRACE(fmt, ...) do { \
    time_t now = time(NULL); \
    char timestr[20]; \
    strftime(timestr, sizeof(timestr), "%Y-%m-%d %H:%M:%S", localtime(&now)); \
    printf("[%s] ", timestr); \
    printf(fmt, ##__VA_ARGS__); \
    printf("\n"); \
} while(0)

typedef struct {
    char path[MAX_PATH_LENGTH];
    int depth;
} DirEntry;

typedef struct {
    DirEntry entries[STACK_SIZE];
    int top;
} DirStack;

typedef struct {
    DirEntry* entries;
    size_t capacity;
    size_t size;
    size_t processed;
} BatchQueue;

typedef struct {
    char name[100];
    char version[20];
    int count;
} CachedDependency;

typedef struct {
    CachedDependency items[MAX_CACHED_DEPS];
    int count;
} DependencyCache;

typedef struct {
    char name[100];
    char version[20];
    int is_dev_dependency;
} Dependency;

typedef struct {
    Dependency items[MAX_DEPENDENCIES];
    int count;
} DependencyList;

typedef struct {
    char source[100];
    char target[100];
} PackageReference;

typedef struct {
    char script_name[50];
    char command[256];
} PackageScript;

typedef struct {
    PackageReference refs[MAX_PACKAGES];
    int ref_count;
    PackageScript scripts[50];
    int script_count;
    char build_output_path[MAX_PATH];
    char test_output_path[MAX_PATH];
    int has_shared_configs;
    int uses_typescript;
    int uses_eslint;
    int uses_prettier;
    int uses_jest;
    char node_version[20];
} PackageConfig;

typedef struct {
    char name[100];
    char version[20];
    char path[MAX_PATH];
    DependencyList dependencies;
    FrameworkInfo framework_info;
    PackageConfig config;
} Package;

typedef struct {
    char name[100];
    char type[50];  // e.g., "build", "test", "lint"
    char packages[MAX_PACKAGES][100];
    int package_count;
} TaskGroup;

typedef struct {
    char root_path[MAX_PATH];
    char name[100];
    Package packages[MAX_PACKAGES];
    int package_count;
    DependencyList shared_dependencies;
    char workspace_globs[MAX_WORKSPACES][100];
    int workspace_count;

    // Workspace types
    int is_lerna;
    int is_yarn_workspace;
    int is_pnpm_workspace;
    int is_nx_workspace;
    int is_rush;

    // Workspace features
    int has_hoisting;
    int has_workspaces_prefix;
    int uses_npm_workspaces;
    int uses_changesets;
    int uses_turborepo;
    int has_shared_configs;
    int uses_conventional_commits;
    int uses_git_tags;

    // Build configuration
    TaskGroup task_groups[20];
    int task_group_count;
    char build_cache_path[MAX_PATH];

    // Shared configurations
    char tsconfig_path[MAX_PATH];
    char eslint_config_path[MAX_PATH];
    char prettier_config_path[MAX_PATH];
    char jest_config_path[MAX_PATH];
    char babel_config_path[MAX_PATH];

    // Version management
    char version_strategy[50]; // "fixed", "independent", or "synchronized"
    int uses_semantic_release;
} WorkspaceInfo;

typedef struct {
    char path[MAX_PATH];
} QueueEntry;

typedef struct {
    char** paths;
    size_t capacity;
    size_t size;
    size_t front;
    size_t rear;
} FastDirQueue;

typedef struct {
    const char* extension;
    int type;
} FileTypeEntry;

// Project analysis
typedef struct {
    // Framework detection
    FrameworkInfo framework_info;
    char framework[50];
    char required_libraries[1000];

    int total_dependencies;
    int dev_dependencies;
    int prod_dependencies;
    int framework_dependencies;

    // File counts
    int html_file_count;
    int css_file_count;
    int js_file_count;
    int json_file_count;
    int ts_file_count;
    int jsx_file_count;
    int vue_file_count;
    int xml_file_count;
    int image_file_count;

    // Component tracking
    int react_component_count;
    CustomElement custom_elements[MAX_CUSTOM_ELEMENTS];
    int custom_element_count;
    ExternalResource external_resources[MAX_EXTERNAL_RESOURCES];
    int external_resource_count;
    char framework_components[MAX_FRAMEWORK_COMPONENTS][50];
    int framework_component_count;

    // Parser results
    HTMLInfo total_html_info;
    CSSInfo total_css_info;
    JSInfo total_js_info;
    JSONInfo total_json_info;
    TSInfo total_ts_info;
    JSXInfo total_jsx_info;
    VueInfo total_vue_info;
    XMLInfo total_xml_info;

    // Salesforce specific
    char salesforce_metadata[MAX_SALESFORCE_METADATA][100];
    int salesforce_metadata_count;

    // Issues tracking
    PotentialIssue potential_issues[MAX_POTENTIAL_ISSUES];
    int potential_issue_count;

    DependencyList dependencies;
    char module_paths[MAX_IMPORT_PATHS][MAX_PATH];
    int module_path_count;
    int uses_commonjs;
    int uses_esmodules;
    int has_webpack;
    int has_vite;
    int has_babel;
    int has_ci;
    int has_env_config;
    int has_typescript;
    WorkspaceInfo workspace;
    int is_monorepo;
} ProjectType;

// Resource estimation
typedef struct {
    size_t js_heap_size;             // In bytes
    size_t transferred_data;         // In bytes
    size_t resource_size;            // In bytes
    int dom_content_loaded;          // In milliseconds
    int largest_contentful_paint;    // In milliseconds
} ResourceEstimation;

typedef struct DirQueue {
    QueueEntry entries[MAX_QUEUE];
    int front;
    int rear;
    int size;
} DirQueue;

typedef struct {
    char path[MAX_PATH_LENGTH];
    int depth;
} DirectoryEntry;

typedef struct {
    DirectoryEntry* entries;
    size_t capacity;
    size_t size;
} DirectoryStack;

#pragma pack(pop)

// Main analysis functions
EXPORT ProjectType* analyze_project_type(const char* project_path);
EXPORT ResourceEstimation estimate_resources(const ProjectType* project);
EXPORT double calculate_performance_impact(const ProjectType* project);

// Display functions
EXPORT void display_resource_usage(const ResourceEstimation* estimation);
EXPORT void display_potential_issues(const ProjectType* project);
EXPORT void display_specific_value(const char* value_name, double value, const char* format);

// File handling functions
EXPORT int traverse_directory(const char* path, ProjectType* project);
EXPORT int analyze_salesforce_metadata(const char* path, ProjectType* project);
EXPORT void analyze_external_resources(ProjectType* project);

// Helper function declarations
static void process_file(const char* file_path, const char* file_name, ProjectType* project);
static int should_ignore_directory(const char* name);
static int is_image_file(const char* filename);
static void parse_json_array(const char* json, char globs[][100], int* count, int max_count);
static void parse_turbo_pipeline(const char* pipeline_json, ProjectType* project);
static void parse_global_deps(const char* deps_json, ProjectType* project);
static void parse_package_info(const char* content, Package* pkg);
static void analyze_package_dependencies(const char* content, Package* pkg);
static void detect_framework_usage(const char* content, FrameworkInfo* framework);
static void parse_dependencies_section(const char* content, const char* section_name, DependencyList* deps, int is_dev);
static char* parse_version(const char* content, const char* package_name);
static void update_shared_dependency_count(const char* name, ProjectType* project);
static void parse_lerna_packages(const char* root_path, ProjectType* project);
static void parse_nx_workspace(const char* root_path, ProjectType* project);
static void parse_rush_config(const char* root_path, ProjectType* project);
static void scan_workspace_glob(const char* root_path, const char* glob_pattern, ProjectType* project);
static void extract_module_name(const char* ptr, char* module_name, size_t size);
static void extract_import_path(const char* ptr, char* module_name, size_t size);
static void print_package_frameworks(const FrameworkInfo* framework_info);

static DependencyCache dep_cache = {0};
static FileTypeEntry file_type_table[FILE_TYPE_HASH_SIZE] = {0};
// Cross-platform high precision timer
static double get_time_seconds(void) {
#ifdef _WIN32
    LARGE_INTEGER freq, counter;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&counter);
    return (double)counter.QuadPart / (double)freq.QuadPart;
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + (ts.tv_nsec / 1.0e9);
#endif
}
#endif // WEB_RESOURCE_ANALYZER_H