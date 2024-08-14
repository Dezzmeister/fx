/*
 * This file is part of fx, a graphical file explorer.
 * Copyright (C) 2024  Joe Desmond
 *
 * fx is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * fx is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with fx.  If not, see <https://www.gnu.org/licenses/>.
 */
#ifndef INCLUDE_WINDOW_CONTEXT_H
#define INCLUDE_WINDOW_CONTEXT_H

#include <dirent.h>
#include <linux/limits.h>
#include <sys/stat.h>
#include <X11/Xlib.h>

#define NO_EXIT             0
#define USER_QUIT_EXIT_CODE 1
#define USER_CD_EXIT_CODE   2

const size_t MAX_CHILDREN = 1024;
// Must be >= 256
const size_t MAX_PATH_SEGMENT_SIZE = 256;

const size_t ROW_HEIGHT = 13;

struct path_segment {
    char name[MAX_PATH_SEGMENT_SIZE];
    size_t len;
    int y_bot;
    unsigned int mode;
    unsigned int uid;
    unsigned int gid;
};

class window_context {
    public:
        Display * dis;

        window_context(int x, int y, unsigned int width, unsigned int height, const char * const title);

        int on_expose(XExposeEvent &event);

        int on_button_press(XButtonEvent &event);

        int on_key_press(XKeyEvent &event);

        int on_motion(XMotionEvent &event);

        ~window_context();

    private:
        int screen;
        Window win;
        GC gc;
        // /usr/share/X11/rgb.txt
        unsigned long black;
        unsigned long text_color;
        unsigned long file_color;
        unsigned long dir_color;
        unsigned long debug_color;
        unsigned long hover_color;
        unsigned long no_perm_color;
        unsigned long status_color;
        XWindowAttributes window_attrs;
        // https://insanecoding.blogspot.com/2007/11/pathmax-simply-isnt.html
        // I am using PATH_MAX anyway. If you have a path longer than PATH_MAX, you have bigger problems
        // than a buffer overflow in a silly file explorer
        char cwd[PATH_MAX + 1];
        size_t cwd_len;
        DIR * dir;
        path_segment children[MAX_CHILDREN];
        int mouse_y;
        int max_y;
        bool can_scroll;
        int scrollrow;
        int max_scrollrow;
        bool debug_enabled;
        struct stat tmp_stat;
        char tmp_path[PATH_MAX + 1];
        size_t tmp_path_len;
        unsigned int uid;
        unsigned int gid;
        // 256 for message text + 256 max filename size in case I want to write
        // filenames here
        char status[512];
        size_t status_len;
        Pixmap back_buffer;
        int max_area;
        bool show_help;

        void read_child_dirs(int start_at);

        void redraw();
        void draw_help();

        void draw_filetype(int y, unsigned int mode);

        path_segment * get_selected_segment();

        void path_join(char * const wd, size_t * wd_len, path_segment &path);

        void navigate(path_segment &path);

        // If `path` is a directory, returns true if the current user
        // has execute permissions. Otherwise returns true if the current
        // user has read permissions.
        bool has_permission(path_segment &path);

        void set_status(const char * const text);

        void set_debug_mode(bool enabled);

        template <size_t N>
        void print_multiline_str(const char (&str)[N], int x, int y);
};

#endif
