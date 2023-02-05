#!/bin/sh
tty_hash="`tty | sha256sum | head -c 8`"
echo ${tty_hash} > ~/.git-live/${1}
