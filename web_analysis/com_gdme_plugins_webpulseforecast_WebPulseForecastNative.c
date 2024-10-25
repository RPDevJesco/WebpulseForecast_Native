#include "com_gdme_plugins_webpulseforecast_WebPulseForecastNative.h"
#include "web_resource_analyzer.h"
#include <string.h>
#include <stdio.h>

// Helper function to extract dependency list (complete implementation)
static void extract_dependency_list(JNIEnv *env, jobject jProjectType, DependencyList *cDependencyList) {
    TRACE("Entering extract_dependency_list");
    jclass cls = (*env)->GetObjectClass(env, jProjectType);

    jfieldID depsField = (*env)->GetFieldID(env, cls, "dependencies",
                                            "Lcom/gdme/webpulseforecast/WebPulseForecastNative$DependencyList;");
    jobject dependencyList = (*env)->GetObjectField(env, jProjectType, depsField);

    if (!dependencyList) {
        TRACE("No dependency list found");
        return;
    }

    jclass depListCls = (*env)->GetObjectClass(env, dependencyList);

    // Get count
    jfieldID countField = (*env)->GetFieldID(env, depListCls, "count", "I");
    cDependencyList->count = (*env)->GetIntField(env, dependencyList, countField);

    // Get items array
    jfieldID itemsField = (*env)->GetFieldID(env, depListCls, "items",
                                             "[Lcom/gdme/webpulseforecast/WebPulseForecastNative$Dependency;");
    jobjectArray items = (*env)->GetObjectField(env, dependencyList, itemsField);

    if (items) {
        jsize length = (*env)->GetArrayLength(env, items);
        int count = (length > MAX_DEPENDENCIES) ? MAX_DEPENDENCIES : length;

        for (int i = 0; i < count; i++) {
            jobject dep = (*env)->GetObjectArrayElement(env, items, i);
            if (!dep) continue;

            jclass depCls = (*env)->GetObjectClass(env, dep);

            // Get name
            jfieldID nameField = (*env)->GetFieldID(env, depCls, "name", "Ljava/lang/String;");
            jstring name = (*env)->GetObjectField(env, dep, nameField);
            if (name) {
                const char* cName = (*env)->GetStringUTFChars(env, name, NULL);
                strncpy(cDependencyList->items[i].name, cName, sizeof(cDependencyList->items[i].name) - 1);
                (*env)->ReleaseStringUTFChars(env, name, cName);
                (*env)->DeleteLocalRef(env, name);
            }

            // Get version
            jfieldID versionField = (*env)->GetFieldID(env, depCls, "version", "Ljava/lang/String;");
            jstring version = (*env)->GetObjectField(env, dep, versionField);
            if (version) {
                const char* cVersion = (*env)->GetStringUTFChars(env, version, NULL);
                strncpy(cDependencyList->items[i].version, cVersion, sizeof(cDependencyList->items[i].version) - 1);
                (*env)->ReleaseStringUTFChars(env, version, cVersion);
                (*env)->DeleteLocalRef(env, version);
            }

            // Get isDevDependency
            jfieldID isDevField = (*env)->GetFieldID(env, depCls, "isDevDependency", "Z");
            cDependencyList->items[i].is_dev_dependency = (*env)->GetBooleanField(env, dep, isDevField);

            (*env)->DeleteLocalRef(env, dep);
        }
        (*env)->DeleteLocalRef(env, items);
    }
    (*env)->DeleteLocalRef(env, dependencyList);
    TRACE("Exiting extract_dependency_list");
}

// Helper function to extract FrameworkInfo from Java object
void extract_framework_info(JNIEnv *env, jobject jFrameworkInfo, FrameworkInfo *cFrameworkInfo) {
    jclass cls = (*env)->GetObjectClass(env, jFrameworkInfo);

    // Core frameworks
    jfieldID field;
    field = (*env)->GetFieldID(env, cls, "hasReact", "Z");
    cFrameworkInfo->has_react = (*env)->GetBooleanField(env, jFrameworkInfo, field);

    field = (*env)->GetFieldID(env, cls, "hasVue", "Z");
    cFrameworkInfo->has_vue = (*env)->GetBooleanField(env, jFrameworkInfo, field);

    field = (*env)->GetFieldID(env, cls, "hasAngular", "Z");
    cFrameworkInfo->has_angular = (*env)->GetBooleanField(env, jFrameworkInfo, field);

    field = (*env)->GetFieldID(env, cls, "hasSvelte", "Z");
    cFrameworkInfo->has_svelte = (*env)->GetBooleanField(env, jFrameworkInfo, field);

    field = (*env)->GetFieldID(env, cls, "hasNodejs", "Z");
    cFrameworkInfo->has_nodejs = (*env)->GetBooleanField(env, jFrameworkInfo, field);

    // Meta frameworks
    field = (*env)->GetFieldID(env, cls, "hasNextjs", "Z");
    cFrameworkInfo->has_nextjs = (*env)->GetBooleanField(env, jFrameworkInfo, field);

    field = (*env)->GetFieldID(env, cls, "hasNuxtjs", "Z");
    cFrameworkInfo->has_nuxtjs = (*env)->GetBooleanField(env, jFrameworkInfo, field);

    // Framework features
    field = (*env)->GetFieldID(env, cls, "reactHooksCount", "I");
    cFrameworkInfo->react_hooks_count = (*env)->GetIntField(env, jFrameworkInfo, field);

    field = (*env)->GetFieldID(env, cls, "vueCompositionApi", "Z");
    cFrameworkInfo->vue_composition_api = (*env)->GetBooleanField(env, jFrameworkInfo, field);

    // Extract version strings
    field = (*env)->GetFieldID(env, cls, "typescriptVersion", "Ljava/lang/String;");
    jstring tsVersion = (jstring)(*env)->GetObjectField(env, jFrameworkInfo, field);
    if (tsVersion) {
        const char* cTsVersion = (*env)->GetStringUTFChars(env, tsVersion, NULL);
        strncpy(cFrameworkInfo->typescript_version, cTsVersion, sizeof(cFrameworkInfo->typescript_version) - 1);
        (*env)->ReleaseStringUTFChars(env, tsVersion, cTsVersion);
    }
}

static void extract_package_config(JNIEnv *env, jobject config, PackageConfig *cConfig) {
    TRACE("Entering extract_package_config");
    if (!config || !cConfig) return;

    jclass cls = (*env)->GetObjectClass(env, config);

    // Extract build paths
    jfieldID field = (*env)->GetFieldID(env, cls, "buildOutputPath", "Ljava/lang/String;");
    jstring buildPath = (*env)->GetObjectField(env, config, field);
    if (buildPath) {
        const char* cBuildPath = (*env)->GetStringUTFChars(env, buildPath, NULL);
        strncpy(cConfig->build_output_path, cBuildPath, MAX_PATH - 1);
        (*env)->ReleaseStringUTFChars(env, buildPath, cBuildPath);
        (*env)->DeleteLocalRef(env, buildPath);
    }

    // Extract test output path
    field = (*env)->GetFieldID(env, cls, "testOutputPath", "Ljava/lang/String;");
    jstring testPath = (*env)->GetObjectField(env, config, field);
    if (testPath) {
        const char* cTestPath = (*env)->GetStringUTFChars(env, testPath, NULL);
        strncpy(cConfig->test_output_path, cTestPath, MAX_PATH - 1);
        (*env)->ReleaseStringUTFChars(env, testPath, cTestPath);
        (*env)->DeleteLocalRef(env, testPath);
    }

    // Extract node version
    field = (*env)->GetFieldID(env, cls, "nodeVersion", "Ljava/lang/String;");
    jstring nodeVersion = (*env)->GetObjectField(env, config, field);
    if (nodeVersion) {
        const char* cNodeVersion = (*env)->GetStringUTFChars(env, nodeVersion, NULL);
        strncpy(cConfig->node_version, cNodeVersion, sizeof(cConfig->node_version) - 1);
        (*env)->ReleaseStringUTFChars(env, nodeVersion, cNodeVersion);
        (*env)->DeleteLocalRef(env, nodeVersion);
    }

    // Extract flags
    field = (*env)->GetFieldID(env, cls, "hasSharedConfigs", "Z");
    cConfig->has_shared_configs = (*env)->GetBooleanField(env, config, field);

    field = (*env)->GetFieldID(env, cls, "usesTypescript", "Z");
    cConfig->uses_typescript = (*env)->GetBooleanField(env, config, field);

    field = (*env)->GetFieldID(env, cls, "usesEslint", "Z");
    cConfig->uses_eslint = (*env)->GetBooleanField(env, config, field);

    field = (*env)->GetFieldID(env, cls, "usesPrettier", "Z");
    cConfig->uses_prettier = (*env)->GetBooleanField(env, config, field);

    field = (*env)->GetFieldID(env, cls, "usesJest", "Z");
    cConfig->uses_jest = (*env)->GetBooleanField(env, config, field);

    // Extract package references
    field = (*env)->GetFieldID(env, cls, "refCount", "I");
    cConfig->ref_count = (*env)->GetIntField(env, config, field);

    field = (*env)->GetFieldID(env, cls, "refs", "[Lcom/gdme/webpulseforecast/WebPulseForecastNative$PackageReference;");
    jobjectArray refs = (*env)->GetObjectField(env, config, field);
    if (refs) {
        jsize length = (*env)->GetArrayLength(env, refs);
        int count = (length > MAX_PACKAGES) ? MAX_PACKAGES : length;

        for (int i = 0; i < count; i++) {
            jobject ref = (*env)->GetObjectArrayElement(env, refs, i);
            if (ref) {
                jclass refCls = (*env)->GetObjectClass(env, ref);

                // Get source
                jfieldID sourceField = (*env)->GetFieldID(env, refCls, "source", "Ljava/lang/String;");
                jstring source = (*env)->GetObjectField(env, ref, sourceField);
                if (source) {
                    const char* cSource = (*env)->GetStringUTFChars(env, source, NULL);
                    strncpy(cConfig->refs[i].source, cSource, sizeof(cConfig->refs[i].source) - 1);
                    (*env)->ReleaseStringUTFChars(env, source, cSource);
                    (*env)->DeleteLocalRef(env, source);
                }

                // Get target
                jfieldID targetField = (*env)->GetFieldID(env, refCls, "target", "Ljava/lang/String;");
                jstring target = (*env)->GetObjectField(env, ref, targetField);
                if (target) {
                    const char* cTarget = (*env)->GetStringUTFChars(env, target, NULL);
                    strncpy(cConfig->refs[i].target, cTarget, sizeof(cConfig->refs[i].target) - 1);
                    (*env)->ReleaseStringUTFChars(env, target, cTarget);
                    (*env)->DeleteLocalRef(env, target);
                }

                (*env)->DeleteLocalRef(env, ref);
            }
        }
        (*env)->DeleteLocalRef(env, refs);
    }

    // Extract scripts
    field = (*env)->GetFieldID(env, cls, "scriptCount", "I");
    cConfig->script_count = (*env)->GetIntField(env, config, field);

    field = (*env)->GetFieldID(env, cls, "scripts", "[Lcom/gdme/webpulseforecast/WebPulseForecastNative$PackageScript;");
    jobjectArray scripts = (*env)->GetObjectField(env, config, field);
    if (scripts) {
        jsize length = (*env)->GetArrayLength(env, scripts);
        int count = (length > 50) ? 50 : length;  // Maximum 50 scripts as defined in struct

        for (int i = 0; i < count; i++) {
            jobject script = (*env)->GetObjectArrayElement(env, scripts, i);
            if (script) {
                jclass scriptCls = (*env)->GetObjectClass(env, script);

                // Get script name
                jfieldID nameField = (*env)->GetFieldID(env, scriptCls, "scriptName", "Ljava/lang/String;");
                jstring scriptName = (*env)->GetObjectField(env, script, nameField);
                if (scriptName) {
                    const char* cScriptName = (*env)->GetStringUTFChars(env, scriptName, NULL);
                    strncpy(cConfig->scripts[i].script_name, cScriptName, 49);
                    (*env)->ReleaseStringUTFChars(env, scriptName, cScriptName);
                    (*env)->DeleteLocalRef(env, scriptName);
                }

                // Get command
                jfieldID cmdField = (*env)->GetFieldID(env, scriptCls, "command", "Ljava/lang/String;");
                jstring command = (*env)->GetObjectField(env, script, cmdField);
                if (command) {
                    const char* cCommand = (*env)->GetStringUTFChars(env, command, NULL);
                    strncpy(cConfig->scripts[i].command, cCommand, 255);
                    (*env)->ReleaseStringUTFChars(env, command, cCommand);
                    (*env)->DeleteLocalRef(env, command);
                }

                (*env)->DeleteLocalRef(env, script);
            }
        }
        (*env)->DeleteLocalRef(env, scripts);
    }

    TRACE("Exiting extract_package_config");
}

// Helper function to extract package info
static void extract_package_info(JNIEnv *env, jobject package, Package *cPackage) {
    TRACE("Entering extract_package_info");
    if (!package || !cPackage) return;

    jclass cls = (*env)->GetObjectClass(env, package);

    // Extract basic package info
    jfieldID field = (*env)->GetFieldID(env, cls, "name", "Ljava/lang/String;");
    jstring name = (*env)->GetObjectField(env, package, field);
    if (name) {
        const char* cName = (*env)->GetStringUTFChars(env, name, NULL);
        strncpy(cPackage->name, cName, sizeof(cPackage->name) - 1);
        (*env)->ReleaseStringUTFChars(env, name, cName);
        (*env)->DeleteLocalRef(env, name);
    }

    field = (*env)->GetFieldID(env, cls, "version", "Ljava/lang/String;");
    jstring version = (*env)->GetObjectField(env, package, field);
    if (version) {
        const char* cVersion = (*env)->GetStringUTFChars(env, version, NULL);
        strncpy(cPackage->version, cVersion, sizeof(cPackage->version) - 1);
        (*env)->ReleaseStringUTFChars(env, version, cVersion);
        (*env)->DeleteLocalRef(env, version);
    }

    field = (*env)->GetFieldID(env, cls, "path", "Ljava/lang/String;");
    jstring path = (*env)->GetObjectField(env, package, field);
    if (path) {
        const char* cPath = (*env)->GetStringUTFChars(env, path, NULL);
        strncpy(cPackage->path, cPath, MAX_PATH - 1);
        (*env)->ReleaseStringUTFChars(env, path, cPath);
        (*env)->DeleteLocalRef(env, path);
    }

    // Extract dependencies
    field = (*env)->GetFieldID(env, cls, "dependencies",
                               "Lcom/gdme/webpulseforecast/WebPulseForecastNative$DependencyList;");
    jobject dependencies = (*env)->GetObjectField(env, package, field);
    if (dependencies) {
        extract_dependency_list(env, dependencies, &cPackage->dependencies);
        (*env)->DeleteLocalRef(env, dependencies);
    }

    // Extract framework info
    field = (*env)->GetFieldID(env, cls, "frameworkInfo",
                               "Lcom/gdme/webpulseforecast/WebPulseForecastNative$FrameworkInfo;");
    jobject frameworkInfo = (*env)->GetObjectField(env, package, field);
    if (frameworkInfo) {
        extract_framework_info(env, frameworkInfo, &cPackage->framework_info);
        (*env)->DeleteLocalRef(env, frameworkInfo);
    }

    // Extract package config
    field = (*env)->GetFieldID(env, cls, "config",
                               "Lcom/gdme/webpulseforecast/WebPulseForecastNative$PackageConfig;");
    jobject config = (*env)->GetObjectField(env, package, field);
    if (config) {
        extract_package_config(env, config, &cPackage->config);
        (*env)->DeleteLocalRef(env, config);
    }

    TRACE("Exiting extract_package_info");
}

// Helper function to extract workspace info
static void extract_workspace_info(JNIEnv *env, jobject workspaceInfo, WorkspaceInfo *cWorkspaceInfo) {
    TRACE("Entering extract_workspace_info");
    if (!workspaceInfo || !cWorkspaceInfo) {
        TRACE("NULL parameter provided to extract_workspace_info");
        return;
    }

    jclass cls = (*env)->GetObjectClass(env, workspaceInfo);

    // Extract root path
    jfieldID field = (*env)->GetFieldID(env, cls, "rootPath", "Ljava/lang/String;");
    jstring rootPath = (*env)->GetObjectField(env, workspaceInfo, field);
    if (rootPath) {
        const char* cRootPath = (*env)->GetStringUTFChars(env, rootPath, NULL);
        strncpy(cWorkspaceInfo->root_path, cRootPath, MAX_PATH - 1);
        (*env)->ReleaseStringUTFChars(env, rootPath, cRootPath);
        (*env)->DeleteLocalRef(env, rootPath);
    }

    // Extract workspace flags
    field = (*env)->GetFieldID(env, cls, "isLerna", "Z");
    cWorkspaceInfo->is_lerna = (*env)->GetBooleanField(env, workspaceInfo, field);

    field = (*env)->GetFieldID(env, cls, "isYarnWorkspace", "Z");
    cWorkspaceInfo->is_yarn_workspace = (*env)->GetBooleanField(env, workspaceInfo, field);

    field = (*env)->GetFieldID(env, cls, "isPnpmWorkspace", "Z");
    cWorkspaceInfo->is_pnpm_workspace = (*env)->GetBooleanField(env, workspaceInfo, field);

    field = (*env)->GetFieldID(env, cls, "isNxWorkspace", "Z");
    cWorkspaceInfo->is_nx_workspace = (*env)->GetBooleanField(env, workspaceInfo, field);

    field = (*env)->GetFieldID(env, cls, "isRush", "Z");
    cWorkspaceInfo->is_rush = (*env)->GetBooleanField(env, workspaceInfo, field);

    // Extract workspace features
    field = (*env)->GetFieldID(env, cls, "hasHoisting", "Z");
    cWorkspaceInfo->has_hoisting = (*env)->GetBooleanField(env, workspaceInfo, field);

    field = (*env)->GetFieldID(env, cls, "usesNpmWorkspaces", "Z");
    cWorkspaceInfo->uses_npm_workspaces = (*env)->GetBooleanField(env, workspaceInfo, field);

    field = (*env)->GetFieldID(env, cls, "usesChangesets", "Z");
    cWorkspaceInfo->uses_changesets = (*env)->GetBooleanField(env, workspaceInfo, field);

    field = (*env)->GetFieldID(env, cls, "usesTurborepo", "Z");
    cWorkspaceInfo->uses_turborepo = (*env)->GetBooleanField(env, workspaceInfo, field);

    // Extract workspace packages
    field = (*env)->GetFieldID(env, cls, "packageCount", "I");
    cWorkspaceInfo->package_count = (*env)->GetIntField(env, workspaceInfo, field);

    field = (*env)->GetFieldID(env, cls, "packages", "[Lcom/gdme/webpulseforecast/WebPulseForecastNative$Package;");
    jobjectArray packages = (*env)->GetObjectField(env, workspaceInfo, field);
    if (packages) {
        jsize length = (*env)->GetArrayLength(env, packages);
        int count = (length > MAX_PACKAGES) ? MAX_PACKAGES : length;

        for (int i = 0; i < count; i++) {
            jobject pkg = (*env)->GetObjectArrayElement(env, packages, i);
            if (pkg) {
                extract_package_info(env, pkg, &cWorkspaceInfo->packages[i]);
                (*env)->DeleteLocalRef(env, pkg);
            }
        }
        (*env)->DeleteLocalRef(env, packages);
    }

    TRACE("Exiting extract_workspace_info");
}

// Helper function to create Java FrameworkInfo object
jobject create_framework_info_object(JNIEnv *env, const FrameworkInfo *cFrameworkInfo) {
    TRACE("Entering create_framework_info_object");

    if (!cFrameworkInfo) {
        TRACE("NULL cFrameworkInfo pointer provided");
        return NULL;
    }

    // Find the FrameworkInfo class
    jclass cls = (*env)->FindClass(env, "com/gdme/webpulseforecast/WebPulseForecastNative$FrameworkInfo");
    if (!cls) {
        TRACE("Failed to find FrameworkInfo class");
        return NULL;
    }

    // Get the constructor
    jmethodID constructor = (*env)->GetMethodID(env, cls, "<init>", "()V");
    if (!constructor) {
        TRACE("Failed to get FrameworkInfo constructor");
        return NULL;
    }

    // Create new object
    jobject obj = (*env)->NewObject(env, cls, constructor);
    if (!obj) {
        TRACE("Failed to create new FrameworkInfo object");
        return NULL;
    }

    // Get all field IDs
    jfieldID field;

    // Core frameworks
    field = (*env)->GetFieldID(env, cls, "hasReact", "Z");
    (*env)->SetBooleanField(env, obj, field, cFrameworkInfo->has_react);

    field = (*env)->GetFieldID(env, cls, "hasVue", "Z");
    (*env)->SetBooleanField(env, obj, field, cFrameworkInfo->has_vue);

    field = (*env)->GetFieldID(env, cls, "hasAngular", "Z");
    (*env)->SetBooleanField(env, obj, field, cFrameworkInfo->has_angular);

    field = (*env)->GetFieldID(env, cls, "hasSvelte", "Z");
    (*env)->SetBooleanField(env, obj, field, cFrameworkInfo->has_svelte);

    field = (*env)->GetFieldID(env, cls, "hasNodejs", "Z");
    (*env)->SetBooleanField(env, obj, field, cFrameworkInfo->has_nodejs);

    // Meta frameworks
    field = (*env)->GetFieldID(env, cls, "hasNextjs", "Z");
    (*env)->SetBooleanField(env, obj, field, cFrameworkInfo->has_nextjs);

    field = (*env)->GetFieldID(env, cls, "hasNuxtjs", "Z");
    (*env)->SetBooleanField(env, obj, field, cFrameworkInfo->has_nuxtjs);

    // Framework features
    field = (*env)->GetFieldID(env, cls, "reactHooksCount", "I");
    (*env)->SetIntField(env, obj, field, cFrameworkInfo->react_hooks_count);

    field = (*env)->GetFieldID(env, cls, "vueCompositionApi", "Z");
    (*env)->SetBooleanField(env, obj, field, cFrameworkInfo->vue_composition_api);

    field = (*env)->GetFieldID(env, cls, "usesTypescript", "Z");
    (*env)->SetBooleanField(env, obj, field, cFrameworkInfo->uses_typescript);

    field = (*env)->GetFieldID(env, cls, "hasBundler", "Z");
    (*env)->SetBooleanField(env, obj, field, cFrameworkInfo->has_bundler);

    field = (*env)->GetFieldID(env, cls, "hasTesting", "Z");
    (*env)->SetBooleanField(env, obj, field, cFrameworkInfo->has_testing);

    // Architecture and state
    field = (*env)->GetFieldID(env, cls, "hasStateManagement", "Z");
    (*env)->SetBooleanField(env, obj, field, cFrameworkInfo->has_state_management);

    field = (*env)->GetFieldID(env, cls, "hasRouting", "Z");
    (*env)->SetBooleanField(env, obj, field, cFrameworkInfo->has_routing);

    // UI and styling
    field = (*env)->GetFieldID(env, cls, "hasCssFramework", "Z");
    (*env)->SetBooleanField(env, obj, field, cFrameworkInfo->has_css_framework);

    field = (*env)->GetFieldID(env, cls, "hasUiLibrary", "Z");
    (*env)->SetBooleanField(env, obj, field, cFrameworkInfo->has_ui_library);

    field = (*env)->GetFieldID(env, cls, "hasFormLibrary", "Z");
    (*env)->SetBooleanField(env, obj, field, cFrameworkInfo->has_form_library);

    // Version strings
    field = (*env)->GetFieldID(env, cls, "typescriptVersion", "Ljava/lang/String;");
    jstring tsVersion = (*env)->NewStringUTF(env, cFrameworkInfo->typescript_version);
    (*env)->SetObjectField(env, obj, field, tsVersion);

    field = (*env)->GetFieldID(env, cls, "nodeVersion", "Ljava/lang/String;");
    jstring nodeVersion = (*env)->NewStringUTF(env, cFrameworkInfo->node_version);
    (*env)->SetObjectField(env, obj, field, nodeVersion);

    field = (*env)->GetFieldID(env, cls, "primaryBundler", "Ljava/lang/String;");
    jstring bundler = (*env)->NewStringUTF(env, cFrameworkInfo->primary_bundler);
    (*env)->SetObjectField(env, obj, field, bundler);

    field = (*env)->GetFieldID(env, cls, "primaryUiLibrary", "Ljava/lang/String;");
    jstring uiLib = (*env)->NewStringUTF(env, cFrameworkInfo->primary_ui_library);
    (*env)->SetObjectField(env, obj, field, uiLib);

    field = (*env)->GetFieldID(env, cls, "cssSolution", "Ljava/lang/String;");
    jstring css = (*env)->NewStringUTF(env, cFrameworkInfo->css_solution);
    (*env)->SetObjectField(env, obj, field, css);

    // CSS related flags
    field = (*env)->GetFieldID(env, cls, "usesCssModules", "Z");
    (*env)->SetBooleanField(env, obj, field, cFrameworkInfo->uses_css_modules);

    field = (*env)->GetFieldID(env, cls, "usesCssInJs", "Z");
    (*env)->SetBooleanField(env, obj, field, cFrameworkInfo->uses_css_in_js);

    field = (*env)->GetFieldID(env, cls, "usesTailwind", "Z");
    (*env)->SetBooleanField(env, obj, field, cFrameworkInfo->uses_tailwind);

    field = (*env)->GetFieldID(env, cls, "usesSass", "Z");
    (*env)->SetBooleanField(env, obj, field, cFrameworkInfo->uses_sass);

    field = (*env)->GetFieldID(env, cls, "usesLess", "Z");
    (*env)->SetBooleanField(env, obj, field, cFrameworkInfo->uses_less);

    // Testing and quality
    field = (*env)->GetFieldID(env, cls, "hasE2eTesting", "Z");
    (*env)->SetBooleanField(env, obj, field, cFrameworkInfo->has_e2e_testing);

    field = (*env)->GetFieldID(env, cls, "hasUnitTesting", "Z");
    (*env)->SetBooleanField(env, obj, field, cFrameworkInfo->has_unit_testing);

    field = (*env)->GetFieldID(env, cls, "hasComponentTesting", "Z");
    (*env)->SetBooleanField(env, obj, field, cFrameworkInfo->has_component_testing);

    field = (*env)->GetFieldID(env, cls, "hasLinting", "Z");
    (*env)->SetBooleanField(env, obj, field, cFrameworkInfo->has_linting);

    field = (*env)->GetFieldID(env, cls, "hasFormatting", "Z");
    (*env)->SetBooleanField(env, obj, field, cFrameworkInfo->has_formatting);

    // Build and deployment
    field = (*env)->GetFieldID(env, cls, "hasCiCd", "Z");
    (*env)->SetBooleanField(env, obj, field, cFrameworkInfo->has_ci_cd);

    field = (*env)->GetFieldID(env, cls, "hasDocker", "Z");
    (*env)->SetBooleanField(env, obj, field, cFrameworkInfo->has_docker);

    field = (*env)->GetFieldID(env, cls, "hasDeploymentConfig", "Z");
    (*env)->SetBooleanField(env, obj, field, cFrameworkInfo->has_deployment_config);

    // Development tools
    field = (*env)->GetFieldID(env, cls, "hasHotReload", "Z");
    (*env)->SetBooleanField(env, obj, field, cFrameworkInfo->has_hot_reload);

    field = (*env)->GetFieldID(env, cls, "hasDevServer", "Z");
    (*env)->SetBooleanField(env, obj, field, cFrameworkInfo->has_dev_server);

    field = (*env)->GetFieldID(env, cls, "hasDebugConfig", "Z");
    (*env)->SetBooleanField(env, obj, field, cFrameworkInfo->has_debug_config);

    // Package management
    field = (*env)->GetFieldID(env, cls, "usesNpm", "Z");
    (*env)->SetBooleanField(env, obj, field, cFrameworkInfo->uses_npm);

    field = (*env)->GetFieldID(env, cls, "usesYarn", "Z");
    (*env)->SetBooleanField(env, obj, field, cFrameworkInfo->uses_yarn);

    field = (*env)->GetFieldID(env, cls, "usesPnpm", "Z");
    (*env)->SetBooleanField(env, obj, field, cFrameworkInfo->uses_pnpm);

    // Clean up local references
    (*env)->DeleteLocalRef(env, tsVersion);
    (*env)->DeleteLocalRef(env, nodeVersion);
    (*env)->DeleteLocalRef(env, bundler);
    (*env)->DeleteLocalRef(env, uiLib);
    (*env)->DeleteLocalRef(env, css);

    TRACE("Exiting create_framework_info_object successfully");
    return obj;
}

// Helper function to extract HTML info from Java object
static void extract_html_info(JNIEnv *env, jobject htmlInfo, HTMLInfo *cHtmlInfo) {
    if (!htmlInfo) return;
    jclass cls = (*env)->GetObjectClass(env, htmlInfo);

    jfieldID field;
    field = (*env)->GetFieldID(env, cls, "tagCount", "I");
    cHtmlInfo->tag_count = (*env)->GetIntField(env, htmlInfo, field);

    field = (*env)->GetFieldID(env, cls, "scriptCount", "I");
    cHtmlInfo->script_count = (*env)->GetIntField(env, htmlInfo, field);

    field = (*env)->GetFieldID(env, cls, "styleCount", "I");
    cHtmlInfo->style_count = (*env)->GetIntField(env, htmlInfo, field);

    field = (*env)->GetFieldID(env, cls, "linkCount", "I");
    cHtmlInfo->link_count = (*env)->GetIntField(env, htmlInfo, field);

    // Extract custom elements
    jfieldID customElementsField = (*env)->GetFieldID(env, cls, "customElements",
                                                      "[Lcom/gdme/webpulseforecast/WebPulseForecastNative$CustomElement;");
    jobjectArray customElements = (*env)->GetObjectField(env, htmlInfo, customElementsField);

    if (customElements) {
        jsize length = (*env)->GetArrayLength(env, customElements);
        cHtmlInfo->custom_element_count = (length > MAX_CUSTOM_ELEMENTS) ? MAX_CUSTOM_ELEMENTS : length;

        for (int i = 0; i < cHtmlInfo->custom_element_count; i++) {
            jobject element = (*env)->GetObjectArrayElement(env, customElements, i);
            jclass elementCls = (*env)->GetObjectClass(env, element);

            // Get name
            jfieldID nameField = (*env)->GetFieldID(env, elementCls, "name", "Ljava/lang/String;");
            jstring name = (*env)->GetObjectField(env, element, nameField);
            const char* cName = (*env)->GetStringUTFChars(env, name, NULL);
            strncpy(cHtmlInfo->custom_elements[i].name, cName, 49);
            (*env)->ReleaseStringUTFChars(env, name, cName);

            // Get count
            jfieldID countField = (*env)->GetFieldID(env, elementCls, "count", "I");
            cHtmlInfo->custom_elements[i].count = (*env)->GetIntField(env, element, countField);

            (*env)->DeleteLocalRef(env, element);
            (*env)->DeleteLocalRef(env, name);
        }
    }
}

static void extract_css_info(JNIEnv *env, jobject cssInfo, CSSInfo *cCssInfo) {
    if (!cssInfo) return;
    jclass cls = (*env)->GetObjectClass(env, cssInfo);

    jfieldID field;
    field = (*env)->GetFieldID(env, cls, "ruleCount", "I");
    cCssInfo->rule_count = (*env)->GetIntField(env, cssInfo, field);

    field = (*env)->GetFieldID(env, cls, "selectorCount", "I");
    cCssInfo->selector_count = (*env)->GetIntField(env, cssInfo, field);

    field = (*env)->GetFieldID(env, cls, "propertyCount", "I");
    cCssInfo->property_count = (*env)->GetIntField(env, cssInfo, field);

    field = (*env)->GetFieldID(env, cls, "mediaQueryCount", "I");
    cCssInfo->media_query_count = (*env)->GetIntField(env, cssInfo, field);

    field = (*env)->GetFieldID(env, cls, "keyframeCount", "I");
    cCssInfo->keyframe_count = (*env)->GetIntField(env, cssInfo, field);
}

void extract_project_type(JNIEnv *env, jobject jProjectType, ProjectType *cProjectType) {
    TRACE("Entering extract_project_type");
    if (!jProjectType || !cProjectType) {
        TRACE("NULL parameter provided to extract_project_type");
        return;
    }

    // Initialize the C structure
    memset(cProjectType, 0, sizeof(ProjectType));

    jclass cls = (*env)->GetObjectClass(env, jProjectType);

    // Extract framework info
    jfieldID frameworkInfoField = (*env)->GetFieldID(env, cls, "frameworkInfo",
                                                     "Lcom/gdme/webpulseforecast/WebPulseForecastNative$FrameworkInfo;");
    jobject frameworkInfo = (*env)->GetObjectField(env, jProjectType, frameworkInfoField);
    if (frameworkInfo) {
        extract_framework_info(env, frameworkInfo, &cProjectType->framework_info);
        (*env)->DeleteLocalRef(env, frameworkInfo);
    }

    // Extract file counts
    jfieldID field;
    field = (*env)->GetFieldID(env, cls, "htmlFileCount", "I");
    cProjectType->html_file_count = (*env)->GetIntField(env, jProjectType, field);

    field = (*env)->GetFieldID(env, cls, "cssFileCount", "I");
    cProjectType->css_file_count = (*env)->GetIntField(env, jProjectType, field);

    field = (*env)->GetFieldID(env, cls, "jsFileCount", "I");
    cProjectType->js_file_count = (*env)->GetIntField(env, jProjectType, field);

    field = (*env)->GetFieldID(env, cls, "jsonFileCount", "I");
    cProjectType->json_file_count = (*env)->GetIntField(env, jProjectType, field);

    field = (*env)->GetFieldID(env, cls, "tsFileCount", "I");
    cProjectType->ts_file_count = (*env)->GetIntField(env, jProjectType, field);

    field = (*env)->GetFieldID(env, cls, "jsxFileCount", "I");
    cProjectType->jsx_file_count = (*env)->GetIntField(env, jProjectType, field);

    field = (*env)->GetFieldID(env, cls, "vueFileCount", "I");
    cProjectType->vue_file_count = (*env)->GetIntField(env, jProjectType, field);

    field = (*env)->GetFieldID(env, cls, "xmlFileCount", "I");
    cProjectType->xml_file_count = (*env)->GetIntField(env, jProjectType, field);

    field = (*env)->GetFieldID(env, cls, "imageFileCount", "I");
    cProjectType->image_file_count = (*env)->GetIntField(env, jProjectType, field);

    // Extract component tracking
    field = (*env)->GetFieldID(env, cls, "reactComponentCount", "I");
    cProjectType->react_component_count = (*env)->GetIntField(env, jProjectType, field);

    field = (*env)->GetFieldID(env, cls, "customElementCount", "I");
    cProjectType->custom_element_count = (*env)->GetIntField(env, jProjectType, field);

    // Extract external resources
    field = (*env)->GetFieldID(env, cls, "externalResourceCount", "I");
    cProjectType->external_resource_count = (*env)->GetIntField(env, jProjectType, field);

    // Extract Parser Results
    // HTML Info
    field = (*env)->GetFieldID(env, cls, "totalHtmlInfo",
                               "Lcom/gdme/webpulseforecast/WebPulseForecastNative$HTMLInfo;");
    jobject htmlInfo = (*env)->GetObjectField(env, jProjectType, field);
    if (htmlInfo) {
        extract_html_info(env, htmlInfo, &cProjectType->total_html_info);
        (*env)->DeleteLocalRef(env, htmlInfo);
    }

    // CSS Info
    field = (*env)->GetFieldID(env, cls, "totalCssInfo",
                               "Lcom/gdme/webpulseforecast/WebPulseForecastNative$CSSInfo;");
    jobject cssInfo = (*env)->GetObjectField(env, jProjectType, field);
    if (cssInfo) {
        extract_css_info(env, cssInfo, &cProjectType->total_css_info);
        (*env)->DeleteLocalRef(env, cssInfo);
    }

    // Extract workspace info if present
    field = (*env)->GetFieldID(env, cls, "workspaceInfo",
                               "Lcom/gdme/webpulseforecast/WebPulseForecastNative$WorkspaceInfo;");
    jobject workspaceInfo = (*env)->GetObjectField(env, jProjectType, field);
    if (workspaceInfo) {
        extract_workspace_info(env, workspaceInfo, &cProjectType->workspace);
        (*env)->DeleteLocalRef(env, workspaceInfo);
    }

    // Extract flags
    field = (*env)->GetFieldID(env, cls, "usesCommonjs", "Z");
    cProjectType->uses_commonjs = (*env)->GetBooleanField(env, jProjectType, field);

    field = (*env)->GetFieldID(env, cls, "usesEsmodules", "Z");
    cProjectType->uses_esmodules = (*env)->GetBooleanField(env, jProjectType, field);

    field = (*env)->GetFieldID(env, cls, "hasWebpack", "Z");
    cProjectType->has_webpack = (*env)->GetBooleanField(env, jProjectType, field);

    field = (*env)->GetFieldID(env, cls, "hasVite", "Z");
    cProjectType->has_vite = (*env)->GetBooleanField(env, jProjectType, field);

    field = (*env)->GetFieldID(env, cls, "hasBabel", "Z");
    cProjectType->has_babel = (*env)->GetBooleanField(env, jProjectType, field);

    field = (*env)->GetFieldID(env, cls, "hasCi", "Z");
    cProjectType->has_ci = (*env)->GetBooleanField(env, jProjectType, field);

    field = (*env)->GetFieldID(env, cls, "hasEnvConfig", "Z");
    cProjectType->has_env_config = (*env)->GetBooleanField(env, jProjectType, field);

    field = (*env)->GetFieldID(env, cls, "hasTypescript", "Z");
    cProjectType->has_typescript = (*env)->GetBooleanField(env, jProjectType, field);

    field = (*env)->GetFieldID(env, cls, "isMonorepo", "Z");
    cProjectType->is_monorepo = (*env)->GetBooleanField(env, jProjectType, field);

    // Extract dependencies
    extract_dependency_list(env, jProjectType, &cProjectType->dependencies);

    // Extract module paths
    field = (*env)->GetFieldID(env, cls, "modulePathCount", "I");
    cProjectType->module_path_count = (*env)->GetIntField(env, jProjectType, field);

    field = (*env)->GetFieldID(env, cls, "modulePaths", "[Ljava/lang/String;");
    jobjectArray modulePaths = (*env)->GetObjectField(env, jProjectType, field);
    if (modulePaths) {
        jsize length = (*env)->GetArrayLength(env, modulePaths);
        int count = (length > MAX_IMPORT_PATHS) ? MAX_IMPORT_PATHS : length;

        for (int i = 0; i < count; i++) {
            jstring path = (*env)->GetObjectArrayElement(env, modulePaths, i);
            const char* cPath = (*env)->GetStringUTFChars(env, path, NULL);
            strncpy(cProjectType->module_paths[i], cPath, MAX_PATH - 1);
            (*env)->ReleaseStringUTFChars(env, path, cPath);
            (*env)->DeleteLocalRef(env, path);
        }
    }

    TRACE("Exiting extract_project_type");
}

// Helper function to create a Java String from a C string
jstring create_jstring(JNIEnv *env, const char *str) {
    return (*env)->NewStringUTF(env, str);
}

// Helper function to create Custom Element object
static jobject create_custom_element_object(JNIEnv *env, const CustomElement *element) {
    TRACE("Entering create_custom_element_object");
    if (!element) return NULL;

    jclass cls = (*env)->FindClass(env,
                                   "com/gdme/webpulseforecast/WebPulseForecastNative$CustomElement");
    if (!cls) return NULL;

    jmethodID constructor = (*env)->GetMethodID(env, cls, "<init>", "()V");
    jobject obj = (*env)->NewObject(env, cls, constructor);
    if (!obj) return NULL;

    // Set name
    jfieldID field = (*env)->GetFieldID(env, cls, "name", "Ljava/lang/String;");
    jstring name = (*env)->NewStringUTF(env, element->name);
    (*env)->SetObjectField(env, obj, field, name);
    (*env)->DeleteLocalRef(env, name);

    // Set count
    field = (*env)->GetFieldID(env, cls, "count", "I");
    (*env)->SetIntField(env, obj, field, element->count);

    TRACE("Exiting create_custom_element_object");
    return obj;
}

// Helper function to create HTML Info object
static jobject create_html_info_object(JNIEnv *env, const HTMLInfo *cHtmlInfo) {
    TRACE("Entering create_html_info_object");
    if (!cHtmlInfo) return NULL;

    jclass cls = (*env)->FindClass(env, "com/gdme/webpulseforecast/WebPulseForecastNative$HTMLInfo");
    if (!cls) return NULL;

    jmethodID constructor = (*env)->GetMethodID(env, cls, "<init>", "()V");
    jobject obj = (*env)->NewObject(env, cls, constructor);
    if (!obj) return NULL;

    // Set basic counts
    jfieldID field = (*env)->GetFieldID(env, cls, "tagCount", "I");
    (*env)->SetIntField(env, obj, field, cHtmlInfo->tag_count);

    field = (*env)->GetFieldID(env, cls, "scriptCount", "I");
    (*env)->SetIntField(env, obj, field, cHtmlInfo->script_count);

    field = (*env)->GetFieldID(env, cls, "styleCount", "I");
    (*env)->SetIntField(env, obj, field, cHtmlInfo->style_count);

    field = (*env)->GetFieldID(env, cls, "linkCount", "I");
    (*env)->SetIntField(env, obj, field, cHtmlInfo->link_count);

    // Create custom elements array
    jclass elementCls = (*env)->FindClass(env,
                                          "com/gdme/webpulseforecast/WebPulseForecastNative$CustomElement");
    jobjectArray customElements = (*env)->NewObjectArray(env,
                                                         cHtmlInfo->custom_element_count, elementCls, NULL);

    for (int i = 0; i < cHtmlInfo->custom_element_count; i++) {
        jobject element = create_custom_element_object(env, &cHtmlInfo->custom_elements[i]);
        if (element) {
            (*env)->SetObjectArrayElement(env, customElements, i, element);
            (*env)->DeleteLocalRef(env, element);
        }
    }

    field = (*env)->GetFieldID(env, cls, "customElements",
                               "[Lcom/gdme/webpulseforecast/WebPulseForecastNative$CustomElement;");
    (*env)->SetObjectField(env, obj, field, customElements);
    (*env)->DeleteLocalRef(env, customElements);

    TRACE("Exiting create_html_info_object");
    return obj;
}

// Helper function to create CSS Info object
static jobject create_css_info_object(JNIEnv *env, const CSSInfo *cCssInfo) {
    TRACE("Entering create_css_info_object");
    if (!cCssInfo) return NULL;

    jclass cls = (*env)->FindClass(env, "com/gdme/webpulseforecast/WebPulseForecastNative$CSSInfo");
    if (!cls) return NULL;

    jmethodID constructor = (*env)->GetMethodID(env, cls, "<init>", "()V");
    jobject obj = (*env)->NewObject(env, cls, constructor);
    if (!obj) return NULL;

    jfieldID field = (*env)->GetFieldID(env, cls, "ruleCount", "I");
    (*env)->SetIntField(env, obj, field, cCssInfo->rule_count);

    field = (*env)->GetFieldID(env, cls, "selectorCount", "I");
    (*env)->SetIntField(env, obj, field, cCssInfo->selector_count);

    field = (*env)->GetFieldID(env, cls, "propertyCount", "I");
    (*env)->SetIntField(env, obj, field, cCssInfo->property_count);

    field = (*env)->GetFieldID(env, cls, "mediaQueryCount", "I");
    (*env)->SetIntField(env, obj, field, cCssInfo->media_query_count);

    field = (*env)->GetFieldID(env, cls, "keyframeCount", "I");
    (*env)->SetIntField(env, obj, field, cCssInfo->keyframe_count);

    TRACE("Exiting create_css_info_object");
    return obj;
}

// Helper function to create JS Info object
static jobject create_js_info_object(JNIEnv *env, const JSInfo *cJsInfo) {
    TRACE("Entering create_js_info_object");
    if (!cJsInfo) return NULL;

    jclass cls = (*env)->FindClass(env, "com/gdme/webpulseforecast/WebPulseForecastNative$JSInfo");
    if (!cls) return NULL;

    jmethodID constructor = (*env)->GetMethodID(env, cls, "<init>", "()V");
    jobject obj = (*env)->NewObject(env, cls, constructor);
    if (!obj) return NULL;

    // Set basic counts
    jfieldID field = (*env)->GetFieldID(env, cls, "functionCount", "I");
    (*env)->SetIntField(env, obj, field, cJsInfo->function_count);

    field = (*env)->GetFieldID(env, cls, "variableCount", "I");
    (*env)->SetIntField(env, obj, field, cJsInfo->variable_count);

    field = (*env)->GetFieldID(env, cls, "classCount", "I");
    (*env)->SetIntField(env, obj, field, cJsInfo->class_count);

    field = (*env)->GetFieldID(env, cls, "reactComponentCount", "I");
    (*env)->SetIntField(env, obj, field, cJsInfo->react_component_count);

    field = (*env)->GetFieldID(env, cls, "vueInstanceCount", "I");
    (*env)->SetIntField(env, obj, field, cJsInfo->vue_instance_count);

    field = (*env)->GetFieldID(env, cls, "angularModuleCount", "I");
    (*env)->SetIntField(env, obj, field, cJsInfo->angular_module_count);

    field = (*env)->GetFieldID(env, cls, "eventListenerCount", "I");
    (*env)->SetIntField(env, obj, field, cJsInfo->event_listener_count);

    field = (*env)->GetFieldID(env, cls, "asyncFunctionCount", "I");
    (*env)->SetIntField(env, obj, field, cJsInfo->async_function_count);

    field = (*env)->GetFieldID(env, cls, "promiseCount", "I");
    (*env)->SetIntField(env, obj, field, cJsInfo->promise_count);

    field = (*env)->GetFieldID(env, cls, "closureCount", "I");
    (*env)->SetIntField(env, obj, field, cJsInfo->closure_count);

    // Set framework info
    jobject frameworkInfo = create_framework_info_object(env, &cJsInfo->framework);
    if (frameworkInfo) {
        field = (*env)->GetFieldID(env, cls, "framework",
                                   "Lcom/gdme/webpulseforecast/WebPulseForecastNative$FrameworkInfo;");
        (*env)->SetObjectField(env, obj, field, frameworkInfo);
        (*env)->DeleteLocalRef(env, frameworkInfo);
    }

    TRACE("Exiting create_js_info_object");
    return obj;
}

// Helper function to create JSON Info object
static jobject create_json_info_object(JNIEnv *env, const JSONInfo *cJsonInfo) {
    TRACE("Entering create_json_info_object");
    if (!cJsonInfo) return NULL;

    jclass cls = (*env)->FindClass(env, "com/gdme/webpulseforecast/WebPulseForecastNative$JSONInfo");
    if (!cls) return NULL;

    jmethodID constructor = (*env)->GetMethodID(env, cls, "<init>", "()V");
    jobject obj = (*env)->NewObject(env, cls, constructor);
    if (!obj) return NULL;

    jfieldID field = (*env)->GetFieldID(env, cls, "objectCount", "I");
    (*env)->SetIntField(env, obj, field, cJsonInfo->object_count);

    field = (*env)->GetFieldID(env, cls, "arrayCount", "I");
    (*env)->SetIntField(env, obj, field, cJsonInfo->array_count);

    field = (*env)->GetFieldID(env, cls, "keyCount", "I");
    (*env)->SetIntField(env, obj, field, cJsonInfo->key_count);

    field = (*env)->GetFieldID(env, cls, "maxNestingLevel", "I");
    (*env)->SetIntField(env, obj, field, cJsonInfo->max_nesting_level);

    TRACE("Exiting create_json_info_object");
    return obj;
}

// Helper function to create Dependency object
static jobject create_dependency_object(JNIEnv *env, const Dependency *dependency) {
    TRACE("Entering create_dependency_object");
    if (!dependency) return NULL;

    jclass cls = (*env)->FindClass(env,
                                   "com/gdme/webpulseforecast/WebPulseForecastNative$Dependency");
    if (!cls) return NULL;

    jmethodID constructor = (*env)->GetMethodID(env, cls, "<init>", "()V");
    jobject obj = (*env)->NewObject(env, cls, constructor);
    if (!obj) return NULL;

    // Set name
    jfieldID field = (*env)->GetFieldID(env, cls, "name", "Ljava/lang/String;");
    jstring name = (*env)->NewStringUTF(env, dependency->name);
    (*env)->SetObjectField(env, obj, field, name);
    (*env)->DeleteLocalRef(env, name);

    // Set version
    field = (*env)->GetFieldID(env, cls, "version", "Ljava/lang/String;");
    jstring version = (*env)->NewStringUTF(env, dependency->version);
    (*env)->SetObjectField(env, obj, field, version);
    (*env)->DeleteLocalRef(env, version);

    // Set isDevDependency
    field = (*env)->GetFieldID(env, cls, "isDevDependency", "Z");
    (*env)->SetBooleanField(env, obj, field, dependency->is_dev_dependency);

    TRACE("Exiting create_dependency_object");
    return obj;
}

// Helper function to create External Resource object
static jobject create_external_resource_object(JNIEnv *env, const ExternalResource *resource) {
    TRACE("Entering create_external_resource_object");
    if (!resource) return NULL;

    jclass cls = (*env)->FindClass(env,
                                   "com/gdme/webpulseforecast/WebPulseForecastNative$ExternalResource");
    if (!cls) return NULL;

    jmethodID constructor = (*env)->GetMethodID(env, cls, "<init>", "()V");
    jobject obj = (*env)->NewObject(env, cls, constructor);
    if (!obj) return NULL;

    // Set URL
    jfieldID field = (*env)->GetFieldID(env, cls, "url", "Ljava/lang/String;");
    jstring url = (*env)->NewStringUTF(env, resource->url);
    (*env)->SetObjectField(env, obj, field, url);
    (*env)->DeleteLocalRef(env, url);

    // Set type
    field = (*env)->GetFieldID(env, cls, "type", "Ljava/lang/String;");
    jstring type = (*env)->NewStringUTF(env, resource->type);
    (*env)->SetObjectField(env, obj, field, type);
    (*env)->DeleteLocalRef(env, type);

    // Set size
    field = (*env)->GetFieldID(env, cls, "size", "J");
    (*env)->SetLongField(env, obj, field, resource->size);

    TRACE("Exiting create_external_resource_object");
    return obj;
}

// Helper function to create a TaskGroup object
static jobject create_task_group_object(JNIEnv *env, const TaskGroup *taskGroup) {
    TRACE("Entering create_task_group_object");
    if (!taskGroup) return NULL;

    jclass cls = (*env)->FindClass(env,
                                   "com/gdme/webpulseforecast/WebPulseForecastNative$TaskGroup");
    if (!cls) return NULL;

    jmethodID constructor = (*env)->GetMethodID(env, cls, "<init>", "()V");
    jobject obj = (*env)->NewObject(env, cls, constructor);
    if (!obj) return NULL;

    // Set name
    jfieldID field = (*env)->GetFieldID(env, cls, "name", "Ljava/lang/String;");
    jstring name = (*env)->NewStringUTF(env, taskGroup->name);
    (*env)->SetObjectField(env, obj, field, name);
    (*env)->DeleteLocalRef(env, name);

    // Set type
    field = (*env)->GetFieldID(env, cls, "type", "Ljava/lang/String;");
    jstring type = (*env)->NewStringUTF(env, taskGroup->type);
    (*env)->SetObjectField(env, obj, field, type);
    (*env)->DeleteLocalRef(env, type);

    // Create and set packages array
    jclass stringCls = (*env)->FindClass(env, "java/lang/String");
    jobjectArray packages = (*env)->NewObjectArray(env, taskGroup->package_count, stringCls, NULL);

    for (int i = 0; i < taskGroup->package_count; i++) {
        jstring pkg = (*env)->NewStringUTF(env, taskGroup->packages[i]);
        (*env)->SetObjectArrayElement(env, packages, i, pkg);
        (*env)->DeleteLocalRef(env, pkg);
    }

    field = (*env)->GetFieldID(env, cls, "packages", "[Ljava/lang/String;");
    (*env)->SetObjectField(env, obj, field, packages);
    (*env)->DeleteLocalRef(env, packages);

    field = (*env)->GetFieldID(env, cls, "packageCount", "I");
    (*env)->SetIntField(env, obj, field, taskGroup->package_count);

    TRACE("Exiting create_task_group_object");
    return obj;
}

static jobject create_package_config_object(JNIEnv *env, const PackageConfig *config) {
    TRACE("Entering create_package_config_object");
    if (!config) {
        TRACE("NULL config pointer provided");
        return NULL;
    }

    jclass cls = (*env)->FindClass(env,
                                   "com/gdme/webpulseforecast/WebPulseForecastNative$PackageConfig");
    if (!cls) {
        TRACE("Failed to find PackageConfig class");
        return NULL;
    }

    jmethodID constructor = (*env)->GetMethodID(env, cls, "<init>", "()V");
    if (!constructor) {
        TRACE("Failed to get PackageConfig constructor");
        return NULL;
    }

    jobject obj = (*env)->NewObject(env, cls, constructor);
    if (!obj) {
        TRACE("Failed to create new PackageConfig object");
        return NULL;
    }

    // Set package references
    jclass refCls = (*env)->FindClass(env,
                                      "com/gdme/webpulseforecast/WebPulseForecastNative$PackageReference");
    jobjectArray refs = (*env)->NewObjectArray(env, config->ref_count, refCls, NULL);

    for (int i = 0; i < config->ref_count; i++) {
        jobject ref = (*env)->NewObject(env, refCls,
                                        (*env)->GetMethodID(env, refCls, "<init>", "()V"));

        if (ref) {
            // Set source
            jfieldID field = (*env)->GetFieldID(env, refCls, "source", "Ljava/lang/String;");
            jstring source = (*env)->NewStringUTF(env, config->refs[i].source);
            (*env)->SetObjectField(env, ref, field, source);
            (*env)->DeleteLocalRef(env, source);

            // Set target
            field = (*env)->GetFieldID(env, refCls, "target", "Ljava/lang/String;");
            jstring target = (*env)->NewStringUTF(env, config->refs[i].target);
            (*env)->SetObjectField(env, ref, field, target);
            (*env)->DeleteLocalRef(env, target);

            (*env)->SetObjectArrayElement(env, refs, i, ref);
            (*env)->DeleteLocalRef(env, ref);
        }
    }

    jfieldID field = (*env)->GetFieldID(env, cls, "refs",
                                        "[Lcom/gdme/webpulseforecast/WebPulseForecastNative$PackageReference;");
    (*env)->SetObjectField(env, obj, field, refs);
    (*env)->DeleteLocalRef(env, refs);

    field = (*env)->GetFieldID(env, cls, "refCount", "I");
    (*env)->SetIntField(env, obj, field, config->ref_count);

    // Set scripts
    jclass scriptCls = (*env)->FindClass(env,
                                         "com/gdme/webpulseforecast/WebPulseForecastNative$PackageScript");
    jobjectArray scripts = (*env)->NewObjectArray(env, config->script_count, scriptCls, NULL);

    for (int i = 0; i < config->script_count; i++) {
        jobject script = (*env)->NewObject(env, scriptCls,
                                           (*env)->GetMethodID(env, scriptCls, "<init>", "()V"));

        if (script) {
            // Set script name
            jfieldID scriptField = (*env)->GetFieldID(env, scriptCls, "scriptName", "Ljava/lang/String;");
            jstring scriptName = (*env)->NewStringUTF(env, config->scripts[i].script_name);
            (*env)->SetObjectField(env, script, scriptField, scriptName);
            (*env)->DeleteLocalRef(env, scriptName);

            // Set command
            scriptField = (*env)->GetFieldID(env, scriptCls, "command", "Ljava/lang/String;");
            jstring command = (*env)->NewStringUTF(env, config->scripts[i].command);
            (*env)->SetObjectField(env, script, scriptField, command);
            (*env)->DeleteLocalRef(env, command);

            (*env)->SetObjectArrayElement(env, scripts, i, script);
            (*env)->DeleteLocalRef(env, script);
        }
    }

    field = (*env)->GetFieldID(env, cls, "scripts",
                               "[Lcom/gdme/webpulseforecast/WebPulseForecastNative$PackageScript;");
    (*env)->SetObjectField(env, obj, field, scripts);
    (*env)->DeleteLocalRef(env, scripts);

    field = (*env)->GetFieldID(env, cls, "scriptCount", "I");
    (*env)->SetIntField(env, obj, field, config->script_count);

    // Set paths
    field = (*env)->GetFieldID(env, cls, "buildOutputPath", "Ljava/lang/String;");
    jstring buildPath = (*env)->NewStringUTF(env, config->build_output_path);
    (*env)->SetObjectField(env, obj, field, buildPath);
    (*env)->DeleteLocalRef(env, buildPath);

    field = (*env)->GetFieldID(env, cls, "testOutputPath", "Ljava/lang/String;");
    jstring testPath = (*env)->NewStringUTF(env, config->test_output_path);
    (*env)->SetObjectField(env, obj, field, testPath);
    (*env)->DeleteLocalRef(env, testPath);

    // Set configuration flags
    field = (*env)->GetFieldID(env, cls, "hasSharedConfigs", "Z");
    (*env)->SetBooleanField(env, obj, field, config->has_shared_configs);

    field = (*env)->GetFieldID(env, cls, "usesTypescript", "Z");
    (*env)->SetBooleanField(env, obj, field, config->uses_typescript);

    field = (*env)->GetFieldID(env, cls, "usesEslint", "Z");
    (*env)->SetBooleanField(env, obj, field, config->uses_eslint);

    field = (*env)->GetFieldID(env, cls, "usesPrettier", "Z");
    (*env)->SetBooleanField(env, obj, field, config->uses_prettier);

    field = (*env)->GetFieldID(env, cls, "usesJest", "Z");
    (*env)->SetBooleanField(env, obj, field, config->uses_jest);

    // Set Node version
    field = (*env)->GetFieldID(env, cls, "nodeVersion", "Ljava/lang/String;");
    jstring nodeVersion = (*env)->NewStringUTF(env, config->node_version);
    (*env)->SetObjectField(env, obj, field, nodeVersion);
    (*env)->DeleteLocalRef(env, nodeVersion);

    TRACE("Exiting create_package_config_object");
    return obj;
}

// Helper function to create Dependency List object
static jobject create_dependency_list_object(JNIEnv *env, const DependencyList *dependencies) {
    TRACE("Entering create_dependency_list_object");
    if (!dependencies) return NULL;

    jclass cls = (*env)->FindClass(env,
                                   "com/gdme/webpulseforecast/WebPulseForecastNative$DependencyList");
    if (!cls) return NULL;

    jmethodID constructor = (*env)->GetMethodID(env, cls, "<init>", "()V");
    jobject obj = (*env)->NewObject(env, cls, constructor);
    if (!obj) return NULL;

    // Set count
    jfieldID field = (*env)->GetFieldID(env, cls, "count", "I");
    (*env)->SetIntField(env, obj, field, dependencies->count);

    // Create and set dependencies array
    jclass depCls = (*env)->FindClass(env,
                                      "com/gdme/webpulseforecast/WebPulseForecastNative$Dependency");
    jobjectArray items = (*env)->NewObjectArray(env, dependencies->count, depCls, NULL);

    for (int i = 0; i < dependencies->count; i++) {
        jobject dep = create_dependency_object(env, &dependencies->items[i]);
        if (dep) {
            (*env)->SetObjectArrayElement(env, items, i, dep);
            (*env)->DeleteLocalRef(env, dep);
        }
    }

    field = (*env)->GetFieldID(env, cls, "items",
                               "[Lcom/gdme/webpulseforecast/WebPulseForecastNative$Dependency;");
    (*env)->SetObjectField(env, obj, field, items);
    (*env)->DeleteLocalRef(env, items);

    TRACE("Exiting create_dependency_list_object");
    return obj;
}

// Helper function to create a Package object
static jobject create_package_object(JNIEnv *env, const Package *package) {
    TRACE("Entering create_package_object");
    if (!package) return NULL;

    jclass cls = (*env)->FindClass(env,
                                   "com/gdme/webpulseforecast/WebPulseForecastNative$Package");
    if (!cls) return NULL;

    jmethodID constructor = (*env)->GetMethodID(env, cls, "<init>", "()V");
    jobject obj = (*env)->NewObject(env, cls, constructor);
    if (!obj) return NULL;

    // Set basic fields
    jfieldID field = (*env)->GetFieldID(env, cls, "name", "Ljava/lang/String;");
    jstring name = (*env)->NewStringUTF(env, package->name);
    (*env)->SetObjectField(env, obj, field, name);
    (*env)->DeleteLocalRef(env, name);

    field = (*env)->GetFieldID(env, cls, "version", "Ljava/lang/String;");
    jstring version = (*env)->NewStringUTF(env, package->version);
    (*env)->SetObjectField(env, obj, field, version);
    (*env)->DeleteLocalRef(env, version);

    field = (*env)->GetFieldID(env, cls, "path", "Ljava/lang/String;");
    jstring path = (*env)->NewStringUTF(env, package->path);
    (*env)->SetObjectField(env, obj, field, path);
    (*env)->DeleteLocalRef(env, path);

    // Set dependencies
    jobject dependencies = create_dependency_list_object(env, &package->dependencies);
    if (dependencies) {
        field = (*env)->GetFieldID(env, cls, "dependencies",
                                   "Lcom/gdme/webpulseforecast/WebPulseForecastNative$DependencyList;");
        (*env)->SetObjectField(env, obj, field, dependencies);
        (*env)->DeleteLocalRef(env, dependencies);
    }

    // Set framework info
    jobject frameworkInfo = create_framework_info_object(env, &package->framework_info);
    if (frameworkInfo) {
        field = (*env)->GetFieldID(env, cls, "frameworkInfo",
                                   "Lcom/gdme/webpulseforecast/WebPulseForecastNative$FrameworkInfo;");
        (*env)->SetObjectField(env, obj, field, frameworkInfo);
        (*env)->DeleteLocalRef(env, frameworkInfo);
    }

    // Set package config
    jobject config = create_package_config_object(env, &package->config);
    if (config) {
        field = (*env)->GetFieldID(env, cls, "config",
                                   "Lcom/gdme/webpulseforecast/WebPulseForecastNative$PackageConfig;");
        (*env)->SetObjectField(env, obj, field, config);
        (*env)->DeleteLocalRef(env, config);
    }

    TRACE("Exiting create_package_object");
    return obj;
}

// Main function to create workspace info object
static jobject create_workspace_info_object(JNIEnv *env, const WorkspaceInfo *workspace) {
    TRACE("Entering create_workspace_info_object");
    if (!workspace) return NULL;

    jclass cls = (*env)->FindClass(env,
                                   "com/gdme/webpulseforecast/WebPulseForecastNative$WorkspaceInfo");
    if (!cls) return NULL;

    jmethodID constructor = (*env)->GetMethodID(env, cls, "<init>", "()V");
    jobject obj = (*env)->NewObject(env, cls, constructor);
    if (!obj) return NULL;

    // Set root path
    jfieldID field = (*env)->GetFieldID(env, cls, "rootPath", "Ljava/lang/String;");
    jstring rootPath = (*env)->NewStringUTF(env, workspace->root_path);
    (*env)->SetObjectField(env, obj, field, rootPath);
    (*env)->DeleteLocalRef(env, rootPath);

    // Set name
    field = (*env)->GetFieldID(env, cls, "name", "Ljava/lang/String;");
    jstring name = (*env)->NewStringUTF(env, workspace->name);
    (*env)->SetObjectField(env, obj, field, name);
    (*env)->DeleteLocalRef(env, name);

    // Create and set packages array
    jclass packageCls = (*env)->FindClass(env,
                                          "com/gdme/webpulseforecast/WebPulseForecastNative$Package");
    jobjectArray packages = (*env)->NewObjectArray(env, workspace->package_count, packageCls, NULL);

    for (int i = 0; i < workspace->package_count; i++) {
        jobject pkg = create_package_object(env, &workspace->packages[i]);
        if (pkg) {
            (*env)->SetObjectArrayElement(env, packages, i, pkg);
            (*env)->DeleteLocalRef(env, pkg);
        }
    }

    field = (*env)->GetFieldID(env, cls, "packages",
                               "[Lcom/gdme/webpulseforecast/WebPulseForecastNative$Package;");
    (*env)->SetObjectField(env, obj, field, packages);
    (*env)->DeleteLocalRef(env, packages);

    // Set shared dependencies
    jobject sharedDeps = create_dependency_list_object(env, &workspace->shared_dependencies);
    if (sharedDeps) {
        field = (*env)->GetFieldID(env, cls, "sharedDependencies",
                                   "Lcom/gdme/webpulseforecast/WebPulseForecastNative$DependencyList;");
        (*env)->SetObjectField(env, obj, field, sharedDeps);
        (*env)->DeleteLocalRef(env, sharedDeps);
    }

    // Create and set workspace globs array
    jclass stringCls = (*env)->FindClass(env, "java/lang/String");
    jobjectArray globs = (*env)->NewObjectArray(env, workspace->workspace_count, stringCls, NULL);

    for (int i = 0; i < workspace->workspace_count; i++) {
        jstring glob = (*env)->NewStringUTF(env, workspace->workspace_globs[i]);
        (*env)->SetObjectArrayElement(env, globs, i, glob);
        (*env)->DeleteLocalRef(env, glob);
    }

    field = (*env)->GetFieldID(env, cls, "workspaceGlobs", "[Ljava/lang/String;");
    (*env)->SetObjectField(env, obj, field, globs);
    (*env)->DeleteLocalRef(env, globs);

    // Set all boolean flags
    field = (*env)->GetFieldID(env, cls, "isLerna", "Z");
    (*env)->SetBooleanField(env, obj, field, workspace->is_lerna);

    field = (*env)->GetFieldID(env, cls, "isYarnWorkspace", "Z");
    (*env)->SetBooleanField(env, obj, field, workspace->is_yarn_workspace);

    field = (*env)->GetFieldID(env, cls, "isPnpmWorkspace", "Z");
    (*env)->SetBooleanField(env, obj, field, workspace->is_pnpm_workspace);

    field = (*env)->GetFieldID(env, cls, "isNxWorkspace", "Z");
    (*env)->SetBooleanField(env, obj, field, workspace->is_nx_workspace);

    field = (*env)->GetFieldID(env, cls, "isRush", "Z");
    (*env)->SetBooleanField(env, obj, field, workspace->is_rush);

    field = (*env)->GetFieldID(env, cls, "hasHoisting", "Z");
    (*env)->SetBooleanField(env, obj, field, workspace->has_hoisting);

    field = (*env)->GetFieldID(env, cls, "hasWorkspacesPrefix", "Z");
    (*env)->SetBooleanField(env, obj, field, workspace->has_workspaces_prefix);

    field = (*env)->GetFieldID(env, cls, "usesNpmWorkspaces", "Z");
    (*env)->SetBooleanField(env, obj, field, workspace->uses_npm_workspaces);

    field = (*env)->GetFieldID(env, cls, "usesChangesets", "Z");
    (*env)->SetBooleanField(env, obj, field, workspace->uses_changesets);

    field = (*env)->GetFieldID(env, cls, "usesTurborepo", "Z");
    (*env)->SetBooleanField(env, obj, field, workspace->uses_turborepo);

    field = (*env)->GetFieldID(env, cls, "hasSharedConfigs", "Z");
    (*env)->SetBooleanField(env, obj, field, workspace->has_shared_configs);

    field = (*env)->GetFieldID(env, cls, "usesConventionalCommits", "Z");
    (*env)->SetBooleanField(env, obj, field, workspace->uses_conventional_commits);

    field = (*env)->GetFieldID(env, cls, "usesGitTags", "Z");
    (*env)->SetBooleanField(env, obj, field, workspace->uses_git_tags);

    // Create and set task groups array
    jclass taskGroupCls = (*env)->FindClass(env,
                                            "com/gdme/webpulseforecast/WebPulseForecastNative$TaskGroup");
    jobjectArray taskGroups = (*env)->NewObjectArray(env, workspace->task_group_count, taskGroupCls, NULL);

    for (int i = 0; i < workspace->task_group_count; i++) {
        jobject group = create_task_group_object(env, &workspace->task_groups[i]);
        if (group) {
            (*env)->SetObjectArrayElement(env, taskGroups, i, group);
            (*env)->DeleteLocalRef(env, group);
        }
    }

    field = (*env)->GetFieldID(env, cls, "taskGroups",
                               "[Lcom/gdme/webpulseforecast/WebPulseForecastNative$TaskGroup;");
    (*env)->SetObjectField(env, obj, field, taskGroups);
    (*env)->DeleteLocalRef(env, taskGroups);

    // Set configuration paths
    field = (*env)->GetFieldID(env, cls, "buildCachePath", "Ljava/lang/String;");
    jstring buildCache = (*env)->NewStringUTF(env, workspace->build_cache_path);
    (*env)->SetObjectField(env, obj, field, buildCache);
    (*env)->DeleteLocalRef(env, buildCache);

    field = (*env)->GetFieldID(env, cls, "tsconfigPath", "Ljava/lang/String;");
    jstring tsconfig = (*env)->NewStringUTF(env, workspace->tsconfig_path);
    (*env)->SetObjectField(env, obj, field, tsconfig);
    (*env)->DeleteLocalRef(env, tsconfig);

    field = (*env)->GetFieldID(env, cls, "eslintConfigPath", "Ljava/lang/String;");
    jstring eslint = (*env)->NewStringUTF(env, workspace->eslint_config_path);
    (*env)->SetObjectField(env, obj, field, eslint);
    (*env)->DeleteLocalRef(env, eslint);

    field = (*env)->GetFieldID(env, cls, "prettierConfigPath", "Ljava/lang/String;");
    jstring prettier = (*env)->NewStringUTF(env, workspace->prettier_config_path);
    (*env)->SetObjectField(env, obj, field, prettier);
    (*env)->DeleteLocalRef(env, prettier);

    field = (*env)->GetFieldID(env, cls, "jestConfigPath", "Ljava/lang/String;");
    jstring jest = (*env)->NewStringUTF(env, workspace->jest_config_path);
    (*env)->SetObjectField(env, obj, field, jest);
    (*env)->DeleteLocalRef(env, jest);

    field = (*env)->GetFieldID(env, cls, "babelConfigPath", "Ljava/lang/String;");
    jstring babel = (*env)->NewStringUTF(env, workspace->babel_config_path);
    (*env)->SetObjectField(env, obj, field, babel);
    (*env)->DeleteLocalRef(env, babel);

    // Set version management info
    field = (*env)->GetFieldID(env, cls, "versionStrategy", "Ljava/lang/String;");
    jstring strategy = (*env)->NewStringUTF(env, workspace->version_strategy);
    (*env)->SetObjectField(env, obj, field, strategy);
    (*env)->DeleteLocalRef(env, strategy);

    field = (*env)->GetFieldID(env, cls, "usesSemanticRelease", "Z");
    (*env)->SetBooleanField(env, obj, field, workspace->uses_semantic_release);

    TRACE("Exiting create_workspace_info_object");
    return obj;
}

// Helper function to create a ProjectType Java object
jobject create_project_type_object(JNIEnv *env, const ProjectType *project) {
    TRACE("Entering create_project_type_object");
    static jclass cls = NULL;
    static jmethodID constructor = NULL;

    if (!project) {
        TRACE("NULL project pointer provided");
        return NULL;
    }

    if (cls == NULL) {
        cls = (*env)->FindClass(env, "com/gdme/webpulseforecast/WebPulseForecastNative$ProjectType");
        if (cls == NULL) {
            TRACE("Failed to find ProjectType class");
            return NULL;
        }
        cls = (*env)->NewGlobalRef(env, cls);
        constructor = (*env)->GetMethodID(env, cls, "<init>", "()V");
        if (constructor == NULL) {
            TRACE("Failed to get constructor");
            return NULL;
        }
    }

    jobject obj = (*env)->NewObject(env, cls, constructor);
    if (obj == NULL) {
        TRACE("Failed to create new ProjectType object");
        return NULL;
    }

    // Set framework info
    jobject frameworkInfo = create_framework_info_object(env, &project->framework_info);
    if (frameworkInfo) {
        jfieldID field = (*env)->GetFieldID(env, cls, "frameworkInfo",
                                            "Lcom/gdme/webpulseforecast/WebPulseForecastNative$FrameworkInfo;");
        (*env)->SetObjectField(env, obj, field, frameworkInfo);
        (*env)->DeleteLocalRef(env, frameworkInfo);
    }

    // Set basic framework field (legacy)
    jfieldID field = (*env)->GetFieldID(env, cls, "framework", "Ljava/lang/String;");
    jstring frameworkString = (*env)->NewStringUTF(env, project->framework);
    (*env)->SetObjectField(env, obj, field, frameworkString);
    (*env)->DeleteLocalRef(env, frameworkString);

    // Set file counts
    field = (*env)->GetFieldID(env, cls, "htmlFileCount", "I");
    (*env)->SetIntField(env, obj, field, project->html_file_count);

    field = (*env)->GetFieldID(env, cls, "cssFileCount", "I");
    (*env)->SetIntField(env, obj, field, project->css_file_count);

    field = (*env)->GetFieldID(env, cls, "jsFileCount", "I");
    (*env)->SetIntField(env, obj, field, project->js_file_count);

    field = (*env)->GetFieldID(env, cls, "jsonFileCount", "I");
    (*env)->SetIntField(env, obj, field, project->json_file_count);

    field = (*env)->GetFieldID(env, cls, "tsFileCount", "I");
    (*env)->SetIntField(env, obj, field, project->ts_file_count);

    field = (*env)->GetFieldID(env, cls, "jsxFileCount", "I");
    (*env)->SetIntField(env, obj, field, project->jsx_file_count);

    field = (*env)->GetFieldID(env, cls, "vueFileCount", "I");
    (*env)->SetIntField(env, obj, field, project->vue_file_count);

    field = (*env)->GetFieldID(env, cls, "xmlFileCount", "I");
    (*env)->SetIntField(env, obj, field, project->xml_file_count);

    field = (*env)->GetFieldID(env, cls, "imageFileCount", "I");
    (*env)->SetIntField(env, obj, field, project->image_file_count);

    // Set component tracking
    field = (*env)->GetFieldID(env, cls, "reactComponentCount", "I");
    (*env)->SetIntField(env, obj, field, project->react_component_count);

    field = (*env)->GetFieldID(env, cls, "customElementCount", "I");
    (*env)->SetIntField(env, obj, field, project->custom_element_count);

    // Set parser results
    // HTML Info
    jobject htmlInfo = create_html_info_object(env, &project->total_html_info);
    if (htmlInfo) {
        field = (*env)->GetFieldID(env, cls, "totalHtmlInfo",
                                   "Lcom/gdme/webpulseforecast/WebPulseForecastNative$HTMLInfo;");
        (*env)->SetObjectField(env, obj, field, htmlInfo);
        (*env)->DeleteLocalRef(env, htmlInfo);
    }

    // CSS Info
    jobject cssInfo = create_css_info_object(env, &project->total_css_info);
    if (cssInfo) {
        field = (*env)->GetFieldID(env, cls, "totalCssInfo",
                                   "Lcom/gdme/webpulseforecast/WebPulseForecastNative$CSSInfo;");
        (*env)->SetObjectField(env, obj, field, cssInfo);
        (*env)->DeleteLocalRef(env, cssInfo);
    }

    // JS Info
    jobject jsInfo = create_js_info_object(env, &project->total_js_info);
    if (jsInfo) {
        field = (*env)->GetFieldID(env, cls, "totalJsInfo",
                                   "Lcom/gdme/webpulseforecast/WebPulseForecastNative$JSInfo;");
        (*env)->SetObjectField(env, obj, field, jsInfo);
        (*env)->DeleteLocalRef(env, jsInfo);
    }

    // JSON Info
    jobject jsonInfo = create_json_info_object(env, &project->total_json_info);
    if (jsonInfo) {
        field = (*env)->GetFieldID(env, cls, "totalJsonInfo",
                                   "Lcom/gdme/webpulseforecast/WebPulseForecastNative$JSONInfo;");
        (*env)->SetObjectField(env, obj, field, jsonInfo);
        (*env)->DeleteLocalRef(env, jsonInfo);
    }

    // Create and set external resources array
    jclass externalResourceCls = (*env)->FindClass(env,
                                                   "com/gdme/webpulseforecast/WebPulseForecastNative$ExternalResource");
    jobjectArray externalResources = (*env)->NewObjectArray(env,
                                                            project->external_resource_count, externalResourceCls, NULL);

    for (int i = 0; i < project->external_resource_count; i++) {
        jobject resource = create_external_resource_object(env, &project->external_resources[i]);
        (*env)->SetObjectArrayElement(env, externalResources, i, resource);
        (*env)->DeleteLocalRef(env, resource);
    }

    field = (*env)->GetFieldID(env, cls, "externalResources",
                               "[Lcom/gdme/webpulseforecast/WebPulseForecastNative$ExternalResource;");
    (*env)->SetObjectField(env, obj, field, externalResources);
    (*env)->DeleteLocalRef(env, externalResources);

    // Set workspace info
    if (project->is_monorepo) {
        jobject workspaceInfo = create_workspace_info_object(env, &project->workspace);
        if (workspaceInfo) {
            field = (*env)->GetFieldID(env, cls, "workspaceInfo",
                                       "Lcom/gdme/webpulseforecast/WebPulseForecastNative$WorkspaceInfo;");
            (*env)->SetObjectField(env, obj, field, workspaceInfo);
            (*env)->DeleteLocalRef(env, workspaceInfo);
        }
    }

    // Set dependencies
    jobject dependencies = create_dependency_list_object(env, &project->dependencies);
    if (dependencies) {
        field = (*env)->GetFieldID(env, cls, "dependencies",
                                   "Lcom/gdme/webpulseforecast/WebPulseForecastNative$DependencyList;");
        (*env)->SetObjectField(env, obj, field, dependencies);
        (*env)->DeleteLocalRef(env, dependencies);
    }

    // Set module paths
    jclass stringCls = (*env)->FindClass(env, "java/lang/String");
    jobjectArray modulePaths = (*env)->NewObjectArray(env, project->module_path_count, stringCls, NULL);

    for (int i = 0; i < project->module_path_count; i++) {
        jstring path = (*env)->NewStringUTF(env, project->module_paths[i]);
        (*env)->SetObjectArrayElement(env, modulePaths, i, path);
        (*env)->DeleteLocalRef(env, path);
    }

    field = (*env)->GetFieldID(env, cls, "modulePaths", "[Ljava/lang/String;");
    (*env)->SetObjectField(env, obj, field, modulePaths);
    (*env)->DeleteLocalRef(env, modulePaths);

    // Set flags
    field = (*env)->GetFieldID(env, cls, "usesCommonjs", "Z");
    (*env)->SetBooleanField(env, obj, field, project->uses_commonjs);

    field = (*env)->GetFieldID(env, cls, "usesEsmodules", "Z");
    (*env)->SetBooleanField(env, obj, field, project->uses_esmodules);

    field = (*env)->GetFieldID(env, cls, "hasWebpack", "Z");
    (*env)->SetBooleanField(env, obj, field, project->has_webpack);

    field = (*env)->GetFieldID(env, cls, "hasVite", "Z");
    (*env)->SetBooleanField(env, obj, field, project->has_vite);

    field = (*env)->GetFieldID(env, cls, "hasBabel", "Z");
    (*env)->SetBooleanField(env, obj, field, project->has_babel);

    field = (*env)->GetFieldID(env, cls, "hasCi", "Z");
    (*env)->SetBooleanField(env, obj, field, project->has_ci);

    field = (*env)->GetFieldID(env, cls, "hasEnvConfig", "Z");
    (*env)->SetBooleanField(env, obj, field, project->has_env_config);

    field = (*env)->GetFieldID(env, cls, "hasTypescript", "Z");
    (*env)->SetBooleanField(env, obj, field, project->has_typescript);

    field = (*env)->GetFieldID(env, cls, "isMonorepo", "Z");
    (*env)->SetBooleanField(env, obj, field, project->is_monorepo);

    TRACE("Exiting create_project_type_object");
    return obj;
}

// Helper function to create a ResourceEstimation Java object
jobject create_resource_estimation_object(JNIEnv *env, const ResourceEstimation *estimation) {
    TRACE("Entering jobject create_resource_estimation_object");
    jclass cls = (*env)->FindClass(env, "com/gdme/webpulseforecast/WebPulseForecastNative$ResourceEstimation");
    jmethodID constructor = (*env)->GetMethodID(env, cls, "<init>", "()V");
    jobject obj = (*env)->NewObject(env, cls, constructor);

    // Set fields
    (*env)->SetLongField(env, obj, (*env)->GetFieldID(env, cls, "jsHeapSize", "J"), estimation->js_heap_size);
    (*env)->SetLongField(env, obj, (*env)->GetFieldID(env, cls, "transferredData", "J"), estimation->transferred_data);
    (*env)->SetLongField(env, obj, (*env)->GetFieldID(env, cls, "resourceSize", "J"), estimation->resource_size);
    (*env)->SetIntField(env, obj, (*env)->GetFieldID(env, cls, "domContentLoaded", "I"), estimation->dom_content_loaded);
    (*env)->SetIntField(env, obj, (*env)->GetFieldID(env, cls, "largestContentfulPaint", "I"), estimation->largest_contentful_paint);

    TRACE("Exiting jobject create_resource_estimation_object");
    return obj;
}

JNIEXPORT jobject JNICALL Java_com_gdme_webpulseforecast_WebPulseForecastNative_analyzeProjectType
        (JNIEnv *env, jobject obj, jstring projectPath) {
    TRACE("Entering Java_com_gdme_webpulseforecast_WebPulseForecastNative_analyzeProjectType");
    const char *path = (*env)->GetStringUTFChars(env, projectPath, 0);

    // Call analyze_project_type, which now returns a pointer
    ProjectType *project = analyze_project_type(path);

    (*env)->ReleaseStringUTFChars(env, projectPath, path);

    if (!project) {
        // Handle the case where analyze_project_type failed
        return NULL;
    }

    jobject result = create_project_type_object(env, project);

    // Free the memory allocated for ProjectType
    free(project);

    TRACE("Exiting Java_com_gdme_webpulseforecast_WebPulseForecastNative_analyzeProjectType");
    return result;
}

JNIEXPORT jobject JNICALL Java_com_gdme_webpulseforecast_WebPulseForecastNative_estimateResources
        (JNIEnv *env, jobject obj, jobject projectType) {

    TRACE("Entering Java_com_gdme_webpulseforecast_WebPulseForecastNative_estimateResources");

    // Extract fields from the ProjectType Java object
    jclass projectTypeCls = (*env)->GetObjectClass(env, projectType);

    jfieldID frameworkField = (*env)->GetFieldID(env, projectTypeCls, "framework", "Ljava/lang/String;");
    jfieldID htmlFileCountField = (*env)->GetFieldID(env, projectTypeCls, "htmlFileCount", "I");
    jfieldID cssFileCountField = (*env)->GetFieldID(env, projectTypeCls, "cssFileCount", "I");
    jfieldID jsFileCountField = (*env)->GetFieldID(env, projectTypeCls, "jsFileCount", "I");
    jfieldID jsonFileCountField = (*env)->GetFieldID(env, projectTypeCls, "jsonFileCount", "I");
    jfieldID imageFileCountField = (*env)->GetFieldID(env, projectTypeCls, "imageFileCount", "I");

    // Get the actual values from the Java object
    jstring framework = (jstring) (*env)->GetObjectField(env, projectType, frameworkField);
    const char *cFramework = (*env)->GetStringUTFChars(env, framework, 0);

    jint htmlFileCount = (*env)->GetIntField(env, projectType, htmlFileCountField);
    jint cssFileCount = (*env)->GetIntField(env, projectType, cssFileCountField);
    jint jsFileCount = (*env)->GetIntField(env, projectType, jsFileCountField);
    jint jsonFileCount = (*env)->GetIntField(env, projectType, jsonFileCountField);
    jint imageFileCount = (*env)->GetIntField(env, projectType, imageFileCountField);

    // Allocate ProjectType structure dynamically to reduce stack usage
    ProjectType *project = (ProjectType *) malloc(sizeof(ProjectType));
    if (project == NULL) {
        // Handle memory allocation failure
        (*env)->ReleaseStringUTFChars(env, framework, cFramework);
        return NULL; // Return NULL if allocation fails
    }

    // Initialize the ProjectType structure to avoid garbage values
    memset(project, 0, sizeof(ProjectType)); // Ensure all fields are set to 0 initially

    // Safely copy and null-terminate the framework string
    strncpy(project->framework, cFramework, sizeof(project->framework) - 1);
    project->framework[sizeof(project->framework) - 1] = '\0'; // Null-terminate

    // Set values from the Java object into the C structure
    project->html_file_count = htmlFileCount;
    project->css_file_count = cssFileCount;
    project->js_file_count = jsFileCount;
    project->json_file_count = jsonFileCount;
    project->image_file_count = imageFileCount;

    // Log or trace extracted values for debugging
    TRACE("Extracted values: framework=%s, htmlFileCount=%d, cssFileCount=%d, jsFileCount=%d, jsonFileCount=%d, imageFileCount=%d",
          project->framework, project->html_file_count, project->css_file_count, project->js_file_count,
          project->json_file_count, project->image_file_count);

    // Release Java String after copying
    (*env)->ReleaseStringUTFChars(env, framework, cFramework);

    // Call the C function to estimate resources
    ResourceEstimation estimation = estimate_resources(project);

    // Log resource estimation results
    TRACE("Estimated resources: js_heap_size=%lld, transferred_data=%lld, resource_size=%lld, dom_content_loaded=%d, lcp=%d",
          estimation.js_heap_size, estimation.transferred_data, estimation.resource_size,
          estimation.dom_content_loaded, estimation.largest_contentful_paint);

    // Convert ResourceEstimation back into a Java object
    jclass estimationCls = (*env)->FindClass(env, "com/gdme/webpulseforecast/WebPulseForecastNative$ResourceEstimation");
    jmethodID constructor = (*env)->GetMethodID(env, estimationCls, "<init>", "()V");
    jobject estimationObj = (*env)->NewObject(env, estimationCls, constructor);

    // Set the fields back in Java object
    (*env)->SetLongField(env, estimationObj, (*env)->GetFieldID(env, estimationCls, "jsHeapSize", "J"), estimation.js_heap_size);
    (*env)->SetLongField(env, estimationObj, (*env)->GetFieldID(env, estimationCls, "transferredData", "J"), estimation.transferred_data);
    (*env)->SetLongField(env, estimationObj, (*env)->GetFieldID(env, estimationCls, "resourceSize", "J"), estimation.resource_size);
    (*env)->SetIntField(env, estimationObj, (*env)->GetFieldID(env, estimationCls, "domContentLoaded", "I"), estimation.dom_content_loaded);
    (*env)->SetIntField(env, estimationObj, (*env)->GetFieldID(env, estimationCls, "largestContentfulPaint", "I"), estimation.largest_contentful_paint);

    // Free dynamically allocated memory for ProjectType
    free(project);

    TRACE("Exiting Java_com_gdme_webpulseforecast_WebPulseForecastNative_estimateResources");
    return estimationObj;
}

JNIEXPORT jdouble JNICALL Java_com_gdme_webpulseforecast_WebPulseForecastNative_calculatePerformanceImpact
        (JNIEnv *env, jobject obj, jobject projectType) {
    TRACE("Entering Java_com_gdme_webpulseforecast_WebPulseForecastNative_calculatePerformanceImpact");

    if (!projectType) {
        TRACE("NULL projectType provided");
        return 0.0;
    }

    // Allocate memory for ProjectType
    ProjectType *project = (ProjectType *) malloc(sizeof(ProjectType));
    if (!project) {
        TRACE("Memory allocation for ProjectType failed");
        return 0.0;
    }

    // Initialize the structure to avoid any garbage values
    memset(project, 0, sizeof(ProjectType));

    // Extract all project information using our helper function
    extract_project_type(env, projectType, project);

    // Log key metrics for debugging
    TRACE("Framework: %s", project->framework);
    TRACE("Total HTML tags: %d", project->total_html_info.tag_count);
    TRACE("Total JS functions: %d", project->total_js_info.function_count);
    TRACE("Total CSS rules: %d", project->total_css_info.rule_count);
    TRACE("Total JSON objects: %d", project->total_json_info.object_count);
    TRACE("Image files: %d", project->image_file_count);
    TRACE("React hooks count: %d", project->framework_info.react_hooks_count);
    TRACE("Custom elements: %d", project->custom_element_count);
    TRACE("External resources: %d", project->external_resource_count);
    TRACE("Is monorepo: %d", project->is_monorepo);

    // Framework info logging
    TRACE("Has React: %d", project->framework_info.has_react);
    TRACE("Has Vue: %d", project->framework_info.has_vue);
    TRACE("Has Angular: %d", project->framework_info.has_angular);
    TRACE("Has Svelte: %d", project->framework_info.has_svelte);
    TRACE("Has Next.js: %d", project->framework_info.has_nextjs);
    TRACE("Has Nuxt.js: %d", project->framework_info.has_nuxtjs);
    TRACE("Uses TypeScript: %d", project->framework_info.uses_typescript);

    // Calculate performance impact
    jdouble impact = calculate_performance_impact(project);
    TRACE("Calculated performance impact: %f", impact);

    // Clean up
    free(project);

    TRACE("Exiting Java_com_gdme_webpulseforecast_WebPulseForecastNative_calculatePerformanceImpact");
    return impact;
}

JNIEXPORT jstring JNICALL Java_com_gdme_webpulseforecast_WebPulseForecastNative_getResourceUsageDisplay
        (JNIEnv *env, jobject obj, jobject resourceEstimation) {
    TRACE("Entering Java_com_gdme_webpulseforecast_WebPulseForecastNative_getResourceUsageDisplay");

    // Extract fields from ResourceEstimation Java object
    jclass estimationCls = (*env)->GetObjectClass(env, resourceEstimation);
    jfieldID jsHeapSizeField = (*env)->GetFieldID(env, estimationCls, "jsHeapSize", "J");
    jfieldID transferredDataField = (*env)->GetFieldID(env, estimationCls, "transferredData", "J");
    jfieldID resourceSizeField = (*env)->GetFieldID(env, estimationCls, "resourceSize", "J");
    jfieldID domContentLoadedField = (*env)->GetFieldID(env, estimationCls, "domContentLoaded", "I");
    jfieldID lcpField = (*env)->GetFieldID(env, estimationCls, "largestContentfulPaint", "I");

    // Extract the actual values from the Java object
    ResourceEstimation estimation = {0};
    estimation.js_heap_size = (*env)->GetLongField(env, resourceEstimation, jsHeapSizeField);
    estimation.transferred_data = (*env)->GetLongField(env, resourceEstimation, transferredDataField);
    estimation.resource_size = (*env)->GetLongField(env, resourceEstimation, resourceSizeField);
    estimation.dom_content_loaded = (*env)->GetIntField(env, resourceEstimation, domContentLoadedField);
    estimation.largest_contentful_paint = (*env)->GetIntField(env, resourceEstimation, lcpField);

    // Format the string as before
    char buffer[1024];
    snprintf(buffer, sizeof(buffer),
             "<html>JS Heap Size: %.2f MB<br>Transferred Data: %.2f KB<br>Resource Size: %.2f KB<br>DOMContentLoaded: %d ms<br>Largest Contentful Paint: %d ms</html>",
             estimation.js_heap_size / 1000000.0,
             estimation.transferred_data / 1000.0,
             estimation.resource_size / 1000.0,
             estimation.dom_content_loaded,
             estimation.largest_contentful_paint);

    // Create a Java string from the formatted buffer
    jstring result = create_jstring(env, buffer);

    TRACE("Exiting Java_com_gdme_webpulseforecast_WebPulseForecastNative_getResourceUsageDisplay");
    return result;
}

JNIEXPORT jobjectArray JNICALL Java_com_gdme_webpulseforecast_WebPulseForecastNative_getPotentialIssues
        (JNIEnv *env, jobject obj, jobject projectType) {
    TRACE("Entering Java_com_gdme_webpulseforecast_WebPulseForecastNative_getPotentialIssues");

    // Extract fields from the ProjectType Java object
    jclass projectTypeCls = (*env)->GetObjectClass(env, projectType);

    // Get the potentialIssueCount field from the ProjectType Java object
    jfieldID issueCountField = (*env)->GetFieldID(env, projectTypeCls, "potentialIssueCount", "I");
    jint potentialIssueCount = (*env)->GetIntField(env, projectType, issueCountField);

    // Get the potentialIssues array from the ProjectType Java object
    jfieldID potentialIssuesField = (*env)->GetFieldID(env, projectTypeCls, "potentialIssues", "[Lcom/gdme/webpulseforecast/WebPulseForecastNative$PotentialIssue;");
    jobjectArray potentialIssuesArray = (jobjectArray)(*env)->GetObjectField(env, projectType, potentialIssuesField);

    // Create a Java String array to hold the issues
    jclass stringClass = (*env)->FindClass(env, "java/lang/String");
    jobjectArray issuesArray = (*env)->NewObjectArray(env, potentialIssueCount, stringClass, NULL);

    // Iterate through potential issues in the Java array and add them to the string array
    for (int i = 0; i < potentialIssueCount; i++) {
        // Get the PotentialIssue object at index `i`
        jobject potentialIssueObj = (*env)->GetObjectArrayElement(env, potentialIssuesArray, i);

        // Get the description field from the PotentialIssue object
        jclass potentialIssueCls = (*env)->GetObjectClass(env, potentialIssueObj);
        jfieldID descriptionField = (*env)->GetFieldID(env, potentialIssueCls, "description", "Ljava/lang/String;");
        jstring description = (jstring)(*env)->GetObjectField(env, potentialIssueObj, descriptionField);

        // Add the description string to the issuesArray
        (*env)->SetObjectArrayElement(env, issuesArray, i, description);

        // Clean up local references
        (*env)->DeleteLocalRef(env, potentialIssueObj);
        (*env)->DeleteLocalRef(env, description);
    }

    TRACE("Exiting Java_com_gdme_webpulseforecast_WebPulseForecastNative_getPotentialIssues");
    return issuesArray;
}