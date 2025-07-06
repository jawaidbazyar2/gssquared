/*
 *   Copyright (c) 2025 Jawaid Bazyar

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

#include <SDL3/SDL.h>

enum MainAtlas {
    DiskII_Open,
    DiskII_Closed,
    DiskII_Drive1,
    DiskII_Drive2,
    DiskII_DriveLightOff,
    DiskII_DriveLightOn,
    ColorDisplayButton,
    GreenDisplayButton,
    AmberDisplayButton,
    MHz1_0Button,
    MHz2_8Button,
    MHz4_0Button,
    MHzInfinityButton,
    ResetButton,
    Unidisk_Face,
    Unidisk_Light,
    Unidisk_Drive1,
    Unidisk_Drive2,
    WhiteDisplayButton,
    RGBDisplayButton,
    Badge_IIPlus,
    Badge_II,
    Badge_IIE,
    Badge_IIEEnh,
    MainAtlas_count
};

extern SDL_FRect asset_rects[];
