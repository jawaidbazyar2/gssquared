#pragma once

#include "Button.hpp"
#include "UIContext.hpp"
#include "systemconfig.hpp"
#include "util/TextRenderer.hpp"

class SystemButton : public Button_t {
    public:
        SystemButton(UIContext *ctx, const SystemConfig_t *system_config, int image_id, Style_t style,
                     bool custom = false, TextRenderer *name_renderer = nullptr)
            : Button_t(ctx, image_id, style, 0),
              system_config(system_config),
              is_custom(custom),
              name_renderer(name_renderer) {}
        ~SystemButton() { };
        void render() override;

    private:
        const SystemConfig_t *system_config;
        bool is_custom = false;
        TextRenderer *name_renderer = nullptr;
};
