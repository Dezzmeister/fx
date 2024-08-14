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
