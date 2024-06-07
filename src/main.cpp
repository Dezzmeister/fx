#include <stdio.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xos.h>
#include "../include/window_context.h"

const char * const EXIT_CODES[] = {
    "",
    "Received quit command",
    "This should never be printed!",
};

int main(int argc, char ** argv) {
    window_context ctx(0, 0, 500, 500, "fx");
    XEvent event;
    int retval;

    while(1) {
        XNextEvent(ctx.dis, &event);

        if (event.type == Expose && event.xexpose.count == 0) {
            retval = ctx.on_expose(event.xexpose);
        }

        if (event.type == ButtonPress) {
            retval = ctx.on_button_press(event.xbutton);
        }

        if (event.type == KeyPress) {
            retval = ctx.on_key_press(event.xkey);
        }

        if (event.type == MotionNotify) {
            retval = ctx.on_motion(event.xmotion);
        }

        if (retval) {
            if (retval != USER_CD_EXIT_CODE) {
                printf("Exit: %s\n", EXIT_CODES[retval]);
            }

            return 0;
        }
    }
}
