// app.h
#pragma once
#include <string>
struct App {
    const std::string version = "0.0.1-beta.1";
    const std::string name = "mp4-to-amv";
    const std::string author = "You";
    const int  build = 20261025;
};

inline const App app;