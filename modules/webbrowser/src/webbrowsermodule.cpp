/*********************************************************************************
 *
 * Inviwo - Interactive Visualization Workshop
 *
 * Copyright (c) 2018-2020 Inviwo Foundation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *********************************************************************************/

#include <modules/webbrowser/webbrowsermodule.h>
#include <modules/webbrowser/processors/webbrowserprocessor.h>
#include <modules/webbrowser/webbrowserapp.h>

#include <modules/json/io/json/boolpropertyjsonconverter.h>
#include <modules/json/io/json/buttonpropertyjsonconverter.h>
#include <modules/json/io/json/directorypropertyjsonconverter.h>
#include <modules/json/io/json/filepropertyjsonconverter.h>
#include <modules/json/io/json/minmaxpropertyjsonconverter.h>
#include <modules/json/io/json/optionpropertyjsonconverter.h>
#include <modules/json/io/json/ordinalpropertyjsonconverter.h>
#include <modules/json/io/json/templatepropertyjsonconverter.h>

#include <inviwo/dataframe/io/json/dataframepropertyjsonconverter.h>

#include <inviwo/core/util/filesystem.h>
#include <inviwo/core/util/settings/systemsettings.h>

#include <modules/opengl/shader/shadermanager.h>

// Autogenerated
#include <modules/webbrowser/shader_resources.h>

#include <warn/push>
#include <warn/ignore/all>
#include "include/cef_app.h"
#include "include/cef_command_line.h"
#include "include/cef_parser.h"
#include <warn/pop>

namespace inviwo {

struct OrdinalCEFWidgetReghelper {
    template <typename T>
    auto operator()(WebBrowserModule& m) {
        using PropertyType = OrdinalProperty<T>;
        m.registerPropertyWidgetCEF<PropertyWidgetCEF, PropertyType>();
    }
};

struct MinMaxCEFWidgetReghelper {
    template <typename T>
    auto operator()(WebBrowserModule& m) {
        using PropertyType = MinMaxProperty<T>;
        m.registerPropertyWidgetCEF<PropertyWidgetCEF, PropertyType>();
    }
};

struct OptionCEFWidgetReghelper {
    template <typename T>
    auto operator()(WebBrowserModule& m) {
        using PropertyType = TemplateOptionProperty<T>;
        m.registerPropertyWidgetCEF<PropertyWidgetCEF, PropertyType>();
    }
};

WebBrowserModule::WebBrowserModule(InviwoApplication* app)
    : InviwoModule(app, "WebBrowser")
    // Call 60 times per second
    , doChromiumWork_(Timer::Milliseconds(1000 / 60), []() { CefDoMessageLoopWork(); }) {

    // Register widgets
    registerPropertyWidgetCEF<PropertyWidgetCEF, BoolProperty>();
    registerPropertyWidgetCEF<PropertyWidgetCEF, ButtonProperty>();
    registerPropertyWidgetCEF<PropertyWidgetCEF, FileProperty>();
    registerPropertyWidgetCEF<PropertyWidgetCEF, DirectoryProperty>();
    registerPropertyWidgetCEF<PropertyWidgetCEF, StringProperty>();

    registerPropertyWidgetCEF<PropertyWidgetCEF, DataFrameColumnProperty>();

    // Register ordinal property widgets
    using OrdinalTypes =
        std::tuple<float, vec2, vec3, vec4, mat2, mat3, mat4, double, dvec2, dvec3, dvec4, dmat2,
                   dmat3, dmat4, int, ivec2, ivec3, ivec4, glm::i64, unsigned int, uvec2, uvec3,
                   uvec4, size_t, size2_t, size3_t, size4_t, glm::fquat, glm::dquat>;

    using ScalarTypes = std::tuple<float, double, int, glm::i64, size_t>;
    util::for_each_type<OrdinalTypes>{}(OrdinalCEFWidgetReghelper{}, *this);

    // Register MinMaxProperty widgets
    util::for_each_type<ScalarTypes>{}(MinMaxCEFWidgetReghelper{}, *this);

    // Register option property widgets
    using OptionTypes = std::tuple<unsigned int, int, size_t, float, double, std::string>;
    util::for_each_type<OptionTypes>{}(OptionCEFWidgetReghelper{}, *this);

    if (!app->getSystemSettings().enablePickingProperty_) {
        LogInfo(
            "Enabling picking system setting since it is required for interaction "
            "(View->Settings->System settings->Enable picking).");
        app->getSystemSettings().enablePickingProperty_.set(true);
    }
    // CEF initialization
    // Specify the path for the sub-process executable.
    auto exeExtension = filesystem::getFileExtension(filesystem::getExecutablePath());
    // Assume that inviwo_web_helper is next to the main executable
    auto exeDirectory = filesystem::getFileDirectory(filesystem::getExecutablePath());

    auto locale = app->getUILocale().name();
    if (locale == "C") {
        // Crash when default locale "C" is used. Reproduce with GLFWMinimum application
        locale = std::locale("en_US").name();
    }

    void* sandbox_info = NULL;  // Windows specific

#ifdef __APPLE__  // Mac specific

    // Find CEF framework and helper app in
    // exe.app/Contents/Frameworks directory first
    auto cefParentDir = filesystem::getCanonicalPath(exeDirectory + std::string("/.."));
    auto frameworkDirectory = cefParentDir + "/Frameworks/Chromium Embedded Framework.framework";
    auto frameworkPath = frameworkDirectory + "/Chromium Embedded Framework";
    // Load the CEF framework library at runtime instead of linking directly
    // as required by the macOS sandbox implementation.
    if (!cefLib_.LoadInMain()) {
        throw ModuleInitException("Could not find Chromium Embedded Framework.framework: " +
                                  frameworkPath);
    }

    CefMainArgs args(app->getCommandLineParser().getARGC(), app->getCommandLineParser().getARGV());
    CefSettings settings;
    // CefString(&settings.framework_dir_path).FromASCII((frameworkDirectory).c_str());
    // Crashes if not set and non-default locale is used
    CefString(&settings.locales_dir_path)
        .FromASCII((frameworkDirectory + std::string("/Resources")).c_str());
    CefString(&settings.resources_dir_path)
        .FromASCII((frameworkDirectory + std::string("/Resources")).c_str());
    // Locale returns "en_US.UFT8" but "en.UTF8" is needed by CEF
    auto startErasePos = locale.find('_');
    if (startErasePos != std::string::npos) {
        locale.erase(startErasePos, locale.find('.') - startErasePos);
    }

#else
    CefMainArgs args;
    CefSettings settings;
    // Non-mac systems uses a single helper executable so here we can specify name
    // Linux will have empty extension
    auto subProcessExecutable = fmt::format("{}/{}{}{}", exeDirectory, "cef_web_helper",
                                            exeExtension.empty() ? "" : ".", exeExtension);
    if (!filesystem::fileExists(subProcessExecutable)) {
        throw ModuleInitException("Could not find web helper executable:" + subProcessExecutable);
    }

    // Necessary to run helpers in separate sub-processes
    // Needed since we do not want to edit the "main" function
    CefString(&settings.browser_subprocess_path).FromASCII(subProcessExecutable.c_str());
#endif

#ifdef WIN32
    // Enable High-DPI support on Windows 7 or newer.
    CefEnableHighDPISupport();
#if defined(CEF_USE_SANDBOX)
    // Manage the life span of the sandbox information object. This is necessary
    // for sandbox support on Windows. See cef_sandbox_win.h for complete details.
    CefScopedSandboxInfo scoped_sandbox;
    sandbox_info = scoped_sandbox.sandbox_info();
#endif
#endif
    // When generating projects with CMake the CEF_USE_SANDBOX value will be defined
    // automatically. Pass -DUSE_SANDBOX=OFF to the CMake command-line to disable
    // use of the sandbox.
#if !defined(CEF_USE_SANDBOX)
    settings.no_sandbox = true;
#endif
    // checkout detailed settings options
    // http://magpcss.org/ceforum/apidocs/projects/%28default%29/_cef_settings_t.html nearly all
    // the settings can be set via args too.
    settings.multi_threaded_message_loop = false;  // not supported, except windows

    // We want to use off-screen rendering
    settings.windowless_rendering_enabled = true;

    // Let the Inviwo application (Qt/GLFW) handle operating system event processing
    // instead of CEF. Setting external message pump to false will cause mouse events
    // to be processed in CefDoMessageLoopWork() instead of in the Qt application loop.
    settings.external_message_pump = true;

    CefString(&settings.locale).FromASCII(locale.c_str());

    // Optional implementation of the CefApp interface.
    CefRefPtr<WebBrowserApp> browserApp(new WebBrowserApp);

    bool result = CefInitialize(args, settings, browserApp, sandbox_info);

    if (!result) {
        throw ModuleInitException("Failed to initialize Chromium Embedded Framework");
    }

    // Add a directory to the search path of the Shadermanager
    webbrowser::addShaderResources(ShaderManager::getPtr(), {getPath(ModulePath::GLSL)});
    // ShaderManager::getPtr()->addShaderSearchPath(getPath(ModulePath::GLSL));

    // Register objects that can be shared with the rest of inviwo here:

    // Processors
    registerProcessor<WebBrowserProcessor>();

    doChromiumWork_.start();
}

WebBrowserModule::~WebBrowserModule() {
    // Stop message pumping and make sure that app has finished processing before CefShutdown
    doChromiumWork_.stop();
    app_->waitForPool();
    CefShutdown();
}

std::string WebBrowserModule::getDataURI(const std::string& data, const std::string& mime_type) {
    return "data:" + mime_type + ";base64," +
           CefURIEncode(CefBase64Encode(data.data(), data.size()), false).ToString();
}

std::string WebBrowserModule::getCefErrorString(cef_errorcode_t code) {
    switch (code) {
        case ERR_NONE:
            return "ERR_NONE";
        case ERR_FAILED:
            return "ERR_FAILED";
        case ERR_ABORTED:
            return "ERR_ABORTED";
        case ERR_INVALID_ARGUMENT:
            return "ERR_INVALID_ARGUMENT";
        case ERR_INVALID_HANDLE:
            return "ERR_INVALID_HANDLE";
        case ERR_FILE_NOT_FOUND:
            return "ERR_FILE_NOT_FOUND";
        case ERR_TIMED_OUT:
            return "ERR_TIMED_OUT";
        case ERR_FILE_TOO_BIG:
            return "ERR_FILE_TOO_BIG";
        case ERR_UNEXPECTED:
            return "ERR_UNEXPECTED";
        case ERR_ACCESS_DENIED:
            return "ERR_ACCESS_DENIED";
        case ERR_NOT_IMPLEMENTED:
            return "ERR_NOT_IMPLEMENTED";
        case ERR_CONNECTION_CLOSED:
            return "ERR_CONNECTION_CLOSED";
        case ERR_CONNECTION_RESET:
            return "ERR_CONNECTION_RESET";
        case ERR_CONNECTION_REFUSED:
            return "ERR_CONNECTION_REFUSED";
        case ERR_CONNECTION_ABORTED:
            return "ERR_CONNECTION_ABORTED";
        case ERR_CONNECTION_FAILED:
            return "ERR_CONNECTION_FAILED";
        case ERR_NAME_NOT_RESOLVED:
            return "ERR_NAME_NOT_RESOLVED";
        case ERR_INTERNET_DISCONNECTED:
            return "ERR_INTERNET_DISCONNECTED";
        case ERR_SSL_PROTOCOL_ERROR:
            return "ERR_SSL_PROTOCOL_ERROR";
        case ERR_ADDRESS_INVALID:
            return "ERR_ADDRESS_INVALID";
        case ERR_ADDRESS_UNREACHABLE:
            return "ERR_ADDRESS_UNREACHABLE";
        case ERR_SSL_CLIENT_AUTH_CERT_NEEDED:
            return "ERR_SSL_CLIENT_AUTH_CERT_NEEDED";
        case ERR_TUNNEL_CONNECTION_FAILED:
            return "ERR_TUNNEL_CONNECTION_FAILED";
        case ERR_NO_SSL_VERSIONS_ENABLED:
            return "ERR_NO_SSL_VERSIONS_ENABLED";
        case ERR_SSL_VERSION_OR_CIPHER_MISMATCH:
            return "ERR_SSL_VERSION_OR_CIPHER_MISMATCH";
        case ERR_SSL_RENEGOTIATION_REQUESTED:
            return "ERR_SSL_RENEGOTIATION_REQUESTED";
        case ERR_CERT_COMMON_NAME_INVALID:
            return "ERR_CERT_COMMON_NAME_INVALID";
        case ERR_CERT_DATE_INVALID:
            return "ERR_CERT_DATE_INVALID";
        case ERR_CERT_AUTHORITY_INVALID:
            return "ERR_CERT_AUTHORITY_INVALID";
        case ERR_CERT_CONTAINS_ERRORS:
            return "ERR_CERT_CONTAINS_ERRORS";
        case ERR_CERT_NO_REVOCATION_MECHANISM:
            return "ERR_CERT_NO_REVOCATION_MECHANISM";
        case ERR_CERT_UNABLE_TO_CHECK_REVOCATION:
            return "ERR_CERT_UNABLE_TO_CHECK_REVOCATION";
        case ERR_CERT_REVOKED:
            return "ERR_CERT_REVOKED";
        case ERR_CERT_INVALID:
            return "ERR_CERT_INVALID";
        case ERR_CERT_END:
            return "ERR_CERT_END";
        case ERR_INVALID_URL:
            return "ERR_INVALID_URL";
        case ERR_DISALLOWED_URL_SCHEME:
            return "ERR_DISALLOWED_URL_SCHEME";
        case ERR_UNKNOWN_URL_SCHEME:
            return "ERR_UNKNOWN_URL_SCHEME";
        case ERR_TOO_MANY_REDIRECTS:
            return "ERR_TOO_MANY_REDIRECTS";
        case ERR_UNSAFE_REDIRECT:
            return "ERR_UNSAFE_REDIRECT";
        case ERR_UNSAFE_PORT:
            return "ERR_UNSAFE_PORT";
        case ERR_INVALID_RESPONSE:
            return "ERR_INVALID_RESPONSE";
        case ERR_INVALID_CHUNKED_ENCODING:
            return "ERR_INVALID_CHUNKED_ENCODING";
        case ERR_METHOD_NOT_SUPPORTED:
            return "ERR_METHOD_NOT_SUPPORTED";
        case ERR_UNEXPECTED_PROXY_AUTH:
            return "ERR_UNEXPECTED_PROXY_AUTH";
        case ERR_EMPTY_RESPONSE:
            return "ERR_EMPTY_RESPONSE";
        case ERR_RESPONSE_HEADERS_TOO_BIG:
            return "ERR_RESPONSE_HEADERS_TOO_BIG";
        case ERR_CACHE_MISS:
            return "ERR_CACHE_MISS";
        case ERR_INSECURE_RESPONSE:
            return "ERR_INSECURE_RESPONSE";
        default:
            return "UNKNOWN";
    }
}
}  // namespace inviwo
