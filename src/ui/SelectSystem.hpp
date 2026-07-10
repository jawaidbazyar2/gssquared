#pragma once

#include <SDL3/SDL.h>
#include <memory>
#include <string>
#include <vector>

#include "Container.hpp"
#include "AssetAtlas.hpp"
#include "UIContext.hpp"
#include "videosystem.hpp"
#include "util/SystemConfig.hpp"

#define SELECT_PENDING   -2
#define SELECT_QUIT      -1
#define SELECT_NEW       -3
#define SELECT_OPEN_EDIT -4
/** recent tile ids are SELECT_RECENT_BASE + index into recent_paths_ */
#define SELECT_RECENT_BASE 1000

class SelectSystem {
protected:
    video_system_t *vs;
    Container_t *container;
    bool updated = false;
    TextRenderer *text_renderer;
    TextRenderer *name_renderer = nullptr;  // smaller font for custom tile labels
    int selected_system = SELECT_PENDING;
    int window_width, window_height;
    int design_width = 1288;
    int design_height = 928;
    AssetAtlas_t *aa;
    UIContext ui_ctx;

    /** Owned loaded configs for recent tiles (keeps SystemConfig_t string pointers valid). */
    std::vector<std::unique_ptr<SystemConfig>> recent_loaded_;
    std::vector<std::string> recent_paths_;

public:
    
    /**
     * @brief Constructs the OSD with the given renderer and window.
     * 
     * @param rendererp SDL renderer to use
     * @param windowp SDL window to render to
     * @param window_width Width of the window
     * @param window_height Height of the window
     */
    SelectSystem(video_system_t *vs, AssetAtlas_t *aa);

    ~SelectSystem();

    /**
     * @brief Updates the SelectSystem state
     */
    bool update();

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

    int get_selected_system();

    /** Path for a recent-tile selection id (SELECT_RECENT_BASE + i). */
    const std::string& get_recent_path(int selection_id) const;

    /** Reset selection back to pending (after consuming NEW / OPEN_EDIT). */
    void clear_selection() { selected_system = SELECT_PENDING; }
};
