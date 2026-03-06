#pragma once

#include "Button.hpp"
#include "UIContext.hpp"

class LabeledButton : public Button_t {
    protected:

    public:       
        LabeledButton(UIContext *ctx, int assetID, const std::string& button_text, int group = 0);
        void render(SDL_Renderer* renderer) override;
};