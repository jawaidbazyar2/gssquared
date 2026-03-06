#pragma once

#include "Button.hpp"

class FadeButton_t : public Button_t {
protected:
    /* Setting */
    int fadeFrames = 0;
    int fadeSteps = 0;

    /* Current state */
    int frameCount = 255;

public:
    FadeButton_t(const std::string& button_text, const Style_t& style = Style_t(), int group = 0) : Button_t(button_text, style, group) {}
    FadeButton_t(UIContext *ctx, int assetID, const Style_t& style = Style_t(), int group = 0) : Button_t(ctx, assetID, style, group) {}
    FadeButton_t(const std::string& button_text, int group = 0) : Button_t(button_text, group) {}

    void reset_fade() { frameCount = fadeFrames; }
    void set_fade_frames(int frames, int steps ) { frameCount = frames; fadeFrames = frames; fadeSteps = steps; }

    //bool handle_mouse_event(const SDL_Event& event) override;

    void render(SDL_Renderer* renderer) override;
};
