#!/bin/bash

# This file is part of fx, a graphical file explorer.
# Copyright (C) 2024  Joe Desmond
#
# fx is free software: you can redistribute it and/or modify
# it under the terms of the GNU Affero General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# any later version.
#
# fx is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Affero General Public License for more details.
#
# You should have received a copy of the GNU Affero General Public License
# along with fx.  If not, see <https://www.gnu.org/licenses/>.

RESULT=$(fx_bin)

if [[ -z "$RESULT" ]] then
    echo "$RESULT"
else
    $RESULT
fi
