export GIT_LIVE_DIR=${GIT_LIVE_DIR:-~/.cache/git-live}
export GIT_LIVE_TERMINAL_ID=$(xxd -ps -l8 /dev/urandom)
export PROMPT_COMMAND="mkdir -p \$GIT_LIVE_DIR/terminals;echo \$PWD > \$GIT_LIVE_DIR/terminals/\$GIT_LIVE_TERMINAL_ID;$PROMPT_COMMAND"
