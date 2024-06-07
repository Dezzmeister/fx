#!/bin/bash

RESULT=$(fx_bin)

if [[ -z "$RESULT" ]] then
    echo "$RESULT"
else
    $RESULT
fi
