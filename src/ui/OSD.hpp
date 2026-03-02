/*
 *   Copyright (c) 2025-2026 Jawaid Bazyar

 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.

 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.

 *   You should have received a copy of the GNU General Public License
 *   along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */
#pragma once

#include <vector>

#include <SDL3/SDL.h>

#include "slots.hpp"

#include "Container.hpp"
#include "ModalContainer.hpp"
#include "MousePositionTile.hpp"
#include "AssetAtlas.hpp"
#include "util/TextRenderer.hpp"
#include "FadeButton.hpp"

#define SLIDE_IN 1
#define SLIDE_OUT 2
#define SLIDE_NONE 0

#define slidePositionDeltaMax 200
#define slidePositionDeltaMin 20
#define slidePositionAcceleration 10
#define slidePositionMax 1120

/**
 * @brief On-Screen Display manager class.
 * 
 * Handles the sliding control panel and its contained UI elements
 * including disk drive controls, slot buttons, and monitor controls.
 */
class OSD {
public:
    EventQueue *event_queue = nullptr;
    computer_t *computer = nullptr;

protected:
    int slideStatus = SLIDE_NONE;
    int currentSlideStatus = SLIDE_OUT;
    int slidePosition = -slidePositionMax;
    int slidePositionDelta = slidePositionDeltaMax;
    SlotManager_t *slot_manager = nullptr;
    
    Container_t *drive_container = nullptr;

    Button_t *speed_btn_10 = nullptr;
    Button_t *speed_btn_28 = nullptr;
    Button_t *speed_btn_71 = nullptr;
    Button_t *speed_btn_8 = nullptr;
    Button_t *speed_btn_14 = nullptr;

    Button_t *save_btn = nullptr;
    Button_t *save_as_btn = nullptr;
    Button_t *discard_btn = nullptr;
    Button_t *cancel_btn = nullptr;

    NClock *clock = nullptr;

    //int controlOpacity = 0;
    Button_t *close_btn = nullptr;
    FadeButton_t *open_btn = nullptr;

    std::vector<Container_t *> containers;
    Container_t *hud_drive_container = nullptr;
    ModalContainer_t *activeModal = nullptr;
    ModalContainer_t *diskii_save_con = nullptr;

    MousePositionTile_t* mouse_pos = nullptr;
    AssetAtlas_t *aa = nullptr;
    SDL_Renderer *renderer = nullptr;
    SDL_Texture *cpTexture = nullptr;
    SDL_Window *window = nullptr;
    int window_w = 0;
    int window_h = 0;
    TextRenderer *text_render = nullptr;
    TextRenderer *title_trender = nullptr;

    std::string headsUpMessageText;
    int headsUpMessageCount = 0;

    int slideStatusBeforeDrop = SLIDE_NONE;

public:
    /**
     * @brief Constructs the OSD with the given renderer and window.
     * 
     * @param rendererp SDL renderer to use
     * @param windowp SDL window to render to
     * @param window_width Width of the window
     * @param window_height Height of the window
     */
    OSD(computer_t *computer, SDL_Renderer *rendererp, SDL_Window *windowp, SlotManager_t *slot_manager, int window_width, int window_height, AssetAtlas_t *aa);
    ~OSD();
    
    /**
     * @brief Gets the SDL window associated with this OSD.
     * @return Pointer to the SDL window
     */
    SDL_Window *get_window();

    /**
     * @brief Updates the OSD state, including slide animations.
     */
    void update();

    /**
     * @brief Renders the OSD and all its components.
     */
    void render();

    /**
     * @brief Handles SDL events for the OSD.
     * @param event The SDL event to process
     */
    bool event(const SDL_Event &event);

    void set_raise_window();

    void show_diskii_modal(storage_key_t key, uint64_t data);
    void close_diskii_modal(storage_key_t key, uint64_t data);

    void set_heads_up_message(const std::string &text, int count);

    void open_panel();
    void close_panel();

    void set_clock(NClock *clock) { this->clock = clock; }
};