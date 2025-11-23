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

//Structure pour gérer une ville
struct Ville {
    string nom;
    Vector2f positionRelative;
    CircleShape marqueur;
    Text texte;
    string cheminAeroport;
};


int main() {

    VideoMode desktop = VideoMode::getDesktopMode();
    RenderWindow app(desktop, "My Camera", State::Fullscreen);
    app.setFramerateLimit(60);

    //Charger la police
    Font font;
    if (!font.openFromFile(path_image + "arial.ttf"))
        return -1;

    //Etat actuel (0 = carte France, 1-6 = aéroports)
    int etatAct = 0;

    Texture backgroundImage;
    if (!backgroundImage.loadFromFile(path_image + "France.jpg"))
        return -1;

    Sprite backgroundSprite(backgroundImage);

    //Fonction pour mettre le fond à jour
    auto updateBackground = [&](const string& path) {
        if (backgroundImage.loadFromFile(path)) {
            backgroundSprite.setTexture(backgroundImage);

            Vector2u windowSize = app.getSize();
            Vector2u textureSize = backgroundImage.getSize();

            float scaleX = static_cast<float>(windowSize.x) / static_cast<float>(textureSize.x);
            float scaleY = static_cast<float>(windowSize.y) / static_cast<float>(textureSize.y);
            float scale = std::min(scaleX, scaleY);

            backgroundSprite.setScale(Vector2f(scale, scale));
            FloatRect bounds = backgroundSprite.getGlobalBounds();
            backgroundSprite.setPosition(Vector2f(
                (static_cast<float>(windowSize.x) - bounds.size.x) / 2.f,
                (static_cast<float>(windowSize.y) - bounds.size.y) / 2.f));
        }
        };

    // Calcul de l'échelle uniforme
    Vector2u windowSize = app.getSize();
    Vector2u textureSize = backgroundImage.getSize();

    float scaleX = static_cast<float>(windowSize.x) / static_cast<float>(textureSize.x);
    float scaleY = static_cast<float>(windowSize.y) / static_cast<float>(textureSize.y);
    float scale = std::min(scaleX, scaleY);

    backgroundSprite.setScale(Vector2f(scale, scale));
    FloatRect bounds = backgroundSprite.getGlobalBounds();
    backgroundSprite.setPosition(Vector2f(
        (static_cast<float>(windowSize.x) - bounds.size.x) / 2.f,
        (static_cast<float>(windowSize.y) - bounds.size.y) / 2.f
    ));

    //Définir les villes avec leurs données
    vector<Ville> villes = {
        {"Lille", {0.54f, 0.05f}, CircleShape(8.f), Text(font), path_image + "avion.png"},
        {"Paris", {0.5f, 0.25f}, CircleShape(8.f), Text(font), path_image + "avion.png"},
        {"Strasbourg", {0.84f, 0.27f}, CircleShape(8.f), Text(font), path_image + "avion.png"},
        {"Rennes", {0.23f, 0.34f}, CircleShape(8.f), Text(font), path_image + "avion.png"},
        {"Nice", {0.83f, 0.83f}, CircleShape(8.f), Text(font), path_image + "avion.png"},
        {"Toulouse", {0.40f, 0.85f}, CircleShape(8.f), Text(font), path_image + "avion.png"}
    };

    // Configurer les marqueurs et textes
    for (auto& ville : villes) {
        // Marqueur
        ville.marqueur.setFillColor(Color::Red);
        ville.marqueur.setOutlineThickness(2.f);
        ville.marqueur.setOutlineColor(Color::Black);

        float X = backgroundSprite.getPosition().x + ville.positionRelative.x * bounds.size.x;
        float Y = backgroundSprite.getPosition().y + ville.positionRelative.y * bounds.size.y;

        ville.marqueur.setOrigin(Vector2f(ville.marqueur.getRadius(), ville.marqueur.getRadius()));
        ville.marqueur.setPosition(Vector2f(X, Y));

        // Texte
        ville.texte.setString(ville.nom);
        ville.texte.setCharacterSize(20);
        ville.texte.setFillColor(Color::Black);
        ville.texte.setOutlineThickness(1.f);
        ville.texte.setOutlineColor(Color::Black);
        ville.texte.setPosition(Vector2f(X + 15.f, Y - 10.f));
    }

    while (app.isOpen())
    {
        // Gestion des événements SFML 3
        while (const std::optional<Event> event = app.pollEvent())
        {
            if (event->is<Event::Closed>())
                app.close();

            // Détecter les clics de souris DANS la boucle d'événements
            if (event->is<Event::MouseButtonPressed>()) {
                const auto& mouseEvent = event->getIf<Event::MouseButtonPressed>();
                if (mouseEvent->button == Mouse::Button::Left) {
                    Vector2f mousePos(static_cast<float>(mouseEvent->position.x),
                        static_cast<float>(mouseEvent->position.y));

                    // Vérifier si on clique sur une ville
                    if (etatAct == 0) {
                        for (size_t i = 0; i < villes.size(); ++i) {
                            if (villes[i].marqueur.getGlobalBounds().contains(mousePos)) {
                                updateBackground(villes[i].cheminAeroport);
                                etatAct = i + 1;
                                break;
                            }
                        }
                    }
                }
                // Clic droit pour revenir à la carte
                else if (mouseEvent->button == Mouse::Button::Right && etatAct != 0) {
                    updateBackground(path_image + "France.jpg");
                    etatAct = 0;
                }
            }
        }

        app.clear();
        app.draw(backgroundSprite);

        // Dessiner marqueurs et noms seulement sur la carte de France
        if (etatAct == 0) {
            for (const auto& ville : villes) {
                app.draw(ville.marqueur);
                app.draw(ville.texte);
            }
        }

        app.display();
    }

    return 0;
}
