#include <WPE/WebKit.h>
#include <WPE/WebKit/WKCookieManagerSoup.h>

#include <unistd.h>
#include <cstdio>
#include <cstring>
#include <glib.h>
#include <initializer_list>

GMainLoop* loop;

#define DEBUG(str) g_print("[debug]" str "\n");

void rstrip(char *string) {
    int l;
    if (!string)
        return;
    l = strlen(string) - 1;
    while (g_ascii_isspace(string[l]) && l >= 0) {
        string[l--] = 0;
    }
}

void lstrip(char *string) {
    int i, l;
    if (!string)
        return;
    l = strlen(string);
    while (g_ascii_isspace(string[(i = 0)])) {
        while(i++ < l) {
            string[i-1] = string[i];
        }
    }
}

void strip(char *string) {
    lstrip(string);
    rstrip(string);
}

static gboolean
io_callback (GIOChannel * io, GIOCondition condition, gpointer data)
{
    GError *error = NULL;
    gchar *value;
    char *url;

    WKPageRef* webView = (WKPageRef *)data;
    
    switch (g_io_channel_read_line (io, &value, NULL, NULL, &error)) {

    case G_IO_STATUS_NORMAL:
        
        if (!g_ascii_strncasecmp("LOAD ", value, 5)) {
            url = value + 5;
            strip(url);
            auto wkUrl = WKURLCreateWithUTF8CString(url);
            WKPageLoadURL(*webView, wkUrl);
            WKRelease(wkUrl);
            g_print("OK\n");
        } else if (!g_ascii_strncasecmp("EXIT", value, 4)) {
            g_main_loop_quit(loop);
            g_print("OK\n");
        } else {
            g_print("ERROR: Invalid command\n");
        }
        g_free(value);
        return TRUE;

    case G_IO_STATUS_ERROR:
        g_print("ERROR: IO error: %s\n", error->message);
        g_error_free (error);

        return FALSE;

    case G_IO_STATUS_EOF:
        g_print("ERROR: No input data available\n");
        return TRUE;

    case G_IO_STATUS_AGAIN:
        return TRUE;

    default:
        g_return_val_if_reached (FALSE);
        break;
    }

    return FALSE;
}

void setup_stdin_io(WKPageRef webView) {
    GIOChannel *io = NULL;
    guint io_watch_id = 0;

    /* standard input callback */
    io = g_io_channel_unix_new (STDIN_FILENO);
    io_watch_id = g_io_add_watch (io, G_IO_IN, io_callback, &webView);
    g_io_channel_unref (io);
}


WKPageNavigationClientV0 s_navigationClient = {
    { 0, nullptr },
    // decidePolicyForNavigationAction
    [](WKPageRef, WKNavigationActionRef, WKFramePolicyListenerRef listener, WKTypeRef, const void*) {
        WKFramePolicyListenerUse(listener);
    },
    // decidePolicyForNavigationResponse
    [](WKPageRef, WKNavigationResponseRef, WKFramePolicyListenerRef listener, WKTypeRef, const void*) {
        WKFramePolicyListenerUse(listener);
    },
    nullptr, // decidePolicyForPluginLoad
    nullptr, // didStartProvisionalNavigation
    nullptr, // didReceiveServerRedirectForProvisionalNavigation
    nullptr, // didFailProvisionalNavigation
    nullptr, // didCommitNavigation
    nullptr, // didFinishNavigation
    nullptr, // didFailNavigation
    nullptr, // didFailProvisionalLoadInSubframe
    nullptr, // didFinishDocumentLoad
    nullptr, // didSameDocumentNavigation
    nullptr, // renderingProgressDidChange
    nullptr, // canAuthenticateAgainstProtectionSpace
    nullptr, // didReceiveAuthenticationChallenge
    nullptr, // webProcessDidCrash
    nullptr, // copyWebCryptoMasterKey
    nullptr, // didBeginNavigationGesture
    nullptr, // willEndNavigationGesture
    nullptr, // didEndNavigationGesture
    nullptr, // didRemoveNavigationGestureSnapshot
};


int main(int argc, char* argv[])
{
    DEBUG("WPELauncher start");

    loop = g_main_loop_new(nullptr, FALSE);

    auto contextConfiguration = WKContextConfigurationCreate();

    gchar *wpeStoragePath = g_build_filename(g_get_user_cache_dir(), "wpe", "local-storage", nullptr);
    g_mkdir_with_parents(wpeStoragePath, 0700);
    auto storageDirectory = WKStringCreateWithUTF8CString(wpeStoragePath);
    g_free(wpeStoragePath);
    WKContextConfigurationSetLocalStorageDirectory(contextConfiguration, storageDirectory);

    gchar *wpeDiskCachePath = g_build_filename(g_get_user_cache_dir(), "wpe", "disk-cache", nullptr);
    g_mkdir_with_parents(wpeDiskCachePath, 0700);
    auto diskCacheDirectory = WKStringCreateWithUTF8CString(wpeDiskCachePath);
    g_free(wpeDiskCachePath);
    WKContextConfigurationSetDiskCacheDirectory(contextConfiguration, diskCacheDirectory);

    WKContextRef context = WKContextCreateWithConfiguration(contextConfiguration);
    WKRelease(contextConfiguration);

    auto pageGroupIdentifier = WKStringCreateWithUTF8CString("WPEPageGroup");
    auto pageGroup = WKPageGroupCreateWithIdentifier(pageGroupIdentifier);
    WKRelease(pageGroupIdentifier);

    auto preferences = WKPreferencesCreate();
    // Allow mixed content.
    WKPreferencesSetAllowRunningOfInsecureContent(preferences, true);
    WKPreferencesSetAllowDisplayOfInsecureContent(preferences, true);

    // By default allow console log messages to system console reporting.
    if (!g_getenv("WPE_SHELL_DISABLE_CONSOLE_LOG"))
        WKPreferencesSetLogsPageMessagesToSystemConsoleEnabled(preferences, true);

    WKPageGroupSetPreferences(pageGroup, preferences);

    auto pageConfiguration  = WKPageConfigurationCreate();
    WKPageConfigurationSetContext(pageConfiguration, context);
    WKPageConfigurationSetPageGroup(pageConfiguration, pageGroup);
    WKPreferencesSetFullScreenEnabled(preferences, true);

    if (!!g_getenv("WPE_SHELL_COOKIE_STORAGE")) {
        gchar *cookieDatabasePath = g_build_filename(g_get_user_cache_dir(), "cookies.db", nullptr);
        auto path = WKStringCreateWithUTF8CString(cookieDatabasePath);
        g_free(cookieDatabasePath);
        auto cookieManager = WKContextGetCookieManager(context);
        WKCookieManagerSetCookiePersistentStorage(cookieManager, path, kWKCookieStorageTypeSQLite);
    }

    DEBUG("WKViewCreate");
    auto view = WKViewCreate(pageConfiguration);

    DEBUG("WKViewGetPage");
    auto page = WKViewGetPage(view);
    WKPageSetPageNavigationClient(page, &s_navigationClient.base);

    setup_stdin_io(page);

    const char* url = "http://youtube.com/tv";
    if (argc > 1)
        url = argv[1];

    DEBUG("Load first URL");
    auto shellURL = WKURLCreateWithUTF8CString(url);
    WKPageLoadURL(page, shellURL);
    WKRelease(shellURL);

    DEBUG("Enter g_main_loop_run");

    g_main_loop_run(loop);
    
    DEBUG("Exit g_main_loop_run");

    WKRelease(view);
    WKRelease(pageConfiguration);
    WKRelease(pageGroup);
    WKRelease(context);
    WKRelease(preferences);
    g_main_loop_unref(loop);
    return 0;
}
