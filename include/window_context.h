#ifndef INCLUDE_WINDOW_CONTEXT_H
#define INCLUDE_WINDOW_CONTEXT_H

#include <dirent.h>
#include <linux/limits.h>
#include <X11/Xlib.h>

#define NO_EXIT             0
#define USER_QUIT_EXIT_CODE 1

const size_t MAX_CHILDREN = 1024;
// Must be >= 256
const size_t MAX_PATH_SEGMENT_SIZE = 256;

const size_t ROW_HEIGHT = 13;

struct path_segment {
    char name[MAX_PATH_SEGMENT_SIZE];
    size_t len;
    int y_bot;
    unsigned char type;
};

class window_context {
    public:
        Display * dis;
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

        window_context(int x, int y, unsigned int width, unsigned int height, const char * const title);

        int on_expose(XExposeEvent &event);

        int on_button_press(XButtonEvent &event);

        int on_key_press(XKeyEvent &event);

        int on_motion(XMotionEvent &event);

        void set_debug_mode(bool enabled);

        ~window_context();

    private:
        XEvent send_event;

        bool debug_enabled;

        void read_child_dirs(int start_at);

        void redraw();

        void draw_filetype(int y, unsigned char type);

        path_segment * get_selected_segment();

        void navigate(path_segment &path);
};

#endif
