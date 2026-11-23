set pagination off
set breakpoint pending on
break SongPreview::Start(Symbol)
commands
silent
printf "PREVIEW Start %s\n", (char*)$rsi
continue
end
break SongPreview::PrepareSong(Symbol)
commands
silent
printf "PREVIEW PrepareSong %s\n", (char*)$rsi
continue
end
break SongPreview::PreparePreview()
commands
silent
printf "PREVIEW PreparePreview\n"
continue
end
break SongPreview::Init()
commands
silent
printf "PREVIEW Init\n"
continue
end
break SongPreview::ContentMounted(char const*, char const*)
commands
silent
printf "PREVIEW ContentMounted %s\n", (char*)$rsi
continue
end
run
