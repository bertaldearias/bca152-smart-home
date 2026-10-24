
Import("env")

import os
import subprocess

build_dir = env.subst("$BUILD_DIR")
ninja_exe = os.path.join(
    os.path.expanduser("~"),
    ".platformio", "packages", "tool-ninja", "ninja.exe"
)

certificates = [
    "rmaker_mqtt_server.crt.S",
    "rmaker_claim_service_server.crt.S",
    "rmaker_ota_server.crt.S",
    "mqtt_server.crt.S",
]

def generate_certificates(source, target, env):
    ninja_file = os.path.join(build_dir, "build.ninja")

    if not os.path.isfile(ninja_file):
        raise RuntimeError("build.ninja not found")

    print("Generating RainMaker certificates before compilation...")

    subprocess.run(
        [ninja_exe, "-C", build_dir, *certificates],
        check=True
    )

# Register actions for the actual Ninja object paths
for certificate in certificates:
    object_file = os.path.join(
        build_dir,
        "esp-idf",
        "esp_rainmaker",
        "CMakeFiles",
        "__idf_esp_rainmaker.dir",
        "__",
        "__",
        certificate + ".obj"
    )

    env.AddPreAction(object_file, generate_certificates)
