#include <SDL3/SDL.h>

#include <string>

#include "SystemButton.hpp"
#include "MainAtlas.hpp"
#include "AssetAtlas.hpp"

namespace {

std::string truncate_to_width(TextRenderer *tr, const char *name, float max_w) {
    std::string s = name ? name : "";
    if (s.empty() || tr->string_width(s) <= max_w) {
        return s;
    }
    static const char *ellipsis = "...";
    const int ellipsis_w = tr->string_width(ellipsis);
    if (ellipsis_w >= max_w) {
        return "";
    }
    while (!s.empty() && tr->string_width(s) + ellipsis_w > max_w) {
        s.pop_back();
    }
    // Avoid leaving a trailing UTF-8 continuation byte if we clipped mid-sequence.
    while (!s.empty()) {
        const unsigned char c = static_cast<unsigned char>(s.back());
        if ((c & 0xC0) != 0x80) {
            break;
        }
        s.pop_back();
    }
    return s + ellipsis;
}

}  // namespace

void SystemButton::render() {
    Button_t::render();

    if (is_custom) {
        constexpr float mark = 32.0f;
        constexpr float pad = 10.0f;
        const float x = tp.x + tp.w - mark - pad;
        const float y = tp.y + pad;
        if (ctx->asset_atlas) {
            Asset_t gear;
            ctx->asset_atlas->get_asset(GS2ConfigGear, gear);
            const SDL_FRect src = gear.rect;
            const SDL_FRect dst = {x, y, mark, mark};
            SDL_RenderTexture(ctx->renderer, gear.image, &src, &dst);
        }

        TextRenderer *label_tr = name_renderer ? name_renderer : ctx->text_render;
        if (system_config && system_config->name && system_config->name[0] && label_tr) {
            const float max_w = tp.w - pad * 2.0f;
            const std::string label = truncate_to_width(label_tr, system_config->name, max_w);
            if (!label.empty()) {
                const int line_h = label_tr->get_font_line_height();
                const float name_x = tp.x + tp.w / 2.0f;
                const float name_y = tp.y + tp.h - static_cast<float>(line_h) - pad;
                label_tr->set_color(0x00, 0x00, 0x00, 0xFF);
                label_tr->render(label, static_cast<int>(name_x), static_cast<int>(name_y),
                                 TEXT_ALIGN_CENTER);
            }
        }
    }

    // draw the system description
    if (is_hovering && system_config && system_config->description && system_config->description[0]) {
        ctx->text_render->set_color(0xFF, 0xFF, 0xFF, 0xFF);
        ctx->text_render->render(system_config->description,
                                 static_cast<int>(ctx->description_x),
                                 static_cast<int>(ctx->description_y),
                                 TEXT_ALIGN_CENTER);
    } else if (is_hovering && system_config && system_config->name && system_config->name[0]) {
        ctx->text_render->set_color(0xFF, 0xFF, 0xFF, 0xFF);
        ctx->text_render->render(system_config->name,
                                 static_cast<int>(ctx->description_x),
                                 static_cast<int>(ctx->description_y),
                                 TEXT_ALIGN_CENTER);
    }
}
