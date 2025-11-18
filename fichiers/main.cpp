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

int main() {


    RenderWindow app(VideoMode({ 735, 633 }, 32), "My Camera");
    app.setFramerateLimit(60); // limite la fenêtre à 60 images par seconde

    Texture backgroundImage;
    if (!backgroundImage.loadFromFile("C:/Users/ivill/source/repos/Projet/fichiers/assets/France.jpg"))
        return -1; // Erreur chargement

    Sprite backgroundSprite(backgroundImage);

    while (app.isOpen())
    {
        // Gestion des événements SFML 3
        while (auto event = app.pollEvent())
        {
            if (event->is<sf::Event::Closed>())
                app.close();
        }

        app.clear();   // Vide la fenêtre
		app.draw(backgroundSprite); // Dessine l'arrière-plan
        app.display(); // Affiche le contenu (ici : rien)
    }

    return 0;
}