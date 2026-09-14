from SCons.Script import Import

Import("env")

# Override OBJCOPY to use generic xtensa-esp-elf-objcopy provided by GCC 14 toolchain package
# instead of target-specific names (like xtensa-esp32-elf-objcopy or xtensa-esp32s3-elf-objcopy)
env.Replace(OBJCOPY="xtensa-esp-elf-objcopy")
