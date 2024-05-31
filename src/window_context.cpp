#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <X11/Xutil.h>
#include "../include/window_context.h"

template <typename T>
static void check_error(T val, T error_state) {
    if (val == error_state) {
        perror(nullptr);

        throw errno;
    }
}

static unsigned long get_color(Display * dis, int screen, XColor * color_info, const char * const color_name) {
    XParseColor(dis, DefaultColormap(dis, screen), color_name, color_info);
    XAllocColor(dis, DefaultColormap(dis, screen), color_info);

    return color_info->pixel;
}

window_context::window_context(int x, int y, unsigned int width, unsigned int height, const char * const title) {
    unsigned long white;

    this->dis = XOpenDisplay((char *) 0);
    this->screen = DefaultScreen(this->dis);
    this->black = BlackPixel(this->dis, this->screen);
    white = WhitePixel(this->dis, this->screen);

    this->win = XCreateSimpleWindow(this->dis, DefaultRootWindow(this->dis), x, y, width, height, 5, white, this->black);

    XSetStandardProperties(this->dis, this->win, title, "TODO", None, nullptr, 0, nullptr);
    XSelectInput(this->dis, this->win, ExposureMask | ButtonPressMask | KeyPressMask | PointerMotionMask);

    this->gc = XCreateGC(this->dis, this->win, 0, 0);

    XSetBackground(this->dis, this->gc, white);
    XSetForeground(this->dis, this->gc, this->black);

    XClearWindow(this->dis, this->win);
    XMapRaised(this->dis, this->win);

    XColor tmp;
    this->text_color = get_color(this->dis, this->screen, &tmp, "slate blue");
    this->file_color = get_color(this->dis, this->screen, &tmp, "slate gray");
    this->dir_color = get_color(this->dis, this->screen, &tmp, "yellow");
    this->debug_color = get_color(this->dis, this->screen, &tmp, "red");
    this->hover_color = get_color(this->dis, this->screen, &tmp, "gray78");

    char * retval = getcwd(this->cwd, PATH_MAX + 1);
    check_error(retval, (char *) NULL);

    this->cwd_len = strlen(this->cwd);

    this->dir = opendir(this->cwd);
    check_error(this->dir, (DIR *) NULL);

    for (int i = 0; i < MAX_CHILDREN; i++) {
        this->children[i].len = 0;
    }

    this->read_child_dirs(0);

    this->debug_enabled = false;
    this->mouse_y = 0;
    // An arbitrary, large number
    this->max_y = 4096;
    this->can_scroll = false;
    this->scrollrow = 0;
}

window_context::~window_context() {
    int retval = closedir(this->dir);
    check_error(retval, -1);

    XFreeGC(this->dis, this->gc);
    XDestroyWindow(this->dis, this->win);
    XCloseDisplay(this->dis);
}

int window_context::on_expose(XExposeEvent &event) {
    XClearWindow(this->dis, this->win);
    XSetForeground(this->dis, this->gc, this->text_color);
    XDrawString(this->dis, this->win, this->gc, 0, 10, this->cwd, this->cwd_len);
    XGetWindowAttributes(this->dis, this->win, &this->window_attrs);

    int y = 23;

    for (int i = 0; i < MAX_CHILDREN; i++) {
        path_segment &path = this->children[i];

        if (path.len == 0) {
            break;
        }

        if (this->mouse_y < y && this->mouse_y >= (y - ROW_HEIGHT)) {
            XSetForeground(this->dis, this->gc, this->hover_color);
            XFillRectangle(this->dis, this->win, this->gc, 0, y - ROW_HEIGHT, this->window_attrs.width, ROW_HEIGHT);
            XSetForeground(this->dis, this->gc, this->black);
        }

        XDrawString(this->dis, this->win, this->gc, 20, y, path.name, path.len);

        path.y_bot = y;

        if (this->debug_enabled) {
            unsigned int w = this->window_attrs.width;
            unsigned int h = ROW_HEIGHT;

            XSetForeground(this->dis, this->gc, this->debug_color);
            XDrawRectangle(this->dis, this->win, this->gc, 0, path.y_bot - ROW_HEIGHT, w, h);
            XSetForeground(this->dis, this->gc, this->text_color);
        }

        this->draw_filetype(y, path.type);

        y += ROW_HEIGHT;
    }

    this->max_y = y;

    return NO_EXIT;
}

int window_context::on_button_press(XButtonEvent &event) {
    if (event.button == Button1) {
        for (int i = 0; i < MAX_CHILDREN; i++) {
            path_segment &path = this->children[i];

            if (path.len == 0) {
                break;
            }

            if (event.y >= (path.y_bot - 10) && event.y <= path.y_bot) {
                if (path.type == DT_DIR) {
                    this->navigate(path);
                }
            }
        }
    } else if (this->can_scroll && event.button == Button4) {
        scrollrow--;
        this->redraw();
    } else if (this->can_scroll && event.button == Button5) {
        scrollrow++;
        this->redraw();
    }

    return NO_EXIT;
}

int window_context::on_key_press(XKeyEvent &event) {
    int num_keysyms;

    KeySym * keysyms = XGetKeyboardMapping(this->dis, event.keycode, 1, &num_keysyms);
    KeySym key = keysyms[0];

    if (key == 'd') {
        this->set_debug_mode(! this->debug_enabled);
    } else if (key == 'q') {
        XFree(keysyms);

        return USER_QUIT_EXIT_CODE;
    }

    XFree(keysyms);

    return NO_EXIT;
}

int window_context::on_motion(XMotionEvent &event) {
    int curr_row = (this->mouse_y - 10) / ROW_HEIGHT;
    int next_row = (event.y - 10) / ROW_HEIGHT;

    this->mouse_y = event.y;

    // printf("curr_row: %d, next_row: %d\n", curr_row, next_row);
    if (this->mouse_y <= this->max_y && curr_row != next_row) {
        this->redraw();
    }

    return NO_EXIT;
}

void window_context::set_debug_mode(bool enabled) {
    this->debug_enabled = enabled;
    this->redraw();
}

void window_context::read_child_dirs(int start_at) {
    int i = start_at;
    struct dirent * entry;

    while (i < MAX_CHILDREN && (entry = readdir(this->dir)) != NULL) {
        memcpy(this->children[i].name, entry->d_name, 256);

        this->children[i].len = strlen(this->children[i].name);
        this->children[i].type = entry->d_type;

        i++;
    }

    if (i < MAX_CHILDREN) {
        // The entry with len = 0 marks the end
        this->children[i].len = 0;
    }
}

void window_context::redraw() {
    XFlush(this->dis);
    this->send_event.type = Expose;
    // XSendEvent(this->dis, this->win, False, ExposureMask, &this->send_event);
    this->on_expose(this->send_event.xexpose);
}

void window_context::draw_filetype(int y, unsigned char type) {
    const char * type_str;
    unsigned long type_color;

    switch (type) {
        case DT_DIR: {
            type_str = "d";
            type_color = this->dir_color;
            break;
        };
        default: {
            type_str = "f";
            type_color = this->file_color;
        };
    }

    XSetForeground(this->dis, this->gc, type_color);
    XDrawString(this->dis, this->win, this->gc, 5, y, type_str, 1);
    XSetForeground(this->dis, this->gc, this->text_color);
}

path_segment * window_context::get_selected_segment() {
    if (this->mouse_y > this->max_y || this->mouse_y < 10) {
        return nullptr;
    }

    int curr_row = (this->mouse_y - 10) / ROW_HEIGHT;

    return &this->children[curr_row];
}

void window_context::navigate(path_segment &path) {
    if (path.len == 1 && path.name[0] == '.') {
        // Do nothing
        return;
    } else if (this->cwd_len == 1 && cwd[0] == '/') {
        if (path.len == 2 && path.name[0] == '.' && path.name[1] == '.') {
            // Can't go up from root
            return;
        }

        // Root - just append the next thing
        memcpy(this->cwd + 1, path.name, path.len);
        this->cwd_len = path.len + 1;
        this->cwd[path.len + 1] = '\0';
    } else if (path.len == 2 && path.name[0] == '.' && path.name[1] == '.') {
        // Strip out last part of path
        int slashpos = this->cwd_len;

        while (slashpos && (this->cwd[--slashpos] != '/'));

        if (slashpos == 0) {
            // Last slash is root, so leave it in
            this->cwd[1] = '\0';
            this->cwd_len = 1;
        } else {
            this->cwd_len = slashpos;
            this->cwd[slashpos] = '\0';
        }
    } else {
        this->cwd[this->cwd_len] = '/';
        memcpy(this->cwd + this->cwd_len + 1, path.name, path.len);
        this->cwd_len += path.len + 1;
        this->cwd[this->cwd_len] = '\0';
    }

    if (this->dir) {
        int retval = closedir(this->dir);
        check_error(retval, -1);
    }

    this->dir = opendir(this->cwd);
    check_error(this->dir, (DIR *) NULL);

    this->read_child_dirs(0);
    this->redraw();
}
