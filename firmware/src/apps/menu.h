#pragma once

#include "app.h"
#include "./display/page_manager.h"

#include <map>
#include <memory>

class MenuPage : public BasePage
{
public:
    MenuPage(lv_obj_t *parent, int8_t app_id, const char *friendly_name, lv_img_dsc_t icon, lv_img_dsc_t small_icon) : BasePage(parent), app_id_(app_id), friendly_name_(friendly_name), icon_(icon), small_icon_(small_icon)
    {
        img_ = lv_img_create(page);
        lv_img_set_src(img_, &icon_);
        lv_obj_set_style_img_recolor_opa(img_, LV_OPA_COVER, 0);
        lv_obj_set_style_img_recolor(img_, LV_COLOR_MAKE(0x00, 0xFF, 0xFF), 0);
        lv_obj_align(img_, LV_ALIGN_CENTER, 0, 0);

        lv_obj_t *label = lv_label_create(page);
        lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
        lv_obj_set_width(label, 180);
        lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
        lv_label_set_text(label, friendly_name_);
        lv_obj_align(label, LV_ALIGN_BOTTOM_MID, 0, -48);

        return;
    }

    const lv_img_dsc_t *getSmallIcon() { return small_icon_src_; }

    // Swap in icons loaded later (from Home Assistant). The descriptors must outlive
    // the page; LVGL keeps pointing at them. Call with the screen mutex held.
    void setIcons(const lv_img_dsc_t *icon, const lv_img_dsc_t *small_icon)
    {
        small_icon_src_ = small_icon;
        lv_img_set_src(img_, icon);
        lv_obj_align(img_, LV_ALIGN_CENTER, 0, 0);
    }

    int8_t app_id_;

private:
    const char *friendly_name_;
    lv_img_dsc_t icon_;
    lv_img_dsc_t small_icon_;
    const lv_img_dsc_t *small_icon_src_ = &small_icon_;
    lv_obj_t *img_;
};

class Menu : public App
{
public:
    Menu(SemaphoreHandle_t mutex);

    void initScreen();
    void add_page(int8_t id, int8_t app_id, const char *friendly_name, lv_img_dsc_t icon, lv_img_dsc_t small_icon);

    std::shared_ptr<MenuPage> find_page(int8_t id);

    uint8_t get_menu_position();
    void set_menu_position(uint8_t position);

    uint8_t get_menu_page_count();
    void set_menu_page_count(uint8_t count);

    // Replace the icons of the page that opens app app_key and refresh the side icons.
    void setAppIcons(int8_t app_key, const lv_img_dsc_t *icon, const lv_img_dsc_t *small_icon);

    void setMenuName(const char *name)
    {
        lv_label_set_text(menu_name_label, name);
    }

protected:
    lv_obj_t *left_image_icon;
    lv_obj_t *right_image_icon;
    lv_img_dsc_t left_icon;
    lv_img_dsc_t right_icon;

    lv_obj_t *menu_name_label;

    uint8_t menu_page_count = 0;
    uint8_t current_menu_position = 0;

    std::map<uint8_t, std::shared_ptr<MenuPage>> pages;
};