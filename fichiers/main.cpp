#include <cstdlib>
#include <iostream>
#include <vector>
#include <memory>
#include <SFML/Graphics.hpp>

using namespace std;
using namespace sf;

#ifdef _MSC_VER
#pragma comment(linker, "/SUBSYSTEM:windows /ENTRY:mainCRTStartup")
#define _PATH_IMG_ "C:/Program Files/SFML/img/"
#else
#define _PATH_IMG_ "./img/"
#endif

const std::string path_image(_PATH_IMG_);

int main() {

    VideoMode desktop = VideoMode::getDesktopMode();

    RenderWindow app(desktop, "My Camera", State::Fullscreen);
    app.setFramerateLimit(60);

    Texture backgroundImage;
    if (!backgroundImage.loadFromFile("C:/Users/ivill/source/repos/Projet/fichiers/assets/France.jpg"))
        return -1;

    Sprite backgroundSprite(backgroundImage);

    // Calcul de l'échelle uniforme
    Vector2u windowSize = app.getSize();
    Vector2u textureSize = backgroundImage.getSize();

    float scaleX = static_cast<float>(windowSize.x) / static_cast<float>(textureSize.x);
    float scaleY = static_cast<float>(windowSize.y) / static_cast<float>(textureSize.y);
    float scale = std::min(scaleX, scaleY);

    backgroundSprite.setScale(Vector2f(scale, scale));

    // Récupération des bounds après mise à l'échelle
    FloatRect bounds = backgroundSprite.getGlobalBounds();

   
    // Centrer le sprite dans la fenêtre
    backgroundSprite.setPosition(Vector2f(
        (static_cast<float>(windowSize.x) - bounds.size.x) / 2.f,
        (static_cast<float>(windowSize.y) - bounds.size.y) / 2.f
    ));

    while (app.isOpen())
    {
        // Gestion des événements SFML 3
        while (const std::optional<Event> event = app.pollEvent())
        {
            if (event->is<Event::Closed>())
                app.close();
        }

        app.clear();
        app.draw(backgroundSprite);
        app.display();
    }

    return 0;
}
