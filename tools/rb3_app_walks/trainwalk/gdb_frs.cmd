set pagination off
set breakpoint pending on
handle SIGPIPE SIGUSR1 SIGUSR2 nostop noprint pass
break FreestylePanel::Swing
commands
silent
printf "GDB Swing slot=%d\n", $rsi
continue
end
break FreestylePanel::SetBpm
commands
silent
printf "GDB SetBpm %d\n", $rsi
continue
end
break FreestylePanel::CreateController
commands
silent
printf "GDB CreateController\n"
continue
end
break Metronome::PlayBeat
commands
silent
printf "GDB PlayBeat %d\n", $rsi
continue
end

break Sequence::Play
commands
silent
printf "GDB SeqPlay from:\n"
bt 3
continue
end
run
