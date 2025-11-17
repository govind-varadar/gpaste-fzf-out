#!/usr/bin/bash

GPASTE_FZF_OUT=$HOME/bin/gpaste-fzf-out/gpaste-fzf-out

kitty bash -c "$GPASTE_FZF_OUT --read0 | fzf --reverse \
	--read0 \
	--exact \
	--header-lines=1 \
	--delimiter=":" \
	--with-nth=5.. \
	--bind 'enter:become(setsid -f nohup gpaste-client select {4} 2>&1 > /dev/null)' \
	--bind 'ctrl-d:reload(gpaste-client delete {4} ; $GPASTE_FZF_OUT --read0)' \
	--bind 'ctrl-r:reload($GPASTE_FZF_OUT --read0)' \
	"
