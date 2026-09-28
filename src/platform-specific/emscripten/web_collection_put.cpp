#if defined(__EMSCRIPTEN__)

#include "web_collection_put.hpp"

#include <emscripten.h>

void web_put_pack(const std::string& url, const std::string& path, const std::string& token,
                  int generation) {
    // FS.readFile copies the bytes before this returns, so the caller may
    // rewrite or delete the MEMFS file while the fetch is still running.
    MAIN_THREAD_EM_ASM({
        var url = UTF8ToString($0);
        var path = UTF8ToString($1);
        var token = UTF8ToString($2);
        var generation = $3;
        var bytes;
        try {
            bytes = FS.readFile(path);
        } catch (err) {
            ccall('gs2_web_put_done', null, ['number', 'number', 'string'],
                  [generation, 0, String(err)]);
            return;
        }
        fetch(url, {
            method: 'PUT',
            mode: 'cors',
            credentials: 'omit',
            headers: {
                'Content-Type': 'application/x-tar',
                'X-GS2-Save-Token': token
            },
            body: bytes
        }).then(function (resp) {
            ccall('gs2_web_put_done', null, ['number', 'number', 'string'],
                  [generation, resp.status, '']);
        }).catch(function (err) {
            ccall('gs2_web_put_done', null, ['number', 'number', 'string'],
                  [generation, 0, String(err)]);
        });
    }, url.c_str(), path.c_str(), token.c_str(), generation);
}

#endif
