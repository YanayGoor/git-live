#!/bin/sh
mkdir -p ~/.cache/git-live/attached/
echo $GIT_LIVE_TTY_HASH > ~/.cache/git-live/attached/${1}
