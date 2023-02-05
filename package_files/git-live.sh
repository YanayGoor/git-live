#!/bin/sh
tty_hash="`tty | sha256sum | head -c 8`"
export PROMPT_COMMAND="echo \$PWD > ~/.git-live/${tty_hash}; $PROMPT_COMMAND"
