import os
import tempfile


def on_upload(source, target, env):
    firmware_path = str(source[0])

    flash_start = 0x08000000
    app_start = flash_start
    bootloader = None

    upload_flags = env.get('UPLOAD_FLAGS', [])
    for line in upload_flags:
        for flag in line.split():
            if "BOOTLOADER=" in flag:
                bootloader = flag.split("=")[1]
            elif "VECT_OFFSET=" in flag:
                offset = flag.split("=")[1]
                offset = int(offset, 16) if "0x" in offset else int(offset, 10)
                app_start = flash_start + offset

    jlink_exe = os.path.join(env['PROJECT_PACKAGES_DIR'], "tool-jlink", "JLinkExe")

    script_lines = [
        "si SWD",
        "speed 4000",
        "device STM32F103CB",
        "connect",
    ]
    if bootloader:
        script_lines.append('loadfile "%s" %s' % (bootloader, hex(flash_start)))
    script_lines.append('loadfile "%s" %s' % (firmware_path, hex(app_start)))
    script_lines += ["r", "g", "exit"]

    with tempfile.NamedTemporaryFile(mode='w', suffix='.jlink', delete=False) as f:
        f.write("\n".join(script_lines) + "\n")
        script_path = f.name

    try:
        cmd = '"%s" -nogui 1 -CommanderScript "%s"' % (jlink_exe, script_path)
        print("Cmd: %s" % cmd)
        retval = env.Execute(cmd)
    finally:
        os.unlink(script_path)

    return retval
