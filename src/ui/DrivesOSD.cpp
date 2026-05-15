
#include "DrivesOSD.hpp"


/**
 * @brief Lays out tiles in a grid pattern based on the largest tile dimensions.
 * 
 * This method:
 * 0. Generate temporary tile list of visible tiles.
 * 1. Finds largest tile width among visible tiles.
 * 2. Calculates horizontal grid dimension based on container width and tile width.
 * 3. For each line of tiles:
 *    Finds the tallest tile dimension among visible tiles
 * 4. Calculates grid location for each tile based on line, column, and line height
 * 5. Positions each visible tile according to layout direction flags
*/
void DrivesOSD_t::layout() {
    if (!visible || tiles.empty()) return;

    // 0. Generate temporary tile list of visible tiles.
    // * 1. Finds largest tile width among visible tiles.
    float max_tile_width = 0;
    size_t visible_count = 0;

    std::vector<Tile_t*> visible_tiles;
    for (size_t i = 0; i < tiles.size(); i++) {
        if (tiles[i] && tiles[i]->is_visible()) {
            visible_tiles.push_back(tiles[i]);

            float tile_w, tile_h;
            visible_tiles[i]->get_tile_size(&tile_w, &tile_h);
            max_tile_width = std::max(max_tile_width, tile_w);
            visible_count++;
        }
    }
    
    if (visible_count == 0) return;

    // Calculate grid dimensions
    float cell_width = max_tile_width + style.padding * 2;

    //float cell_height = max_tile_height + style.padding * 2;
    
    // Calculate how many tiles can fit in each row
    size_t tiles_per_row = static_cast<size_t>(tp.w / cell_width);
    if (tiles_per_row == 0) tiles_per_row = 1;  // Ensure at least one tile per row
    
    // Calculate number of rows needed
    size_t rows = (visible_count + tiles_per_row - 1) / tiles_per_row;
    float row_y = tp.y;

    for (size_t i = 0; i < rows; i++) {

        // run through this row tiles to find the tallest tile height
        float row_max_tile_height = 0;

        size_t tile_base = i * tiles_per_row;
        for (size_t j = 0; j < tiles_per_row; j++) {
            if (j+tile_base >= visible_count) break; // don't run over the end.
            float tile_w, tile_h;
            visible_tiles[tile_base + j]->get_tile_size(&tile_w, &tile_h);
            row_max_tile_height = std::max(row_max_tile_height, tile_h);
        }

        // now position the tiles in this row
        for (size_t j = 0; j < tiles_per_row; j++) {
            if (j+tile_base >= visible_count) break; // don't run over the end.
            float tile_x = tp.x + j * cell_width + style.padding;
            float tile_y = row_y + style.padding;
            visible_tiles[tile_base + j]->set_position(tile_x, tile_y);
        }
        //printf("row %zu: %f %f\n", i, row_y, row_max_tile_height);
        // now calculate the position for this row
        row_y += row_max_tile_height + style.padding*2;

    }
    
}