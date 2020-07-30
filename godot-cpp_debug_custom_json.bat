cd godot-cpp
scons platform=windows bits=64 target=debug generate_bindings=yes use_custom_api_file=yes custom_api_file=../api.json -j12
cd ..