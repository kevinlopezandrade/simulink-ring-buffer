#!/bin/env zsh

SHARED_FILE="/dev/shm/testing"

if [ -f $SHARED_FILE ]; then
    rm $SHARED_FILE
fi

matrun main
