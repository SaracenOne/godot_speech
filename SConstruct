#!python
import os

opus_path = ARGUMENTS.get("opus_path", None)
opus_library_path = ARGUMENTS.get("opus_library", None)
use_builtin_opus = not opus_path or not opus_library_path

libsamplerate_path = ARGUMENTS.get("libsamplerate_path", None)
libsamplerate_library_path = ARGUMENTS.get("libsamplerate_library", None)
use_builtin_libsamplerate = not libsamplerate_path or not libsamplerate_library_path

# platform= makes it in line with Godots scons file, keeping p for backwards compatibility
platform = ARGUMENTS.get("p", "linux")
platform = ARGUMENTS.get("platform", platform)
target_arch = ARGUMENTS.get('a', ARGUMENTS.get('arch', '64'))

# This makes sure to keep the session environment variables on windows, 
# that way you can run scons in a vs 2017 prompt and it will find all the required tools
if platform == "windows":
    env = Environment(ENV = os.environ)
else:
    env = Environment()

Export("env")

godot_headers_path = ARGUMENTS.get("headers", os.getenv("GODOT_HEADERS", "godot-cpp/godot_headers"))
godot_bindings_path = ARGUMENTS.get("cpp_bindings", os.getenv("CPP_BINDINGS", "godot-cpp"))
external_path = 'src/external'


# default to debug build, must be same setting as used for cpp_bindings
target = ARGUMENTS.get("target", "debug")

platform_suffix = '.' + platform + '.' + ("release" if target == "release_debug" else target) + '.' + target_arch

if ARGUMENTS.get("use_llvm", "no") == "yes":
    env["CXX"] = "clang++"

# put stuff that is the same for all first, saves duplication 
if platform == "osx":
    env.Append(CCFLAGS = ['-g','-O3', '-std=c++14', '-arch', 'x86_64'])
    env.Append(LINKFLAGS = ['-arch', 'x86_64', '-framework', 'Cocoa', '-Wl,-undefined,dynamic_lookup'])
    platform_suffix = ''
elif platform == "linux":
    env.Append(CCFLAGS = ['-g','-O3', '-std=c++14', '-Wno-writable-strings'])
    env.Append(LINKFLAGS = ['-Wl,-R,\'$$ORIGIN\''])
elif platform == "windows":
    # need to add detection of msvc vs mingw, this is for msvc...
    if target == "debug":
        env.Append(CCFLAGS = ['-EHsc', '-D_DEBUG', '/MDd', '/Zi', '/FS'])
        env.Append(LINKFLAGS = ['/WX', '/DEBUG:FULL'])
    elif target == "release_debug":
        env.Append(CCFLAGS = ['-O2', '-EHsc', '-DNDEBUG', '/MD', '/Zi', '/FS'])
        env.Append(LINKFLAGS = ['/WX', '/DEBUG:FULL'])
    else:
        env.Append(CCFLAGS = ['-O2', '-EHsc', '-DNDEBUG', '/MD'])
        env.Append(LINKFLAGS = ['/WX'])

def add_sources(sources, dir):
    for f in os.listdir(dir):
        if f.endswith(".cpp"):
            sources.append(dir + "/" + f)

def add_suffix(libs):
    return [lib + platform_suffix for lib in libs]

env.Append(CPPPATH=[godot_headers_path,
	godot_bindings_path + '/include/',
	godot_bindings_path + '/include/core/',
	godot_bindings_path + '/include/gen/'] + 
        ([opus_path] if not use_builtin_opus else []) +
        ([libsamplerate_path] if not use_builtin_libsamplerate else []))

env.Append(LIBS=[add_suffix(['libgodot-cpp'])] +
        ([opus_library_path] if not use_builtin_opus else []) +
        ([libsamplerate_library_path] if not use_builtin_libsamplerate else []))

env.Append(LIBPATH=[godot_bindings_path + '/bin/'])

env["builtin_opus"] = use_builtin_opus
env["builtin_libsamplerate"] = use_builtin_libsamplerate
env["platform"] = platform

sources = []
env.modules_sources = sources

SConscript("SCsub")

add_sources(sources, "./src")

library = env.SharedLibrary(target='bin/' + target + '/libGodotSpeech', source=sources)
Default(library)
