
#include "FadeButton.hpp"

/**  */
/* bool FadeButton_t::handle_mouse_event(const SDL_Event& event) {
    return Button_t::handle_mouse_event(event);
} */

void FadeButton_t::render() {
    if (frameCount > 0) { // if frameCount is 0, don't render anything.
        frameCount -= fadeSteps;
        if (frameCount < 0) frameCount = 0;

        int opc = frameCount > 255 ? 255 : frameCount;
        set_opacity(opc); 
        Button_t::render();
    }
}