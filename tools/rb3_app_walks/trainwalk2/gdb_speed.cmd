set pagination off
set breakpoint pending on
handle SIGPIPE SIGUSR1 SIGUSR2 nostop noprint pass
break TrainerPanel::SetLessonComplete
commands
silent
printf "GDB TP::SetLessonComplete idx=%d\n", $rsi
continue
end
break BandProfile::SetLessonComplete
commands
silent
printf "GDB BP::SetLessonComplete %s %f\n", *(char**)$rsi, $xmm0.v4_float[0]
continue
end
break BandProfile::IsLessonComplete
commands
silent
printf "GDB BP::IsLessonComplete %s %f\n", *(char**)$rsi, $xmm0.v4_float[0]
continue
end
break CheckboxDisplay::SetChecked
commands
silent
printf "GDB SetChecked %d\n", $rsi
continue
end
break BandProfile::SaveFixed
commands
silent
printf "GDB BP::SaveFixed\n"
continue
end
break BandProfile::LoadFixed
commands
silent
printf "GDB BP::LoadFixed rev=%d\n", $rdx
continue
end
break SaveLoadManager::AutoSave
commands
silent
printf "GDB SLM::AutoSave\n"
continue
end
break AccomplishmentProgress::AddAccomplishment
commands
silent
printf "GDB AddAccomplishment %s\n", (char*)$rsi
continue
end
break AccomplishmentManager::CheckForFinishedTrainerAccomplishmentsForUser
commands
silent
printf "GDB CheckForFinishedTrainerAcc\n"
continue
end
break GemTrainerPanel::SetSpeedRatio
commands
silent
printf "GDB SetSpeedRatio %f\n", $xmm0.v4_float[0]
continue
end
run
