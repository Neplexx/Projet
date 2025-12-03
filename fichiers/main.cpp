#include <cstdlib>
#include <iostream>
#include <vector>
#include <memory>
#include <SFML/Graphics.hpp>

using namespace std;
using namespace sf;

#ifdef _MSC_VER
#pragma comment(linker, "/SUBSYSTEM:windows /ENTRY:mainCRTStartup")
//#define _PATH_IMG_ "C:/Program Files/SFML/img/"
#else
#define _PATH_IMG_ "assets/"
#endif

const std::string path_image(PATH_IMG);

//Structure pour gérer une ville
struct Ville {
    string nom;
    Vector2f positionRelative;
    CircleShape marqueur;
    Text texte;
    string cheminAeroport;
};

//Initialisation de la fenêtre
RenderWindow initWindow() {
    VideoMode desktop = VideoMode::getDesktopMode();
    RenderWindow app(desktop, "My Camera", State::Fullscreen);
    app.setFramerateLimit(60);
    return app;
}

//Chargement police
Font loadFont() {
    Font font;
    if (!font.openFromFile(path_image + "arial.ttf"))
        exit(-1);
    return font;
}

//Chargement de l'image de fond
Texture loadBackgroundImage() {
    Texture backgroundImage;
    if (!backgroundImage.loadFromFile(path_image + "France.jpg"))
        exit(-1);
    return backgroundImage;
}

//Initialisation de fond
Sprite createBackgroundSprite(Texture& backgroundImage) {
    Sprite backgroundSprite(backgroundImage);
    return backgroundSprite;
}

//Mise à jour du fond
auto makeUpdateBackground(RenderWindow& app, Texture& backgroundImage, Sprite& backgroundSprite) {
    return [&](const string& path) {
        if (backgroundImage.loadFromFile(path)) {
            backgroundSprite.setTexture(backgroundImage);

            Vector2u windowSize = app.getSize();
            Vector2u textureSize = backgroundImage.getSize();

            //Calcul de l'échelle 
            float scaleX = static_cast<float>(windowSize.x) / textureSize.x;
            float scaleY = static_cast<float>(windowSize.y) / textureSize.y;
            float scale = std::min(scaleX, scaleY);

            backgroundSprite.setOrigin({ textureSize.x / 2.f, textureSize.y / 2.f });

            //Centrer
            backgroundSprite.setPosition({ windowSize.x / 2.f, windowSize.y / 2.f });

            //Mise à l'échelle
            backgroundSprite.setScale({ scale, scale });
        }
        };
}

// L'échelle et la position
void updateScaleAndPosition(RenderWindow& app, Sprite& backgroundSprite, Texture& backgroundImage) {
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
}

//Les villes
vector<Ville> createVilles(Font& font) {
    return {
        {"Lille", {0.54f, 0.05f}, CircleShape(8.f), Text(font), path_image + "aeroport.png"},
        {"Paris", {0.5f, 0.25f}, CircleShape(8.f), Text(font), path_image + "aeroport.png"},
        {"Strasbourg", {0.84f, 0.27f}, CircleShape(8.f), Text(font), path_image + "aeroport.png"},
        {"Rennes", {0.23f, 0.34f}, CircleShape(8.f), Text(font), path_image + "aeroport.png"},
        {"Nice", {0.83f, 0.83f}, CircleShape(8.f), Text(font), path_image + "aeroport.png"},
        {"Toulouse", {0.40f, 0.85f}, CircleShape(8.f), Text(font), path_image + "aeroport.png"}
    };
}

//Tous les marqueurs et textes des villes
void initMarkersTexts(vector<Ville>& villes, Sprite& backgroundSprite, FloatRect bounds) {
    for (auto& ville : villes) {
        ville.marqueur.setFillColor(Color::Red);
        ville.marqueur.setOutlineThickness(2.f);
        ville.marqueur.setOutlineColor(Color::Black);

        float X = backgroundSprite.getPosition().x + ville.positionRelative.x * bounds.size.x;
        float Y = backgroundSprite.getPosition().y + ville.positionRelative.y * bounds.size.y;

        ville.marqueur.setOrigin(Vector2f(ville.marqueur.getRadius(), ville.marqueur.getRadius()));
        ville.marqueur.setPosition(Vector2f(X, Y));

        ville.texte.setString(ville.nom);
        ville.texte.setCharacterSize(20);
        ville.texte.setFillColor(Color::Black);
        ville.texte.setOutlineThickness(1.f);
        ville.texte.setOutlineColor(Color::Black);
        ville.texte.setPosition(Vector2f(X + 15.f, Y - 10.f));
    }
}

//Fenêtre principale
template<typename F>
void runApp(RenderWindow& app, Sprite& backgroundSprite, vector<Ville>& villes, F updateBackground, int& etatAct) {
    while (app.isOpen()) {
        while (const std::optional<Event> event = app.pollEvent()) {
            if (event->is<Event::Closed>())
                app.close();

            if (event->is<Event::KeyPressed>()) {
                const auto& keyEvent = event->getIf<Event::KeyPressed>();
                if (keyEvent->code == Keyboard::Key::Escape) {
                    app.close();
                }
            }

            //Détecter les clics
            if (event->is<Event::MouseButtonPressed>()) {
                const auto& mouseEvent = event->getIf<Event::MouseButtonPressed>();
                if (mouseEvent->button == Mouse::Button::Left) {
                    Vector2f mousePos(static_cast<float>(mouseEvent->position.x),
                        static_cast<float>(mouseEvent->position.y));

                    //Vérifier si on clique sur une ville
                    if (etatAct == 0) {
                        for (size_t i = 0; i < villes.size(); ++i) {
                            if (villes[i].marqueur.getGlobalBounds().contains(mousePos) || villes[i].texte.getGlobalBounds().contains(mousePos)) {
                                updateBackground(villes[i].cheminAeroport);
                                etatAct = i + 1;
                                break;
                            }
                        }
                    }
                }
                //Clic droit pour revenir à la carte
                else if (mouseEvent->button == Mouse::Button::Right && etatAct != 0) {
                    updateBackground(path_image + "France.jpg");
                    etatAct = 0;
                }
            }
        }

        app.clear();
        app.draw(backgroundSprite);

        //Dessiner marqueurs et noms 
        if (etatAct == 0) {
            for (const auto& ville : villes) {
                app.draw(ville.marqueur);
                app.draw(ville.texte);
            }
        }

        app.display();
    }
}

int main() {
    RenderWindow app = initWindow();

    Font font = loadFont();

    int etatAct = 0;

    Texture backgroundImage = loadBackgroundImage();
    Sprite backgroundSprite = createBackgroundSprite(backgroundImage);

    auto updateBackground = makeUpdateBackground(app, backgroundImage, backgroundSprite);

    updateScaleAndPosition(app, backgroundSprite, backgroundImage);
    FloatRect bounds = backgroundSprite.getGlobalBounds();

    vector<Ville> villes = createVilles(font);

    initMarkersTexts(villes, backgroundSprite, bounds);

    runApp(app, backgroundSprite, villes, updateBackground, etatAct);

    return 0;
}