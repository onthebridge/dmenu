/*
 *  Copyright (C) 2009 Rookie1 <mr.rookie1@gmail.com>
 *                     sca <scahoo <at> gmail <dot> com>
                       timothy.soehnlin <timothy.soehnlin@gmail.com>
 *
 *  Author: <mr.rookie1@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <unistd.h>
#include <stdlib.h>
#include <SDL.h>
#include <SDL_image.h>
#include <SDL_ttf.h>
#include "menu.h"

#include "env.h"
#include "common.h"
#include "conf.h"
#include "dosd/dosd.h"
#include "resource.h"
#include "colorpicker.h"
#include "filelist.h"
#include "imageviewer.h"
#include "persistent.h"
#include "sound.h"
#include "dingoo.h"


extern cfg_t *cfg;
cfg_t *menu_config;
cfg_t *menu_item_config;
cfg_t *menu_subitem_config;
cfg_t *menu_active_item_config;

int menu_text_pad_left = 0;
int menu_needs_redraw = 1; //First draw
SDL_Rect menu_active_rect;

SDL_Surface* background;

SDL_Surface** menu_icons;
SDL_Surface** menu_text;
int number_of_menu;

SDL_Surface*** menuitem_icons;
SDL_Surface*** menuitem_text;
int* number_of_menuitem;

SDL_Surface** submenuitem_icons;
SDL_Surface** submenuitem_text;
int number_of_submenuitem;

SDL_Surface* tmp_surface;

int current_menu_index;
int current_menuitem_index;
int current_submenuitem_index;

TTF_Font* menu_font;
TTF_Font* menuitem_font;

void menu_reload_background() 
{
    free_surface(background);
    background = get_theme_background();
}

void menu_state_changed()
{
    menu_needs_redraw = 1;
}

void menu_update_items()
{
    menu_config  = cfg_getnsec(cfg, "Menu", current_menu_index);
    menu_item_config = cfg_getnsec(menu_config, "MenuItem", current_menuitem_index);
    
    if (number_of_submenuitem > 0) 
    {
        menu_subitem_config = cfg_getnsec(menu_item_config, "SubMenuItem", current_submenuitem_index);
    } 
    else {
        menu_subitem_config = NULL;
    }
    menu_active_item_config = (menu_subitem_config == NULL) ? menu_item_config : menu_subitem_config;
}
    
void menu_get_position(char* menu_name, char* menu_item_name, char* menu_subitem_name)
{
    char* c;
    menu_update_items();
    copyname_trim(menu_name, menu_config);
    copyname_trim(menu_item_name, menu_item_config);
    copyname_trim(menu_subitem_name, menu_subitem_config);
    log_debug("Saving Position As: %s, %s, %s", menu_name, menu_item_name, menu_subitem_name);
}

void menu_set_position(const char* menu_name, const char* menu_item_name, const char* menu_subitem_name) 
{
    cfg_t *m, *mi, *smi;
    int i=0,j=0,k=0,n=0;
    char *c;
    int has_sub = menu_subitem_name[0] != '\0';
    for (i=0;i<number_of_menu;i++) {
        m = cfg_getnsec(cfg, "Menu", i);
        if_names_equal(m, menu_name) {
            for (j=0;j<number_of_menuitem[i];j++) {
                mi = cfg_getnsec(m, "MenuItem", j);
                if_names_equal(mi, menu_item_name) {
                    if (has_sub) {
                        n = cfg_size(mi, "SubMenuItem");
                        for (i=0;i<n;i++) {
                            smi = cfg_getnsec(mi, "SubMenuItem", i);
                            if_names_equal(smi, menu_subitem_name) break;
                        }    
                    }
                    break;
                }
            }
            break;
        }
    }
    if (i == number_of_menu || j == number_of_menuitem[i] || (has_sub && k==n)) 
    {
        log_message("Failed to find stored position: %d, %d, %d", i, j, k);
        return;
    }

    log_debug("Loading Position As: %s, %s, %s", menu_name, menu_item_name, menu_subitem_name);

    //Restore menu
    current_menu_index = i;
    current_menuitem_index = j;
    if (has_sub) {
        menu_open_sub();
        current_submenuitem_index = k;
    }
}

int menu_init()
{
    log_debug("Initializing");
    
    int i, j;
    SDL_Color* color = get_theme_font_color();
    menu_reload_background();
    
    /*fill_fb(background->pixels);*/

    current_menu_index = 0;
    current_menuitem_index = 0;
    current_submenuitem_index = 0;
    
    // load font
    TTF_Init();
    menu_font     = get_theme_font(MENU_TEXT_FONT_SIZE);
    menuitem_font = get_theme_font(MENU_ITEM_FONT_SIZE);

    // load menu
    number_of_menu     = cfg_size(cfg, "Menu");
    number_of_menuitem = new_array(int, number_of_menu);
    menu_icons         = new_array(SDL_Surface*,  number_of_menu);
    menu_text          = new_array(SDL_Surface*,  number_of_menu);
    menuitem_icons     = new_array(SDL_Surface**, number_of_menu);
    menuitem_text      = new_array(SDL_Surface**, number_of_menu);
    menu_text_pad_left = cfg_getint(cfg, "ItemTextPadLeft");
    
    for (i=0;i<number_of_menu;i++) {
        menu_config = cfg_getnsec(cfg, "Menu", i);
        menu_icons[i] = load_theme_image(cfg_getstr(menu_config, "Icon"));
        menu_text[i]  = draw_text(cfg_getstr(menu_config, "Name"), menu_font, color);

        number_of_menuitem[i] = cfg_size(menu_config, "MenuItem");
        menuitem_icons[i] = new_array(SDL_Surface*, number_of_menuitem[i]);
        menuitem_text[i]  = new_array(SDL_Surface*, number_of_menuitem[i]);
        
        for (j=0;j<number_of_menuitem[i];j++) {
            menu_item_config = cfg_getnsec(menu_config, "MenuItem", j);
            menuitem_icons[i][j] = load_theme_image(cfg_getstr(menu_item_config, "Icon")); 
            menuitem_text[i][j]  = draw_text(cfg_getstr(menu_item_config, "Name"), menuitem_font, color);
        }
    }

    free_color(color);
    
    persistent_restore_menu_position();
    
    return 0;
}

void menu_deinit()
{
    log_debug("De-initializing");
    
    int i, j;
    
    free_surface(background);
    
    for (i=0;i<number_of_menu;i++) {
        SDL_FreeSurface(menu_icons[i]);
        SDL_FreeSurface(menu_text[i]);
        for (j=0;j<number_of_menuitem[i];j++) {
            free_surface(menuitem_icons[i][j]);
            free_surface(menuitem_text[i][j]);
        }
        free_erase(menuitem_icons[i]);
        free_erase(menuitem_text[i]);
    }

    free_erase(menuitem_icons);
    free_erase(menuitem_text);
    free_erase(number_of_menuitem);
    free_erase(menu_icons);
    free_erase(menu_text);

    free_font(menu_font);
    free_font(menuitem_font);
    
    TTF_Quit();

    if (number_of_submenuitem > 0) menu_close_sub();
    
    // Save current menu state
    persistent_store_menu_position();
}

//Draw single menu item
void menu_draw_single_item(
    SDL_Surface* screen,
    SDL_Surface* icon, SDL_Rect* icon_rect, 
    SDL_Surface* text, SDL_Rect* text_rect,
    int alpha, int text_position) 
{
    if (text_position >= 0) {
        if (text_position == 0) {
            text_rect->x = (icon_rect->x + icon->w) + menu_text_pad_left;
            text_rect->y = icon_rect->y + icon->h/2 - text->h/2;
        } else {
            text_rect->x = icon_rect->x + icon->w/2 - text->w/2;
            text_rect->y = icon_rect->y + icon->h; 
        }

        blitSurfaceAlpha(text, NULL, screen, text_rect, alpha);
    }

    blitSurfaceAlpha(icon, NULL, screen, icon_rect, alpha);
}


//Draw vertical menu items
void menu_draw_vitems(
    SDL_Surface* screen, SDL_Rect* offset, 
    SDL_Surface** icons, SDL_Rect* icon_rect, 
    SDL_Surface** text,  SDL_Rect* text_rect, 
    int current_item,    int number_of_items, 
    int child_showing,   int is_parent, int lower_offset) {
  
    int draw_item = 0, alpha = 0, ypos = 0;
    
    icon_rect->x = offset->x;

    if (number_of_items > 0 ) {

        // draw upper items 
        draw_item = current_item - 1;
        
        if (current_item > 0 && !child_showing) {
            
            while (1) {
                
                alpha = item_alpha(current_item, draw_item); 
                icon_rect->y = offset->y - (current_item - draw_item)*icons[draw_item]->h;
                ypos = icon_rect->y; //icon_rect's position is overwritten by blitting 
                menu_draw_single_item(
                    screen, icons[draw_item], icon_rect,   
                    text[draw_item], text_rect,
                    alpha, child_showing ? -1 : 0);
                --draw_item; 
                if (ypos < 0 || draw_item < 0) break;
            }
        }

        // draw lower items 
        draw_item = current_item;
        icon_rect->y = offset->y + lower_offset;

        //Will store most recent active menu item (if submenu 
        //   is active, it will overwrite these values)
        menu_active_rect.x = icon_rect->x;
        menu_active_rect.y = icon_rect->y;

        while (1) { 
            alpha = item_alpha(current_item, draw_item)>>(child_showing?1:0);
            menu_draw_single_item(
                screen, 
                icons[draw_item], icon_rect, 
                text[draw_item], text_rect, 
                alpha, child_showing ? -1 : 0); 
            icon_rect->y += icons[draw_item]->h;
            draw_item++;
            alpha--;

            if (child_showing || icon_rect->y > screen->h || draw_item >= number_of_items) {
                break; //Only draw first item if child showing
            }
        }
    }
}

//Draw horizontal menu items
void menu_draw_hitems(
    SDL_Surface* screen, SDL_Rect* child_offset, 
    SDL_Surface** icons, SDL_Rect* icon_rect, 
    SDL_Surface** text,  SDL_Rect* text_rect, 
    int current_item, int number_of_items, int subchild_showing)
{
    int draw_item = current_item - 1;

    //While there are empty slots to the left of the menu
    //   create a gap that is a single icon wide
    while (draw_item<0) {
        icon_rect->x += icons[0]->w; 
        draw_item++;
    }
    
    while (1) {
        int alpha = item_alpha(current_item, draw_item); 
        menu_draw_single_item(
            screen, 
            icons[draw_item], icon_rect, 
            text[draw_item],  text_rect, 
            alpha>>(subchild_showing?1:0), draw_item != current_item ? -1 : 1);
   
        //Store calculated position for current menu items
        if (draw_item == current_item) { 
            init_rect_pos(child_offset, icon_rect->x, icon_rect->y);
        }

        icon_rect->x += icons[draw_item]->w;

        draw_item++;
        
        if ((icon_rect->x >= screen->w) || 
           (draw_item >= number_of_items) ||
           (subchild_showing && draw_item > current_menu_index)) 
        {
            break;
        }
    }
}

int menu_draw(SDL_Surface* screen)
{
    if (!menu_needs_redraw) return 0;
    menu_needs_redraw = 0;
    int subshow = number_of_submenuitem > 0;
    
    SDL_Rect icon_rect, text_rect, offset;

    init_rect_pos(&icon_rect, 0,0);
    init_rect_pos(&text_rect, 0,0);
    init_rect_pos(&offset, 0,0);
    
    // clear screen
    SDL_BlitSurface(background, 0, screen, &offset);
    
    //Set menu offset
    icon_rect.y = (screen->h - menu_icons[0]->h - MENU_TEXT_HEIGHT) / 3; // assuming the font height for menu name is 20

    //Draw main menu items
    menu_draw_hitems(
        screen, &offset, 
        menu_icons, &icon_rect, 
        menu_text, &text_rect, 
        current_menu_index,  number_of_menu, subshow); 

    //Draw menu items, starting at (x,y)
    menu_draw_vitems(
        screen, &offset, 
        menuitem_icons[current_menu_index], &icon_rect, 
        menuitem_text[current_menu_index], &text_rect, 
        current_menuitem_index, number_of_menuitem[current_menu_index],
        subshow, 1, menu_icons[current_menu_index]->h + MENU_TEXT_HEIGHT); 

    //Draw submenu (will only show if it is active)
    // at this point, text_rect.x and text_rect.y should be at the position where we want to draw the current

    if (subshow) {
        int new_w = max(menuitem_icons[current_menu_index][current_menuitem_index]->w, menu_text[current_menu_index]->w);
        icon_rect.x += new_w;
        text_rect.x += new_w;
    }
    
    init_rect_pos(&offset, text_rect.x, text_rect.y+MENU_TEXT_HEIGHT); 
        
    menu_draw_vitems(
        screen, &offset, 
        submenuitem_icons, &icon_rect, 
        submenuitem_text, &text_rect, 
        current_submenuitem_index, 
        number_of_submenuitem, 0, 0, 0); 
                     
    return 1;
}

void menu_force_redraw(SDL_Surface* screen) {
    menu_needs_redraw = 1;
    menu_draw(screen);
}

void menu_animate(SDL_Surface* screen)
{
    /* Menu animation, but it isn't a good use
    
    static int animate_pos = 0;
    SDL_Surface *active_icon;
    if (number_of_submenuitem > 0) {
        active_icon = submenuitem_icons[current_submenuitem_index];
    } else {
        active_icon = menuitem_icons[current_menu_index][current_menuitem_index];
    }
    
    SDL_Rect rect = {menu_active_rect.x,menu_active_rect.y,active_icon->w,active_icon->h};    
    SDL_BlitSurface(background, &rect, screen, &rect); //Clear icon

    //Need to animate better
    SDL_BlitSurface(active_icon, NULL, screen, &rect);
    
    */
}

void menu_osd(SDL_Surface* screen)  {}

void menu_move(Direction dir) 
{
    int delta =  dir==PREV ? -1 : 1;
    
    if (number_of_submenuitem > 0) {
        if (dir == PREV) {
            menu_close_sub();
        } else {
            sound_out( OUT );
        }
        return;
    }
    
    menu_state_changed();
    
    current_menu_index = wrap(current_menu_index+ delta, 0, number_of_menu-1);
    sound_out( MENU_MOVE );
    current_menuitem_index = 0;
}

void menu_move_item(Direction dir ) 
{
    menu_state_changed();
    
    int delta = dir == PREV ? -1 : 1;
    int nosi = number_of_submenuitem;
    int nomi = number_of_menuitem[current_menu_index];
    int next_mi = current_menuitem_index + delta;
    int next_si = current_submenuitem_index + delta;
    
    if (!nosi) {
        current_menuitem_index = wrap(next_mi, 0, nomi-1);
    }
    else {
        current_submenuitem_index = wrap(next_si, 0, nosi-1);
    }
    sound_out( MENUITEM_MOVE );    
}

void menu_open_sub()
{
    menu_state_changed();
    
    int i;
    SDL_Color* color = get_theme_font_color();
    sound_out( DECIDE );

    number_of_submenuitem = cfg_size(menu_item_config, "SubMenuItem");
    submenuitem_icons = new_array(SDL_Surface*, number_of_submenuitem);
    submenuitem_text  = new_array(SDL_Surface*, number_of_submenuitem);
    
    for (i=0;i<number_of_submenuitem;i++) {
        menu_subitem_config = cfg_getnsec(menu_item_config, "SubMenuItem", i);
        submenuitem_icons[i] = load_theme_image(cfg_getstr(menu_subitem_config, "Icon"));
        submenuitem_text[i]  = draw_text(cfg_getstr(menu_subitem_config, "Name"), menuitem_font, color);
    }    
    
    free_color(color);
    current_submenuitem_index = 0;
}

void menu_close_sub()
{
    menu_state_changed();
    
    int i;

    sound_out( CANCEL );
    for (i=0;i<number_of_submenuitem;i++) {
        free_surface(submenuitem_icons[i]);
        free_surface(submenuitem_text[i]);
    }

    free_erase(submenuitem_icons);
    free_erase(submenuitem_text);
    
    number_of_submenuitem = 0;
}

MenuState menu_run_internal()
{
    char* executable = cfg_getstr(menu_active_item_config, "Executable");
    char* name       = cfg_getstr(menu_active_item_config, "Name");
    char* tmp;
    char **files;
    ImageEntry** images;
    MenuState state = MAINMENU;
    int rc=0, i=0, count=0;
    
    switch (get_command(executable))
    {
        case THEMESELECT:
            //A lot of string manipulation happens to convert the file names
            // Of the theme directories to the proper paths for the thumbnails
            // and the theme titles.  This is why we handle so much malloc
            // and free in this case.  The goal is to ensure no memory
            // is leaked.
            files = get_theme_previews();
            while (files[count] != NULL) count++;
            images = new_array(ImageEntry*, count+1);
            
            for (i=0;i<count;i++) 
            {
                images[i] = new_item(ImageEntry); 
                {
                    images[i]->file = strdup(files[i]);
                    tmp = strndup(files[i], strrpos(files[i], '/'));
                    images[i]->title = strdup(strrchr(tmp,'/')+1);
                    free_erase(tmp);
                }
                free_erase(files[i]);
            }
            free_erase(files);
            images[count] = NULL;
            
            rc = imageviewer_init(name, executable, DMENU_THEMES, images);
            if (!rc) state = IMAGEVIEWER;
            for (i=0;i<count;i++) 
            {
                free_erase(images[i]->file);
                free_erase(images[i]->title);
                free_erase(images[i]);
            }
            //TODO: Figure this out. For some reason, this is 
            // causing a segfault on my machine.  
            //free_erase(images);
            break;
        case BACKGROUNDSELECT:
            rc = imageviewer_init(name, executable, DMENU_BACKGROUNDS, NULL);
            if (!rc) state = IMAGEVIEWER;
            break;
        case COLORSELECT:
            tmp = theme_file(get_user_attr("Background"));
            rc = colorpicker_init(name, executable, NULL, get_theme_font_color_string(), tmp);
            if (!rc) state = COLORPICKER;
            free_erase(tmp);
            break;
    }
    
    return state;
}

MenuState menu_run_item()
{
    char* executable = cfg_getstr(menu_active_item_config, "Executable");
    
    if (executable) {
        if (internal_command(executable)) {
            return menu_run_internal();
        } 
        else {
            run_command(executable, NULL, cfg_getstr(menu_active_item_config, "WorkDir"));
        }
    } else {
        menu_open_sub();
    }
    
    return MAINMENU;
}

MenuState menu_keypress(SDLKey key)
{
    MenuState state = MAINMENU;
    Direction dir = getKeyDir(key);
    
    switch (key) {
        case DINGOO_BUTTON_LEFT:
        case DINGOO_BUTTON_RIGHT:
            menu_move(dir);
            break;
        case DINGOO_BUTTON_UP:
        case DINGOO_BUTTON_DOWN:
            menu_move_item(dir);
            break;
        case DINGOO_BUTTON_B:
            if (number_of_submenuitem > 0) menu_close_sub();
            break;
        case DINGOO_BUTTON_A:
            menu_update_items();
            
            if (cfg_getbool(menu_active_item_config, "Selector")) 
            {
                char* listdir = cfg_getstr(menu_active_item_config, "SelectorDir");
                if (!listdir ) listdir = cfg_getstr(menu_active_item_config, "WorkDir");
                if (!filelist_init(cfg_getstr(menu_active_item_config, "Name"),
                        cfg_getstr(menu_active_item_config, "Executable"),
                        cfg_getstr(menu_active_item_config, "WorkDir"),
                        listdir, 1))
                {
                    state = FILELIST;
                } // else we are not able to initialise the filelist display
            } 
            else {
                state = menu_run_item();
            }
            break;
        default: break;
    }
    return state;
}