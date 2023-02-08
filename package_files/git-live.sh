mkdir -p ~/.cache/git-live/workdirs/
export GIT_LIVE_TTY_HASH="`echo $RANDOM | sha256sum | head -c 8`"
export PROMPT_COMMAND="echo \$PWD > ~/.cache/git-live/workdirs/${GIT_LIVE_TTY_HASH}; $PROMPT_COMMAND"
