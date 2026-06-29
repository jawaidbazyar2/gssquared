#if defined(__EMSCRIPTEN__)

#include "web_file_dialog.hpp"

#include <emscripten.h>
#include <cstdlib>
#include <string>

// Pending dialog state. Only one open dialog is supported at a time, which
// matches how the OSD uses it (one drive click -> one picker).
static SDL_DialogFileCallback g_web_dialog_callback = nullptr;
static void                  *g_web_dialog_userdata = nullptr;

// Called from JavaScript (via ccall) once the chosen file has been written into
// the WASM filesystem. `path` is an absolute MEMFS path such as
// "/uploads/foo.dsk". A null/empty path means the user cancelled.
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
    // Supersede any previously pending dialog.
    g_web_dialog_callback = callback;
    g_web_dialog_userdata = userdata;

    const char *accept_attr = accept ? accept : "";

    // Create a transient <input type=file>, click it, and on change read the
    // file into /uploads/<name> then hand the path back to C.
    MAIN_THREAD_EM_ASM({
        var accept = UTF8ToString($0);
        try { FS.mkdir('/uploads'); } catch (e) { /* already exists */ }

        var input = document.createElement('input');
        input.type = 'file';
        if (accept) input.accept = accept;
        input.style.display = 'none';
        document.body.appendChild(input);

        var cleanup = function () {
            if (input && input.parentNode) input.parentNode.removeChild(input);
        };

        input.addEventListener('change', function (event) {
            var file = event.target.files && event.target.files[0];
            if (!file) {
                ccall('gs2_web_file_selected', null, ['string'], ['']);
                cleanup();
                return;
            }
            var reader = new FileReader();
            reader.onload = function (e) {
                var path = '/uploads/' + file.name;
                try {
                    var data = new Uint8Array(e.target.result);
                    FS.writeFile(path, data);
                    ccall('gs2_web_file_selected', null, ['string'], [path]);
                } catch (err) {
                    console.error('web_open_file_dialog: write failed', err);
                    ccall('gs2_web_file_selected', null, ['string'], ['']);
                }
                cleanup();
            };
            reader.onerror = function () {
                ccall('gs2_web_file_selected', null, ['string'], ['']);
                cleanup();
            };
            reader.readAsArrayBuffer(file);
        }, false);

        // If the user cancels the native picker there is no reliable event in
        // all browsers; the dialog state simply stays pending until the next
        // open call supersedes it. Clicking triggers the picker.
        input.click();
    }, accept_attr);
}

#endif // __EMSCRIPTEN__
