#include "web_parsers.h"
#include <string.h>
#include <ctype.h>

static void init_framework_info(FrameworkInfo* info) {
    if (!info) return;

    // Initialize all integer fields to 0
    memset(info, 0, sizeof(FrameworkInfo));

    // Initialize string fields to empty strings
    info->typescript_version[0] = '\0';
    info->node_version[0] = '\0';
    info->primary_bundler[0] = '\0';
    info->primary_ui_library[0] = '\0';
    info->css_solution[0] = '\0';
}

int is_valid_url(const char* url) {
    return (strncmp(url, "http://", 7) == 0 || strncmp(url, "https://", 8) == 0);
}

HTMLInfo parse_html(const char* html_content) {
    HTMLInfo info = {0};
    const char* ptr = html_content;
    int tag_count = 0;

    while (*ptr) {
        if (*ptr == '<') {
            tag_count++;
            if (tag_count % 1000 == 0) {
                printf("Processed %d HTML tags\n", tag_count);
            }
            if (strncmp(ptr, "script", 6) == 0) {
                info.script_count++;
                const char* src = strstr(ptr, "src=");
                if (src && (src - ptr) < 50) {
                    src += 5; // Move past src="
                    int i = 0;
                    char url[256] = {0};
                    while (*src && *src != '"' && i < 255) {
                        url[i++] = *src++;
                    }
                    url[i] = '\0';
                    if (is_valid_url(url) && info.external_resource_count < MAX_EXTERNAL_RESOURCES) {
                        strncpy(info.external_resources[info.external_resource_count].url, url, 255);
                        strncpy(info.external_resources[info.external_resource_count].type, "JS", 9);
                        info.external_resource_count++;
                    }
                }
                // Check for specific frameworks
                if (strstr(ptr, "react")) info.is_react = 1;
                if (strstr(ptr, "vue")) info.is_vue = 1;
                if (strstr(ptr, "angular")) info.is_angular = 1;
                if (strstr(ptr, "svelte")) info.is_svelte = 1;
            } else if (strncmp(ptr, "link", 4) == 0) {
                info.link_count++;
                if (strstr(ptr, "stylesheet")) {
                    const char* href = strstr(ptr, "href=");
                    if (href) {
                        href += 6; // Move past href="
                        int i = 0;
                        char url[256] = {0};
                        while (*href && *href != '"' && i < 255) {
                            url[i++] = *href++;
                        }
                        url[i] = '\0';
                        if (is_valid_url(url) && info.external_resource_count < MAX_EXTERNAL_RESOURCES) {
                            strncpy(info.external_resources[info.external_resource_count].url, url, 255);
                            strncpy(info.external_resources[info.external_resource_count].type, "CSS", 9);
                            info.external_resource_count++;
                        }
                    }
                }
            } else if (strncmp(ptr, "style", 5) == 0) {
                info.style_count++;
            } else {
                char tag_name[50] = {0};
                int i = 0;
                while (*ptr && *ptr != ' ' && *ptr != '>' && i < 49) {
                    tag_name[i++] = *ptr++;
                }
                tag_name[i] = '\0';

                if (strchr(tag_name, '-') && tag_name[0] != '/') {
                    int found = 0;
                    for (i = 0; i < info.custom_element_count; i++) {
                        if (strcmp(info.custom_elements[i].name, tag_name) == 0) {
                            info.custom_elements[i].count++;
                            found = 1;
                            break;
                        }
                    }
                    if (!found && info.custom_element_count < MAX_CUSTOM_ELEMENTS) {
                        strncpy(info.custom_elements[info.custom_element_count].name, tag_name, 49);
                        info.custom_elements[info.custom_element_count].count = 1;
                        info.custom_element_count++;
                    }

                    // Check for framework-specific components
                    if (info.framework_component_count < MAX_FRAMEWORK_COMPONENTS) {
                        if (strncmp(tag_name, "zephyr-", 7) == 0) {
                            info.is_zephyr = 1;
                            strncpy(info.framework_components[info.framework_component_count++], tag_name, 49);
                        } else if (strncmp(tag_name, "app-", 4) == 0 || strncmp(tag_name, "ng-", 3) == 0) {
                            info.is_angular = 1;
                            strncpy(info.framework_components[info.framework_component_count++], tag_name, 49);
                        }
                    }
                }
            }
            info.tag_count++;
        }
        ptr++;

        if (info.tag_count % 1000 == 0) {
            printf("Processed %d HTML tags\n", info.tag_count);
        }
    }

    // Check for potential issues
    if (info.script_count > 15) {
        snprintf(info.potential_issues[info.potential_issue_count].description, 255,
                 "High number of script tags (%d) may impact performance", info.script_count);
        info.potential_issue_count++;
    }
    if (info.external_resource_count > 20) {
        snprintf(info.potential_issues[info.potential_issue_count].description, 255,
                 "High number of external resources (%d) may slow down page load", info.external_resource_count);
        info.potential_issue_count++;
    }

    printf("Finished parsing HTML. Total tags: %d\n", info.tag_count);
    return info;
}

CSSInfo parse_css(const char* css_content) {
    CSSInfo info = {0};
    const char* ptr = css_content;

    while (*ptr) {
        if (*ptr == '{') {
            info.rule_count++;
        } else if (*ptr == ':') {
            info.property_count++;
        } else if (*ptr == ',' || *ptr == '{') {
            info.selector_count++;
        } else if (strncmp(ptr, "@media", 6) == 0) {
            info.media_query_count++;
        } else if (strncmp(ptr, "@keyframes", 10) == 0) {
            info.keyframe_count++;
        }
        ptr++;
    }

    // Check for potential issues
    if (info.selector_count > 4000) {
        snprintf(info.potential_issues[info.potential_issue_count].description, 255,
                 "High number of selectors (%d) may cause performance issues", info.selector_count);
        info.potential_issue_count++;
    }
    if (info.media_query_count > 50) {
        snprintf(info.potential_issues[info.potential_issue_count].description, 255,
                 "High number of media queries (%d) may complicate responsive design", info.media_query_count);
        info.potential_issue_count++;
    }

    return info;
}

JSInfo parse_javascript(const char* js_content) {
    JSInfo info = {0};
    const char* ptr = js_content;

    init_framework_info(&info.framework);

    while (*ptr) {
        if (strstr(ptr, "function") == ptr || strstr(ptr, "=>") == ptr) {
            info.function_count++;
        } else if (strstr(ptr, "var ") == ptr || strstr(ptr, "let ") == ptr || strstr(ptr, "const ") == ptr) {
            info.variable_count++;
        } else if (strstr(ptr, "class") == ptr) {
            info.class_count++;
            if (strstr(ptr, "extends React.Component")) {
                info.react_component_count++;
            }
        } else if (strstr(ptr, "React.createElement(") == ptr) {
            info.react_component_count++;
        } else if (strstr(ptr, "new Vue(") == ptr) {
            info.vue_instance_count++;
        } else if (strstr(ptr, "angular.module(") == ptr) {
            info.angular_module_count++;
        } else if (strstr(ptr, "addEventListener(") == ptr) {
            info.event_listener_count++;
        } else if (strstr(ptr, "async ") == ptr) {
            info.async_function_count++;
        } else if (strstr(ptr, "new Promise(") == ptr) {
            info.promise_count++;
        }
        ptr++;
    }

    // Rough estimation of closures (this is not perfect and may have false positives)
    ptr = js_content;
    int nested_functions = 0;
    while (*ptr) {
        if (strstr(ptr, "function") == ptr) {
            nested_functions++;
            if (nested_functions > 1) {
                info.closure_count++;
            }
        } else if (*ptr == '}') {
            if (nested_functions > 0) {
                nested_functions--;
            }
        }
        ptr++;
    }

    // Check for potential issues
    if (info.function_count > 200) {
        snprintf(info.potential_issues[info.potential_issue_count].description, 255,
                 "High number of functions (%d) may indicate overly complex code", info.function_count);
        info.potential_issue_count++;
    }
    if (info.event_listener_count > 50) {
        snprintf(info.potential_issues[info.potential_issue_count].description, 255,
                 "High number of event listeners (%d) may cause memory leaks if not properly managed", info.event_listener_count);
        info.potential_issue_count++;
    }
    if (info.closure_count > 100) {
        snprintf(info.potential_issues[info.potential_issue_count].description, 255,
                 "High number of potential closures (%d) may lead to memory leaks if not handled correctly", info.closure_count);
        info.potential_issue_count++;
    }

    return info;
}

JSONInfo parse_json(const char* json_content) {
    JSONInfo info = {0};
    int depth = 0;
    int in_string = 0;
    char prev_char = '\0';

    for (const char* c = json_content; *c != '\0'; c++) {
        if (*c == '"' && prev_char != '\\') {
            in_string = !in_string;
        }

        if (!in_string) {
            switch (*c) {
                case '{':
                    info.object_count++;
                    depth++;
                    if (depth > info.max_nesting_level) {
                        info.max_nesting_level = depth;
                    }
                    break;
                case '}':
                    depth--;
                    break;
                case '[':
                    info.array_count++;
                    depth++;
                    if (depth > info.max_nesting_level) {
                        info.max_nesting_level = depth;
                    }
                    break;
                case ']':
                    depth--;
                    break;
                case ':':
                    if (depth > 0) {
                        info.key_count++;
                    }
                    break;
            }
        }

        prev_char = *c;
    }

    // Check for potential issues
    if (info.max_nesting_level > 10) {
        snprintf(info.potential_issues[info.potential_issue_count].description, 255,
                 "Deep nesting level (%d) may cause performance issues when parsing", info.max_nesting_level);
        info.potential_issue_count++;
    }
    if (info.object_count + info.array_count > 1000) {
        snprintf(info.potential_issues[info.potential_issue_count].description, 255,
                 "Large number of objects and arrays (%d) may indicate overly complex data structure",
                 info.object_count + info.array_count);
        info.potential_issue_count++;
    }

    return info;
}

// Helper function to check for TypeScript types
static int is_typescript_syntax(const char* content) {
    return (strstr(content, ": string") ||
            strstr(content, ": number") ||
            strstr(content, ": boolean") ||
            strstr(content, ": interface") ||
            strstr(content, ": type"));
}

// Helper function to detect JSX syntax
static int is_jsx_syntax(const char* content) {
    return (strstr(content, "className=") ||
            strstr(content, "render()") ||
            strstr(content, "ReactDOM") ||
            strstr(content, "<>"));  // Fragment syntax
}

// Enhanced framework detection without regex
static void detect_framework(const char* content, FrameworkInfo* framework) {
    // React detection using simple string matching
    if (strstr(content, "import React") ||
        strstr(content, "React.Component") ||
        strstr(content, "useState") ||
        strstr(content, "useEffect")) {
        framework->has_react = 1;

        // Count hooks using direct string searches
        const char* ptr = content;
        while ((ptr = strstr(ptr, "use"))) {
            if (strncmp(ptr, "useState", 8) == 0 ||
                strncmp(ptr, "useEffect", 9) == 0 ||
                strncmp(ptr, "useContext", 10) == 0 ||
                strncmp(ptr, "useReducer", 10) == 0 ||
                strncmp(ptr, "useCallback", 11) == 0 ||
                strncmp(ptr, "useMemo", 7) == 0) {
                framework->react_hooks_count++;
            }
            ptr++;
        }
    }

    // Vue.js detection
    if (strstr(content, "createApp") ||
        strstr(content, "defineComponent") ||
        strstr(content, "setup()") ||
        strstr(content, "<template>")) {
        framework->has_vue = 1;
        framework->vue_composition_api = (strstr(content, "setup()") != NULL);
    }

    // Angular detection using simple string matching
    if (strstr(content, "@Component") ||
        strstr(content, "@Injectable") ||
        strstr(content, "ngOnInit")) {
        framework->has_angular = 1;
    }

    // Svelte detection
    if (strstr(content, "<script context=\"module\">") ||
        strstr(content, "$:") ||
        strstr(content, "export let")) {
        framework->has_svelte = 1;
    }

    // Node.js detection
    if (strstr(content, "require(") ||
        strstr(content, "module.exports") ||
        strstr(content, "process.env")) {
        framework->has_nodejs = 1;
    }
}

TSInfo parse_typescript(const char* ts_content) {
    TSInfo info = {0};
    const char* ptr = ts_content;

    while (*ptr) {
        // Interface detection
        if (strstr(ptr, "interface ") == ptr) {
            info.interface_count++;
            // Count properties within interface
            const char* brace = strchr(ptr, '{');
            if (brace) {
                const char* end_brace = strchr(brace, '}');
                if (end_brace) {
                    const char* prop = brace;
                    while (prop < end_brace) {
                        if (*prop == ':') info.type_definition_count++;
                        prop++;
                    }
                }
            }
        }

        // Type alias detection
        if (strstr(ptr, "type ") == ptr && strchr(ptr, '=')) {
            info.type_alias_count++;
        }

        // Generic type detection
        if (*ptr == '<' && isalpha(*(ptr + 1))) {
            const char* end = strchr(ptr, '>');
            if (end && (end - ptr) < 50) { // Reasonable generic length
                info.generic_type_count++;
            }
        }

        // Enum detection
        if (strstr(ptr, "enum ") == ptr) {
            info.enum_count++;
        }

        ptr++;
    }

    // Detect framework usage in TypeScript
    detect_framework(ts_content, &info.framework);

    // Check for potential issues
    if (info.interface_count > 50) {
        snprintf(info.potential_issues[info.potential_issue_count].description, 255,
                 "High number of interfaces (%d) may indicate over-engineering", info.interface_count);
        info.potential_issue_count++;
    }

    return info;
}

JSXInfo parse_jsx(const char* jsx_content) {
    JSXInfo info = {0};
    const char* ptr = jsx_content;
    int in_component = 0;
    char component_stack[100][50] = {0};
    int stack_depth = 0;

    while (*ptr) {
        // Component detection
        if (*ptr == '<' && isalpha(*(ptr + 1))) {
            char component_name[50] = {0};
            int i = 0;
            ptr++;
            while (isalnum(*ptr) || *ptr == '_') {
                component_name[i++] = *ptr++;
                if (i >= 49) break;
            }

            if (i > 0) {
                // Check if it's a custom component (starts with capital letter)
                if (isupper(component_name[0])) {
                    info.custom_component_count++;
                    strncpy(component_stack[stack_depth++], component_name, 49);
                }

                // Track component nesting
                if (stack_depth > info.max_component_nesting) {
                    info.max_component_nesting = stack_depth;
                }
            }
        }

        // Hook usage detection
        if (strstr(ptr, "useState(") == ptr) info.hook_count++;
        if (strstr(ptr, "useEffect(") == ptr) info.hook_count++;
        if (strstr(ptr, "useContext(") == ptr) info.hook_count++;
        if (strstr(ptr, "useReducer(") == ptr) info.hook_count++;
        if (strstr(ptr, "useCallback(") == ptr) info.hook_count++;
        if (strstr(ptr, "useMemo(") == ptr) info.hook_count++;

        // Prop spreading detection
        if (strstr(ptr, "{...") == ptr) info.prop_spreading_count++;

        ptr++;
    }

    // Detect framework usage
    detect_framework(jsx_content, &info.framework);

    // Check for potential issues
    if (info.max_component_nesting > 5) {
        snprintf(info.potential_issues[info.potential_issue_count].description, 255,
                 "Deep component nesting (depth: %d) may impact performance", info.max_component_nesting);
        info.potential_issue_count++;
    }

    if (info.prop_spreading_count > 10) {
        snprintf(info.potential_issues[info.potential_issue_count].description, 255,
                 "Heavy use of prop spreading (%d occurrences) may make props harder to track",
                 info.prop_spreading_count);
        info.potential_issue_count++;
    }

    return info;
}

VueInfo parse_vue(const char* vue_content) {
    VueInfo info = {0};
    const char* ptr = vue_content;
    int in_template = 0;
    int in_script = 0;
    int in_style = 0;

    while (*ptr) {
        // Section detection
        if (strstr(ptr, "<template>") == ptr) {
            in_template = 1;
            info.has_template = 1;
        } else if (strstr(ptr, "</template>") == ptr) {
            in_template = 0;
        } else if (strstr(ptr, "<script>") == ptr || strstr(ptr, "<script setup>") == ptr) {
            in_script = 1;
            info.has_script = 1;
            if (strstr(ptr, "setup>")) info.uses_script_setup = 1;
        } else if (strstr(ptr, "</script>") == ptr) {
            in_script = 0;
        } else if (strstr(ptr, "<style") == ptr) {
            in_style = 1;
            info.has_style = 1;
            if (strstr(ptr, "scoped>")) info.uses_scoped_styles = 1;
        } else if (strstr(ptr, "</style>") == ptr) {
            in_style = 0;
        }

        // Feature detection in template
        if (in_template) {
            if (strstr(ptr, "v-if") == ptr) info.directive_count++;
            if (strstr(ptr, "v-for") == ptr) info.directive_count++;
            if (strstr(ptr, "v-model") == ptr) info.directive_count++;
            if (strstr(ptr, "@") == ptr || strstr(ptr, "v-on:") == ptr) info.event_binding_count++;
            if (strstr(ptr, ":") == ptr || strstr(ptr, "v-bind:") == ptr) info.prop_binding_count++;
        }

        // Feature detection in script
        if (in_script) {
            if (strstr(ptr, "computed:") == ptr) info.computed_property_count++;
            if (strstr(ptr, "watch:") == ptr) info.watcher_count++;
            if (strstr(ptr, "emit(") == ptr) info.emit_count++;
            if (strstr(ptr, "provide(") == ptr) info.provide_inject_count++;
            if (strstr(ptr, "inject(") == ptr) info.provide_inject_count++;
        }

        ptr++;
    }

    // Detect framework usage
    detect_framework(vue_content, &info.framework);

    // Check for potential issues
    if (info.directive_count > 50) {
        snprintf(info.potential_issues[info.potential_issue_count].description, 255,
                 "High number of directives (%d) may indicate complex template logic", info.directive_count);
        info.potential_issue_count++;
    }

    if (info.watcher_count > 20) {
        snprintf(info.potential_issues[info.potential_issue_count].description, 255,
                 "High number of watchers (%d) may impact performance", info.watcher_count);
        info.potential_issue_count++;
    }

    return info;
}

XMLInfo parse_xml(const char* xml_content) {
    XMLInfo info = {0};
    const char* ptr = xml_content;
    int depth = 0;
    char namespace_stack[100][50] = {0};
    int namespace_depth = 0;

    while (*ptr) {
        if (*ptr == '<' && *(ptr + 1) != '/') {
            depth++;
            if (depth > info.max_nesting_level) {
                info.max_nesting_level = depth;
            }

            // Namespace detection
            const char* ns_end = strchr(ptr, ':');
            if (ns_end && (ns_end - ptr) < 50) {
                char namespace[50] = {0};
                strncpy(namespace, ptr + 1, ns_end - ptr - 1);

                // Check if namespace is already counted
                int found = 0;
                for (int i = 0; i < namespace_depth; i++) {
                    if (strcmp(namespace_stack[i], namespace) == 0) {
                        found = 1;
                        break;
                    }
                }

                if (!found && namespace_depth < 100) {
                    strncpy(namespace_stack[namespace_depth++], namespace, 49);
                    info.namespace_count++;
                }
            }

            info.element_count++;
        } else if (*ptr == '<' && *(ptr + 1) == '/') {
            depth--;
        } else if (*ptr == '<' && *(ptr + 1) == '?' && strstr(ptr, "<?xml") == ptr) {
            info.has_xml_declaration = 1;
        }

        // Attribute counting
        if (*ptr == '=' && *(ptr - 1) != '>' && *(ptr + 1) == '"') {
            info.attribute_count++;
        }

        ptr++;
    }

    // Check for potential issues
    if (info.max_nesting_level > 10) {
        snprintf(info.potential_issues[info.potential_issue_count].description, 255,
                 "Deep XML nesting (depth: %d) may impact readability and processing", info.max_nesting_level);
        info.potential_issue_count++;
    }

    if (info.namespace_count > 5) {
        snprintf(info.potential_issues[info.potential_issue_count].description, 255,
                 "High number of namespaces (%d) may complicate maintenance", info.namespace_count);
        info.potential_issue_count++;
    }

    return info;
}