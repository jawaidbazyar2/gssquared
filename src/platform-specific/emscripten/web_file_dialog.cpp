#if defined(__EMSCRIPTEN__)

#include "web_file_dialog.hpp"

#include <emscripten.h>
#include <cstdlib>
#include <string>

// Pending dialog state. Only one open dialog is supported at a time, which
// matches how the OSD uses it (one drive click -> one picker).
static SDL_DialogFileCallback g_web_dialog_callback = nullptr;
static void                  *g_web_dialog_userdata = nullptr;
static int                    g_web_dialog_generation = 0;

int web_file_dialog_generation()
{
    return g_web_dialog_generation;
}

void web_install_file_dialog_hook()
{
    // Bubble phase runs after SDL's canvas pointerup handler has queued the
    // mouse button. The click itself is a user-activation event, so a picker
    // opened before this listener returns is allowed in Safari.
    EM_ASM({
        if (window.__gs2FileGestureInstalled) return;
        window.__gs2FileGestureInstalled = true;
        var canvas = (typeof Module !== 'undefined' && Module['canvas'])
            || document.getElementById('canvas');
        document.addEventListener('click', function (ev) {
            if (!canvas || ev.target !== canvas) return;
            try {
                ccall('gs2_web_user_gesture', null, [], []);
            } catch (err) {
                console.error('gs2_web_user_gesture', err);
            }
        }, false);
    });
}

// Called from JavaScript (via ccall) once the chosen file has been written into
// the WASM filesystem. `path` is an absolute MEMFS path such as
// "/uploads/foo.dsk". A null/empty path means the user cancelled.
// `generation` must match the dialog that created the <input>; a superseded
// picker must not deliver into the newer callback.
extern "C" void gs2_web_file_selected(const char *path);

extern "C" EMSCRIPTEN_KEEPALIVE
void gs2_web_file_selected_if(int generation, const char *path)
{
    if (generation != g_web_dialog_generation)
        return;
    gs2_web_file_selected(path);
}

extern "C" EMSCRIPTEN_KEEPALIVE
void gs2_web_file_selected(const char *path)
{
    SDL_DialogFileCallback cb = g_web_dialog_callback;
    void *userdata = g_web_dialog_userdata;
    g_web_dialog_callback = nullptr;
    g_web_dialog_userdata = nullptr;

    if (!cb) return;

    if (path && path[0]) {
        const char *filelist[2] = { path, nullptr };
        cb(userdata, filelist, -1);
    } else {
        // Cancel: SDL convention is a non-null list whose first entry is null.
        const char *filelist[1] = { nullptr };
        cb(userdata, filelist, -1);
    }
}

void web_open_file_dialog(SDL_DialogFileCallback callback, void *userdata, const char *accept)
{
    // Supersede any previously pending dialog. Tell the old caller it was
    // cancelled so it can free userdata; otherwise a Safari rejection leaks it.
    if (g_web_dialog_callback) {
        SDL_DialogFileCallback old_cb = g_web_dialog_callback;
        void *old_ud = g_web_dialog_userdata;
        g_web_dialog_callback = nullptr;
        g_web_dialog_userdata = nullptr;
        const char *none[1] = { nullptr };
        old_cb(old_ud, none, -1);
    }
    g_web_dialog_callback = callback;
    g_web_dialog_userdata = userdata;

    const char *accept_attr = accept ? accept : "";
    g_web_dialog_generation++;

    // Safari's input.click() only opens a picker when
    // UserGestureIndicator::processingUserGesture() is true — the DOM event
    // call stack, not the few seconds of transient activation Chrome allows.
    // File > Drives used to reach this from a later animation frame, so Safari
    // dropped the dialog with no error. Callers on the web drain that click
    // inside the DOM listener (gs2_web_user_gesture) before we get here.
    //
    // display:none also suppresses the picker in Safari (no renderer). The
    // input stays in the layout tree, off to the side.
    MAIN_THREAD_EM_ASM({
        var accept = UTF8ToString($0);
        var generation = $1;
        try { FS.mkdir('/uploads'); } catch (e) { /* already exists */ }

        var input = document.createElement('input');
        input.type = 'file';
        input.setAttribute('data-gs2-file', '1');
        if (accept) input.accept = accept;
        input.style.position = 'fixed';
        input.style.left = '-1000px';
        input.style.top = '0';
        input.style.width = '20px';
        input.style.height = '20px';
        input.style.opacity = '0';
        document.body.appendChild(input);

        var cleanup = function () {
            if (input && input.parentNode) input.parentNode.removeChild(input);
        };

        var deliver = function (path) {
            ccall('gs2_web_file_selected_if', null, ['number', 'string'], [generation, path]);
            cleanup();
        };

        input.addEventListener('change', function (event) {
            var file = event.target.files && event.target.files[0];
            if (!file) {
                deliver("");
                return;
            }
            var reader = new FileReader();
            reader.onload = function (e) {
                var path = '/uploads/' + file.name;
                try {
                    var data = new Uint8Array(e.target.result);
                    FS.writeFile(path, data);
                    deliver(path);
                } catch (err) {
                    console.error('web_open_file_dialog: write failed', err);
                    deliver("");
                }
            };
            reader.onerror = function () {
                deliver("");
            };
            reader.readAsArrayBuffer(file);
        }, false);

        input.addEventListener('cancel', function () {
            deliver("");
        }, false);

        var opened = false;
        try {
            // showPicker() consults transient activation and throws if the
            // gesture is gone. click() is the fallback for older Safari; it
            // fails silent when the gesture is gone, so only use it second.
            if (typeof input.showPicker === 'function') {
                input.showPicker();
                opened = true;
            }
        } catch (err) {
            console.warn('web_open_file_dialog: showPicker failed', err);
        }
        if (!opened) {
            try {
                input.click();
            } catch (err) {
                console.error('web_open_file_dialog: picker blocked', err);
                deliver("");
            }
        }
    }, accept_attr, g_web_dialog_generation);
}

#endif // __EMSCRIPTEN__
