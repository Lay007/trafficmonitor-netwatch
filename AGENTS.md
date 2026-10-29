# NetWatch project instructions

- Public source, comments and documentation are written in English. User conversation is in Russian.
- Target Windows x64, MSVC, C++17. Use the unmodified TrafficMonitor V1.86 API v7 header in vendor/.
- Do not modify or deploy to IPRegionMon or an installed TrafficMonitor. Integration artifacts belong in .work/.
- Build: `cmake --preset windows-x64`; `cmake --build --preset release`.
- Test: `ctest --preset release --output-on-failure`.
- No network I/O in host callbacks, drawing, or under the engine mutex. At most four active probe workers and four launches per second, including manual tests.
- Every DLL worker must retain a module reference and release it with FreeLibraryAndExitThread after destroying its C++ state. Never wait or perform I/O in DllMain/static destructors.
- Keep group and check IDs stable. Host item objects and their metadata are immutable until unload.
- Changes to exported display layout require a host restart. Do not mutate the host's cached item list.
- Do not disable TLS validation, follow redirects to HTTP, or add mandatory public monitoring targets.
- Keep actual verification evidence and limitations in docs/verification.md. Private research notes belong in ignored .private/.
