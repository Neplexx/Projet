#include <cstdlib>
#include <iostream>
#include <vector>
#include <memory>
#include <SFML/Graphics.hpp>

//#include "Plane.hpp"

using namespace std;
using namespace sf;

#ifdef _MSC_VER
#pragma comment(linker, "/SUBSYSTEM:windows /ENTRY:mainCRTStartup")
#define _PATH_IMG_ "C:/Program Files/SFML/img/"
#else
// #define _PATH_IMG_ "../img/"
#define _PATH_IMG_ "./img/"
#endif

const std::string path_image(PATH_IMG);

