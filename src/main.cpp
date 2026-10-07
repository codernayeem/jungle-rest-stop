// =============================================================================
//  Jungle Rest Stop  -  Computer Graphics Lab Project (OpenGL 3.3 core)
//  Student: Md. Nayeem   Roll: 2107050
//
//  Command line:
//    main.exe               normal run (1280x720 window)
//    main.exe --low         low quality (smaller shadow map, fewer plants)
//    main.exe --fullscreen  full screen
//    main.exe --shots DIR   render the report screenshots into DIR and exit
//    main.exe --video F.mp4 render a short cinematic video (needs ffmpeg) and exit
// =============================================================================
#include "app/App.h"
#include <cstdlib>
#include <cstring>
#include <iostream>

int main(int argc, char** argv) {
    AppOptions opt;
    for (int i = 1; i < argc; ++i) {
        if (!std::strcmp(argv[i], "--low")) opt.lowQuality = true;
        else if (!std::strcmp(argv[i], "--fullscreen")) opt.fullscreen = true;
        else if (!std::strcmp(argv[i], "--shots") && i + 1 < argc) opt.shotsDir = argv[++i];
        else if (!std::strcmp(argv[i], "--video") && i + 1 < argc) opt.videoPath = argv[++i];
        else if (!std::strcmp(argv[i], "--size") && i + 2 < argc) { opt.width = std::atoi(argv[i + 1]); opt.height = std::atoi(argv[i + 2]); i += 2; }
    }
    std::cout << "==============================================================\n"
              << "  JUNGLE REST STOP  -  OpenGL 3.3 Computer Graphics Project\n"
              << "  Md. Nayeem  |  Roll 2107050\n"
              << "==============================================================\n"
              << "  Press H in the window for the full list of controls.\n\n";
    App app;
    if (!app.init(opt)) return 1;
    app.run();
    app.shutdown();
    return 0;
}
