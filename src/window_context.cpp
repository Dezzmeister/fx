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

static int str_cmp(const char * const a, size_t a_len, const char * const b, size_t b_len) {
    size_t len = a_len <= b_len ? a_len : b_len;

    for (int i = 0; i < len; i++) {
        char a_char = a[i];
        char b_char = b[i];

        if (a_char < b_char) {
            return -1;
        } else if (a_char > b_char) {
            return 1;
        }
    }

    if (a_len < b_len) {
        return -1;
    } else if (a_len > b_len) {
        return 1;
    }

    // This should never be possible for filenames in the same directory
    return 0;
}

static int partition(path_segment * a, int lo, int hi) {
    path_segment * pivot = a + lo;

    int i = lo - 1;
    int j = hi + 1;

    while (1) {
        do {
            i++;
        } while (str_cmp((a + i)->name, (a + i)->len, pivot->name, pivot->len) < 0);

        do {
            j--;
        } while (str_cmp((a + j)->name, (a + j)->len, pivot->name, pivot->len) > 0);

        if (i >= j) {
            return j;
        }

        path_segment tmp = a[i];
        a[i] = a[j];
        a[j] = tmp;
    }
}

// Quicksort with Hoare's partitioning scheme
static void quicksort(path_segment * a, int lo, int hi) {
    if (lo >= 0 && hi >= 0 && lo < hi) {
        int p = partition(a, lo, hi);

        quicksort(a, lo, p);
        quicksort(a, p + 1, hi);
    }
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

    XClearWindow(this->dis, this->win);
    XMapRaised(this->dis, this->win);

    XColor tmp;
    this->text_color = get_color(this->dis, this->screen, &tmp, "slate blue");
    this->file_color = get_color(this->dis, this->screen, &tmp, "slate gray");
    this->dir_color = get_color(this->dis, this->screen, &tmp, "yellow");
    this->debug_color = get_color(this->dis, this->screen, &tmp, "red");
    this->hover_color = get_color(this->dis, this->screen, &tmp, "gray78");
    this->no_perm_color = get_color(this->dis, this->screen, &tmp, "red");
    this->status_color = get_color(this->dis, this->screen, &tmp, "green");

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
    this->tmp_path[0] = '\0';
    this->tmp_path_len = 0;
    this->status[0] = '\0';
    this->status_len = 0;

    this->uid = getuid();
    this->gid = getgid();

    XGetWindowAttributes(this->dis, this->win, &this->window_attrs);
    this->back_buffer = XCreatePixmap(this->dis, this->win, this->window_attrs.width, this->window_attrs.height, 24);
    this->max_area = this->window_attrs.width * this->window_attrs.height;
}

window_context::~window_context() {
    int retval = closedir(this->dir);
    check_error(retval, -1);

    XFreePixmap(this->dis, this->back_buffer);
    XFreeGC(this->dis, this->gc);
    XDestroyWindow(this->dis, this->win);
    XCloseDisplay(this->dis);
}

int window_context::on_expose(XExposeEvent &event) {
    int old_w = this->window_attrs.width;
    int old_h = this->window_attrs.height;

    XGetWindowAttributes(this->dis, this->win, &this->window_attrs);

    int new_w = this->window_attrs.width;
    int new_h = this->window_attrs.height;

    if (new_w > old_w || new_h > old_h) {
        // Reallocate the pixmap
        XFreePixmap(this->dis, this->back_buffer);
        this->back_buffer = XCreatePixmap(this->dis, this->win, new_w, new_h, 24);
        this->max_area = new_w * new_h;
    } else {
        // Shrink the pixmap only if the new size is a quarter of the max. This is only to save some
        // memory - we don't actually care about whatever doesn't fit in the window
        int new_area = new_w * new_h;

        if (new_area * 4 <= this->max_area) {
            this->max_area = new_area;
            XFreePixmap(this->dis, this->back_buffer);
            this->back_buffer = XCreatePixmap(this->dis, this->win, new_w, new_h, 24);
        }
    }

    this->redraw();

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
                if (! this->has_permission(path)) {
                    this->set_status("No permission");
                } else {
                    if (S_ISDIR(path.mode)) {
                        this->navigate(path);
                    }
                    this->set_status("");
                }

                this->redraw();
            }
        }
    } else if (this->can_scroll && event.button == Button4) {
        if (this->scrollrow > 0) {
            this->scrollrow--;
        }

        this->redraw();
    } else if (this->can_scroll && event.button == Button5) {
        if (this->scrollrow < this->max_scrollrow) {
            this->scrollrow++;
        }

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
    int retval;

    while (i < MAX_CHILDREN && (entry = readdir(this->dir)) != NULL) {
        memcpy(this->children[i].name, entry->d_name, 256);

        this->children[i].len = strlen(this->children[i].name);

        memcpy(this->tmp_path, this->cwd, this->cwd_len + 1);
        this->tmp_path_len = this->cwd_len;
        this->path_join(this->tmp_path, &this->tmp_path_len, this->children[i]);

        retval = stat(this->tmp_path, &this->tmp_stat);
        check_error(retval, -1);

        this->children[i].mode = this->tmp_stat.st_mode;
        this->children[i].uid = this->tmp_stat.st_uid;
        this->children[i].gid = this->tmp_stat.st_gid;

        i++;
    }

    if (i < MAX_CHILDREN) {
        // The entry with len = 0 marks the end
        this->children[i].len = 0;
    }

    quicksort(this->children, 0, i - 1);
}

void window_context::redraw() {
    XSetForeground(this->dis, this->gc, this->black);
    XFillRectangle(this->dis, this->back_buffer, this->gc, 0, 0, this->window_attrs.width, this->window_attrs.height);
    XSetForeground(this->dis, this->gc, this->text_color);
    XDrawString(this->dis, this->back_buffer, this->gc, 0, 10, this->cwd, this->cwd_len);

    int y = 23;

    int i;
    for (i = this->scrollrow; i < MAX_CHILDREN; i++) {
        path_segment &path = this->children[i];

        if (path.len == 0 || y > (this->window_attrs.height - 10)) {
            break;
        }

        const bool has_perm = this->has_permission(path);
        const bool is_selected = this->mouse_y < y && this->mouse_y >= (y - ROW_HEIGHT);

        if (! has_perm) {
            XSetForeground(this->dis, this->gc, this->no_perm_color);
            XFillRectangle(this->dis, this->back_buffer, this->gc, 0, y - ROW_HEIGHT, this->window_attrs.width, ROW_HEIGHT);
        }

        if (is_selected) {
            XSetForeground(this->dis, this->gc, this->hover_color);
            XFillRectangle(this->dis, this->back_buffer, this->gc, 0, y - ROW_HEIGHT, this->window_attrs.width, ROW_HEIGHT);
        }

        if (! has_perm || is_selected) {
            XSetForeground(this->dis, this->gc, this->text_color);
        }

        XDrawString(this->dis, this->back_buffer, this->gc, 20, y, path.name, path.len);

        path.y_bot = y;

        if (this->debug_enabled) {
            unsigned int w = this->window_attrs.width;
            unsigned int h = ROW_HEIGHT;

            XSetForeground(this->dis, this->gc, this->debug_color);
            XDrawRectangle(this->dis, this->back_buffer, this->gc, 0, path.y_bot - ROW_HEIGHT, w, h);
            XSetForeground(this->dis, this->gc, this->text_color);
        }

        this->draw_filetype(y, path.mode);

        y += ROW_HEIGHT;
    }

    this->max_y = y;

    int screen_rows = (this->window_attrs.height - 20) / ROW_HEIGHT;
    this->can_scroll = i >= screen_rows;
    this->max_scrollrow = i - screen_rows + 2;

    // Draw the statusline
    XDrawLine(this->dis, this->back_buffer, this->gc, 0, (this->window_attrs.height - 10), this->window_attrs.width, (this->window_attrs.height - 10));

    if (this->status_len != 0) {
        XSetForeground(this->dis, this->gc, this->status_color);
        XDrawString(this->dis, this->back_buffer, this->gc, 0, this->window_attrs.height, this->status, this->status_len);
    }

    XSetForeground(this->dis, this->gc, this->text_color);
    XCopyArea(this->dis, this->back_buffer, this->win, this->gc, 0, 0, this->window_attrs.width, this->window_attrs.height, 0, 0);
}

void window_context::draw_filetype(int y, unsigned int mode) {
    const char * type_str;
    unsigned long type_color;

    if (S_ISDIR(mode)) {
        type_str = "d";
        type_color = this->dir_color;
    } else {
        type_str = "f";
        type_color = this->file_color;
    }

    XSetForeground(this->dis, this->gc, type_color);
    XDrawString(this->dis, this->back_buffer, this->gc, 5, y, type_str, 1);
    XSetForeground(this->dis, this->gc, this->text_color);
}

path_segment * window_context::get_selected_segment() {
    if (this->mouse_y > this->max_y || this->mouse_y < 10) {
        return nullptr;
    }

    int curr_row = (this->mouse_y - 10) / ROW_HEIGHT;

    return &this->children[curr_row];
}

void window_context::path_join(char * const wd, size_t * wd_len, path_segment &path) {
    if (path.len == 1 && path.name[0] == '.') {
        // Do nothing
        return;
    } else if (*wd_len == 1 && wd[0] == '/') {
        if (path.len == 2 && path.name[0] == '.' && path.name[1] == '.') {
            // Can't go up from root
            return;
        }

        // Root - just append the next thing
        memcpy(wd + 1, path.name, path.len);
        *wd_len = path.len + 1;
        wd[path.len + 1] = '\0';
    } else if (path.len == 2 && path.name[0] == '.' && path.name[1] == '.') {
        // Strip out last part of path
        int slashpos = *wd_len;

        while (slashpos && (wd[--slashpos] != '/'));

        if (slashpos == 0) {
            // Last slash is root, so leave it in
            wd[1] = '\0';
            *wd_len = 1;
        } else {
            *wd_len = slashpos;
            wd[slashpos] = '\0';
        }
    } else {
        wd[*wd_len] = '/';
        memcpy(wd + *wd_len + 1, path.name, path.len);
        *wd_len += path.len + 1;
        wd[*wd_len] = '\0';
    }
}

void window_context::navigate(path_segment &path) {
    this->path_join(this->cwd, &this->cwd_len, path);

    if (this->dir) {
        int retval = closedir(this->dir);
        check_error(retval, -1);
    }

    this->dir = opendir(this->cwd);
    check_error(this->dir, (DIR *) NULL);

    this->read_child_dirs(0);
    this->scrollrow = 0;
}

bool window_context::has_permission(path_segment &path) {
    if (S_ISDIR(path.mode)) {
        return (S_IXOTH & path.mode) || ((S_IXUSR & path.mode) && this->uid == path.uid) || ((S_IXGRP & path.mode) && this->gid == path.gid);
    }

    return (S_IROTH & path.mode) || ((S_IRUSR & path.mode) && this->uid == path.uid) || ((S_IRGRP & path.mode) && this->gid == path.gid);
}

void window_context::set_status(const char * const text) {
    this->status_len = strlen(text);
    memcpy(this->status, text, this->status_len + 1);
}
