// app.h
#pragma once
#include <string>
struct App {
    const std::string version = "1.0.0";
    const std::string name = "mp4-to-amv";
    const std::string author = "You";
    const int  build = 20261018;
};

inline const App app;