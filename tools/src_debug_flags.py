# Build our own src/ with full DWARF (-g) debug info while the Arduino core and
# every third-party library stay at line tables only (-g1).
#
# Why this needs to run from a script rather than platformio.ini alone:
#   * The Arduino framework appends "-gdwarf-4 -ggdb" to CCFLAGS *after* our
#     [base]build_flags. -gdwarf-4 implies a debug *level* of 2, so it silently
#     overrode the -g1 we set, which is why the flag appeared to do nothing.
#     [env]build_unflags strips both, which makes -g1 the effective level.
#   * PlatformIO applies SRC_BUILD_FLAGS only to project sources and after the
#     framework flags, so it is the one hook where -g wins for src/ without
#     having to repeat the flag in all ~50 environments' build_src_flags.
Import("env")

flags = list(env.get("SRC_BUILD_FLAGS") or [])
if "-g" not in flags:
    flags.append("-g")
env.Replace(SRC_BUILD_FLAGS=flags)
print("[Debug-Flags] Project sources will be built with full -g debug info.")
