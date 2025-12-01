savedcmd_ko5204.mod := printf '%s\n'   ko5204.o | awk '!x[$$0]++ { print("./"$$0) }' > ko5204.mod
