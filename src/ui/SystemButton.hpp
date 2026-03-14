#pragma once

#include "Button.hpp"
#include "UIContext.hpp"
#include "systemconfig.hpp"

class SystemButton : public Button_t {
    public:
        SystemButton(UIContext *ctx, SystemConfig_t *system_config, int image_id, Style_t style) : Button_t(ctx, image_id, style, 0) {
            this->system_config = system_config;
        }
        ~SystemButton() { };
        void render() override;

    private:
        SystemConfig_t *system_config;
        int image_id;
};