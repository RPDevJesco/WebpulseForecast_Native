#ifndef WEB_PARSERS_H
#define WEB_PARSERS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#ifdef _WIN32
#define EXPORT __declspec(dllexport)
#else
#define EXPORT __attribute__((visibility("default")))
#endif

#pragma pack(push, 1)

// Constants for array sizes
#define MAX_CUSTOM_ELEMENTS 50
#define MAX_EXTERNAL_RESOURCES 50
#define MAX_FRAMEWORK_COMPONENTS 50
#define MAX_POTENTIAL_ISSUES 20

// Common structures used across different parsers
typedef struct {
    char description[256];
    char location[100];
} PotentialIssue;

typedef struct {
    char name[50];
    int count;
} CustomElement;

typedef struct {
    char url[256];
    char type[10]; // "JS" or "CSS"
    size_t size;   // Size in bytes
} ExternalResource;

// Framework detection information
typedef struct {
    // Core frameworks
    int has_react;
    int has_vue;
    int has_angular;
    int has_svelte;
    int has_nodejs;

    // Meta frameworks
    int has_nextjs;
    int has_nuxtjs;

    // Framework features
    int react_hooks_count;
    int vue_composition_api;

    // Language and build tools
    int uses_typescript;
    int has_bundler;
    int has_testing;

    // Architecture and state
    int has_state_management;
    int has_routing;

    // UI and styling
    int has_css_framework;
    int has_ui_library;
    int has_form_library;

    // Additional metadata
    char typescript_version[20];
    char node_version[20];
    char primary_bundler[50];    // e.g., "webpack", "vite", etc.
    char primary_ui_library[50]; // e.g., "material-ui", "antd", etc.
    char css_solution[50];       // e.g., "styled-components", "tailwind", etc.

    // Feature flags for detailed analysis
    int uses_css_modules;
    int uses_css_in_js;
    int uses_tailwind;
    int uses_sass;
    int uses_less;

    // Testing and quality
    int has_e2e_testing;        // e.g., Cypress, Playwright
    int has_unit_testing;       // e.g., Jest, Vitest
    int has_component_testing;  // e.g., Testing Library
    int has_linting;           // e.g., ESLint
    int has_formatting;        // e.g., Prettier

    // Build and deployment
    int has_ci_cd;            // e.g., GitHub Actions, Travis
    int has_docker;
    int has_deployment_config; // e.g., Vercel, Netlify

    // Development tools
    int has_hot_reload;
    int has_dev_server;
    int has_debug_config;

    // Package management
    int uses_npm;
    int uses_yarn;
    int uses_pnpm;
} FrameworkInfo;

// HTML Parser information
typedef struct {
    int tag_count;
    int script_count;
    int style_count;
    int link_count;
    int is_zephyr;
    int is_react;
    int is_vue;
    int is_angular;
    int is_svelte;
    CustomElement custom_elements[MAX_CUSTOM_ELEMENTS];
    int custom_element_count;
    ExternalResource external_resources[MAX_EXTERNAL_RESOURCES];
    int external_resource_count;
    char framework_components[MAX_FRAMEWORK_COMPONENTS][50];
    int framework_component_count;
    PotentialIssue potential_issues[MAX_POTENTIAL_ISSUES];
    int potential_issue_count;
} HTMLInfo;

// CSS Parser information
typedef struct {
    int rule_count;
    int selector_count;
    int property_count;
    int media_query_count;
    int keyframe_count;
    PotentialIssue potential_issues[MAX_POTENTIAL_ISSUES];
    int potential_issue_count;
} CSSInfo;

// JavaScript Parser information
typedef struct {
    int function_count;
    int variable_count;
    int class_count;
    int react_component_count;
    int vue_instance_count;
    int angular_module_count;
    int event_listener_count;
    int async_function_count;
    int promise_count;
    int closure_count;
    FrameworkInfo framework;
    PotentialIssue potential_issues[MAX_POTENTIAL_ISSUES];
    int potential_issue_count;
} JSInfo;

// TypeScript Parser information
typedef struct {
    int interface_count;
    int type_definition_count;
    int type_alias_count;
    int generic_type_count;
    int enum_count;
    FrameworkInfo framework;
    PotentialIssue potential_issues[MAX_POTENTIAL_ISSUES];
    int potential_issue_count;
} TSInfo;

// JSX Parser information
typedef struct {
    int custom_component_count;
    int hook_count;
    int prop_spreading_count;
    int max_component_nesting;
    FrameworkInfo framework;
    PotentialIssue potential_issues[MAX_POTENTIAL_ISSUES];
    int potential_issue_count;
} JSXInfo;

// Vue Parser information
typedef struct {
    int has_template;
    int has_script;
    int has_style;
    int uses_script_setup;
    int uses_scoped_styles;
    int directive_count;
    int computed_property_count;
    int watcher_count;
    int event_binding_count;
    int prop_binding_count;
    int emit_count;
    int provide_inject_count;
    FrameworkInfo framework;
    PotentialIssue potential_issues[MAX_POTENTIAL_ISSUES];
    int potential_issue_count;
} VueInfo;

// XML Parser information
typedef struct {
    int element_count;
    int attribute_count;
    int namespace_count;
    int max_nesting_level;
    int has_xml_declaration;
    PotentialIssue potential_issues[MAX_POTENTIAL_ISSUES];
    int potential_issue_count;
} XMLInfo;

// JSON Parser information
typedef struct {
    int object_count;
    int array_count;
    int key_count;
    int max_nesting_level;
    PotentialIssue potential_issues[MAX_POTENTIAL_ISSUES];
    int potential_issue_count;
} JSONInfo;

// Function declarations
EXPORT HTMLInfo parse_html(const char* html_content);
EXPORT CSSInfo parse_css(const char* css_content);
EXPORT JSInfo parse_javascript(const char* js_content);
EXPORT JSONInfo parse_json(const char* json_content);
EXPORT TSInfo parse_typescript(const char* ts_content);
EXPORT JSXInfo parse_jsx(const char* jsx_content);
EXPORT VueInfo parse_vue(const char* vue_content);
EXPORT XMLInfo parse_xml(const char* xml_content);

#pragma pack(pop)

#endif // WEB_PARSERS_H