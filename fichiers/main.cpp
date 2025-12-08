#include <stdlib.h>
#include <iostream>
#include <vector>
#include <memory>
#include <SFML/Graphics.hpp>
#include "structure/Thread.hpp"
#include <sstream>
#include <iomanip>
#include <map>
#include <mutex>
#include <math.h>
#include <optional>
#include <algorithm>
#include <random>

using namespace std;
using namespace sf;

#ifdef _MSC_VER
#pragma comment(linker, "/SUBSYSTEM:windows /ENTRY:mainCRTStartup")
#define PATH_IMG_ "assets/"
#else
#define PATH_IMG "assets/"
#endif

const std::string path_image(PATH_IMG);
const float PX_TO_KMH = 3.0f;

struct AvionVisuel {
    string nom;
    int id;
    optional<Sprite> sprite;
    bool selectionne;
    Color couleurBase;

    Coord position;
    Coord positionPrecedente;  // AJOUTÉ
    float tempsDepuisMaj;      // AJOUTÉ
    int altitude;
    int vitesse;
    int carburant;
    string villeDepart;
    string villeDestination;
    float distanceParcourue;
    float tempsVol;

    vector<Vector2f> trajectoirePredite;
    bool enEvitement;

    AvionVisuel() : nom(""), id(0), selectionne(false), couleurBase(Color::White),
        position(Coord(0, 0)), positionPrecedente(Coord(0, 0)), tempsDepuisMaj(0.f),  // MODIFIÉ
        altitude(0), vitesse(0), carburant(0),
        villeDepart(""), villeDestination(""), distanceParcourue(0.f), tempsVol(0.f),
        enEvitement(false) {
    }
};

struct Ville {
    string nom;
    Vector2f positionRelative;
    Coord position;
    CircleShape marqueur;
    Text texte;
};

Texture loadBackgroundImage(const string& path) {
    Texture backgroundImage;
    if (!backgroundImage.loadFromFile(path)) {
        cerr << "Erreur chargement image: " << path << endl;
        exit(-1);
    }
    return backgroundImage;
}

Texture loadAvionTexture(const string& path) {
    Texture avionTexture;
    if (!avionTexture.loadFromFile(path)) {
        Image img;
        img.resize({ 32, 32 }, Color::Blue);

        for (unsigned int y = 0; y < 32; ++y) {
            for (unsigned int x = 0; x < 32; ++x) {
                if ((x >= 14 && x <= 18 && y >= 8 && y <= 24) ||
                    (x >= 10 && x <= 22 && y >= 16 && y <= 18) ||
                    (x >= 12 && x <= 20 && y >= 8 && y <= 12)) {
                    img.setPixel({ x, y }, Color::White);
                }
            }
        }
        avionTexture.loadFromImage(img);
    }
    return avionTexture;
}

Font loadFont(const string& path) {
    Font font;
    if (!font.openFromFile(path)) {
        cerr << "Erreur chargement font: " << path << endl;
        exit(-1);
    }
    return font;
}

vector<Ville> createVilles(const Font& font) {
    vector<Ville> villes;

    villes.push_back({ "Lille", {0.54f, 0.05f}, Coord(0, 0), CircleShape(8.f), Text(font) });
    villes.push_back({ "Paris", {0.5f, 0.25f}, Coord(0, 0), CircleShape(8.f), Text(font) });
    villes.push_back({ "Strasbourg", {0.84f, 0.27f}, Coord(0, 0), CircleShape(8.f), Text(font) });
    villes.push_back({ "Rennes", {0.23f, 0.34f}, Coord(0, 0), CircleShape(8.f), Text(font) });
    villes.push_back({ "Nice", {0.83f, 0.83f}, Coord(0, 0), CircleShape(8.f), Text(font) });
    villes.push_back({ "Toulouse", {0.40f, 0.85f}, Coord(0, 0), CircleShape(8.f), Text(font) });

    return villes;
}

void initMarkersTexts(vector<Ville>& villes, const Sprite& backgroundSprite) {
    FloatRect bounds = backgroundSprite.getGlobalBounds();

    for (auto& ville : villes) {
        ville.marqueur.setFillColor(Color::Red);
        ville.marqueur.setOutlineThickness(2.f);
        ville.marqueur.setOutlineColor(Color::Black);

        float X = bounds.position.x + ville.positionRelative.x * bounds.size.x;
        float Y = bounds.position.y + ville.positionRelative.y * bounds.size.y;

        ville.position = Coord(static_cast<int>(X), static_cast<int>(Y));

        ville.marqueur.setOrigin(Vector2f(ville.marqueur.getRadius(), ville.marqueur.getRadius()));
        ville.marqueur.setPosition(Vector2f(X, Y));

        ville.texte.setString(ville.nom);
        ville.texte.setCharacterSize(20);
        ville.texte.setFillColor(Color::White);
        ville.texte.setOutlineThickness(1.f);
        ville.texte.setOutlineColor(Color::Black);
        ville.texte.setPosition(Vector2f(X + 15.f, Y - 10.f));
    }
}

float distance(Vector2f a, Vector2f b) {
    float dx = b.x - a.x;
    float dy = b.y - a.y;
    return std::sqrt(dx * dx + dy * dy);
}

float magnitude(Vector2f v) {
    return std::sqrt(v.x * v.x + v.y * v.y);
}

void dessinerTrajectoire(RenderWindow& app, const AvionVisuel& avion) {
    if (avion.trajectoirePredite.size() < 2) return;

    VertexArray ligne(PrimitiveType::LineStrip, avion.trajectoirePredite.size());

    for (size_t i = 0; i < avion.trajectoirePredite.size(); ++i) {
        ligne[i].position = avion.trajectoirePredite[i];
        float alpha = 255.f * (1.0f - static_cast<float>(i) / avion.trajectoirePredite.size());

        if (avion.enEvitement) {
            ligne[i].color = Color(255, 200, 0, static_cast<uint8_t>(alpha));
        }
        else {
            ligne[i].color = Color(100, 200, 255, static_cast<uint8_t>(alpha));
        }
    }

    app.draw(ligne);

    for (size_t i = 0; i < avion.trajectoirePredite.size(); i += 5) {
        CircleShape point(2.f);
        if (avion.enEvitement) {
            point.setFillColor(Color(255, 200, 0, 150));
        }
        else {
            point.setFillColor(Color(100, 200, 255, 150));
        }
        point.setOrigin({ 2.f, 2.f });
        point.setPosition(avion.trajectoirePredite[i]);
        app.draw(point);
    }
}

void afficherInfosAvion(RenderWindow& app, const AvionVisuel& avion, const Font& font) {
    ostringstream oss;
    oss << fixed << setprecision(1);
    oss << "=== " << avion.nom << " ===\n";
    oss << "Position: (" << avion.position.get_x() << ", " << avion.position.get_y() << ")\n";
    oss << "Depart: " << avion.villeDepart << "\n";
    oss << "Destination: " << avion.villeDestination << "\n";
    oss << "Vitesse: " << avion.vitesse << " km/h\n";
    oss << "Altitude: " << avion.altitude << " ft\n";
    oss << "Carburant: " << avion.carburant << " L\n";
    oss << "Temps vol: " << static_cast<int>(avion.tempsVol / 60) << " min";

    Text infoText(font);
    infoText.setString(oss.str());
    infoText.setCharacterSize(16);
    infoText.setFillColor(Color::White);
    infoText.setOutlineThickness(1.f);
    infoText.setOutlineColor(Color::Black);

    FloatRect textBounds = infoText.getLocalBounds();
    RectangleShape fond(Vector2f(textBounds.size.x + 30.f, textBounds.size.y + 30.f));
    fond.setFillColor(Color(0, 0, 0, 200));
    fond.setOutlineThickness(3.f);
    fond.setOutlineColor(Color(0, 200, 0, 255));

    Vector2f posInfo(20.f, 20.f);
    fond.setPosition(posInfo);
    infoText.setPosition(Vector2f(posInfo.x + 15.f, posInfo.y + 15.f));

    app.draw(fond);
    app.draw(infoText);
}

string determinerVilleFromCoord(const Coord& coord, const vector<Ville>& villes) {
    const int TOLERANCE = 50;

    for (const auto& ville : villes) {
        if (abs(coord.get_x() - ville.position.get_x()) <= TOLERANCE &&
            abs(coord.get_y() - ville.position.get_y()) <= TOLERANCE) {
            return ville.nom;
        }
    }

    return "Inconnu";
}

void calculerTrajectoireSimple(AvionVisuel& av, const Coord& destination, float horizonTemps = 10.f) {
    av.trajectoirePredite.clear();

    Vector2f pos(av.position.get_x(), av.position.get_y());
    Vector2f dest(destination.get_x(), destination.get_y());

    int numPoints = 30;
    for (int i = 0; i <= numPoints; ++i) {
        float t = static_cast<float>(i) / numPoints;
        Vector2f point = pos + (dest - pos) * t;
        av.trajectoirePredite.push_back(point);
    }
}

Coord trouverPositionVille(const string& nomVille, const vector<Ville>& villes) {
    for (const auto& ville : villes) {
        if (ville.nom == nomVille) {
            return ville.position;
        }
    }
    return Coord(0, 0);
}

int main() {
    // Initialisation SFML
    VideoMode desktop = VideoMode::getDesktopMode();
    RenderWindow app(desktop, "Simulation ATC", State::Fullscreen);
    app.setFramerateLimit(60);

    // Chargement ressources
    Texture backgroundImage = loadBackgroundImage(path_image + "France.jpg");
    Texture avionTexture = loadAvionTexture(path_image + "avion.png");
    Font font = loadFont(path_image + "arial.ttf");

    // Sprite de fond centré
    Sprite backgroundSprite(backgroundImage);
    Vector2u windowSize = app.getSize();
    Vector2u textureSize = backgroundImage.getSize();
    float scaleX = static_cast<float>(windowSize.x) / textureSize.x;
    float scaleY = static_cast<float>(windowSize.y) / textureSize.y;
    float scale = min(scaleX, scaleY);
    backgroundSprite.setScale(Vector2f(scale, scale));

    FloatRect spriteBounds = backgroundSprite.getGlobalBounds();
    backgroundSprite.setPosition(Vector2f(
        (static_cast<float>(windowSize.x) - spriteBounds.size.x) / 2.f,
        (static_cast<float>(windowSize.y) - spriteBounds.size.y) / 2.f
    ));

    // Créer les villes
    vector<Ville> villes = createVilles(font);
    initMarkersTexts(villes, backgroundSprite);

    cout << "\n=== POSITIONS DES VILLES ===" << endl;
    for (const auto& ville : villes) {
        cout << ville.nom << ": (" << ville.position.get_x() << ", " << ville.position.get_y() << ")" << endl;
    }
    cout << "============================\n" << endl;

    // AJOUT: Compteur FPS
    Clock fpsClock;
    int frameCount = 0;
    Text fpsText(font);
    fpsText.setCharacterSize(24);
    fpsText.setFillColor(Color::Yellow);
    fpsText.setOutlineThickness(2.f);
    fpsText.setOutlineColor(Color::Black);
    fpsText.setPosition(Vector2f(windowSize.x - 150.f, 20.f));

    // Initialisation système de threads
    SimulationManager simulation;
    simulation.addCCR();

    for (const auto& ville : villes) {
        simulation.addAPP(ville.nom);
        simulation.addTWR(5, ville.nom);
    }

    random_device rd;
    mt19937 rng(rd());
    uniform_int_distribution<int> distVille(0, villes.size() - 1);

    Clock spawnClock;
    const float tempsEntreAvions = 3.f;
    int compteurAvions = 0;

    // Créer quelques avions initiaux
    for (int i = 0; i < 3; ++i) {
        int villeDepart = distVille(rng);
        int villeDest = distVille(rng);

        while (villeDest == villeDepart) {
            villeDest = distVille(rng);
        }

        compteurAvions++;
        string codeAvion = "AF" + to_string(1000 + compteurAvions);

        Coord posDepart = villes[villeDepart].position;
        Coord posDest = villes[villeDest].position;

        auto avion = make_unique<ThreadedAvion>(simulation.shared_data);
        avion->set_code(codeAvion);
        avion->set_altitude(0);
        avion->set_vitesse(0);
        avion->set_position(posDepart);
        avion->set_destination(posDest);
        avion->set_place_parking(0);
        avion->set_carburant(1000);

        simulation.avions.push_back(move(avion));

        cout << "Avion initial créé: " << codeAvion
            << " de " << villes[villeDepart].nom << " " << posDepart
            << " vers " << villes[villeDest].nom << " " << posDest << endl;
    }

    simulation.startSimulation();

    map<string, AvionVisuel> avionsVisuels;
    string avionSelectionneCode = "";

    Clock clock;

    while (app.isOpen()) {
        float deltaTime = clock.restart().asSeconds();

        Vector2i mousePixelPos = Mouse::getPosition(app);
        Vector2f mousePos(static_cast<float>(mousePixelPos.x), static_cast<float>(mousePixelPos.y));

        // Génération automatique d'avions
        if (spawnClock.getElapsedTime().asSeconds() > tempsEntreAvions) {
            int villeDepart = distVille(rng);
            int villeDest = distVille(rng);

            while (villeDest == villeDepart) {
                villeDest = distVille(rng);
            }

            compteurAvions++;
            string codeAvion = "AF" + to_string(1000 + compteurAvions);

            Coord posDepart = villes[villeDepart].position;
            Coord posDest = villes[villeDest].position;

            auto avion = make_unique<ThreadedAvion>(simulation.shared_data);
            avion->set_code(codeAvion);
            avion->set_altitude(0);
            avion->set_vitesse(0);
            avion->set_position(posDepart);
            avion->set_destination(posDest);
            avion->set_place_parking(0);
            avion->set_carburant(1000);

            avion->start();
            simulation.avions.push_back(move(avion));

            cout << "Nouvel avion créé: " << codeAvion
                << " de " << villes[villeDepart].nom
                << " vers " << villes[villeDest].nom << endl;

            spawnClock.restart();
        }

        // Gestion des événements
        while (const optional<Event> event = app.pollEvent()) {
            if (event->is<Event::Closed>())
                app.close();

            if (event->is<Event::KeyPressed>()) {
                const auto& keyEvent = event->getIf<Event::KeyPressed>();
                if (keyEvent->code == Keyboard::Key::Escape) {
                    simulation.stopSimulation();
                    app.close();
                }
            }

            if (event->is<Event::MouseButtonPressed>()) {
                const auto& mouseEvent = event->getIf<Event::MouseButtonPressed>();
                if (mouseEvent->button == Mouse::Button::Left) {
                    Vector2f clickPos(static_cast<float>(mouseEvent->position.x),
                        static_cast<float>(mouseEvent->position.y));

                    bool avionClique = false;
                    for (auto& pair : avionsVisuels) {
                        if (pair.second.sprite.has_value() &&
                            pair.second.sprite->getGlobalBounds().contains(clickPos)) {
                            if (!avionSelectionneCode.empty()) {
                                avionsVisuels[avionSelectionneCode].selectionne = false;
                            }
                            pair.second.selectionne = true;
                            avionSelectionneCode = pair.first;
                            avionClique = true;
                            break;
                        }
                    }

                    if (!avionClique && !avionSelectionneCode.empty()) {
                        avionsVisuels[avionSelectionneCode].selectionne = false;
                        avionSelectionneCode = "";
                    }
                }
            }
        }

        // Récupérer les positions des avions depuis les threads
        {
            lock_guard<mutex> lock(simulation.shared_data->avions_positionsMutex);

            vector<string> aSupprimer;
            for (auto& pair : avionsVisuels) {
                if (simulation.shared_data->avions_positions.find(pair.first) ==
                    simulation.shared_data->avions_positions.end()) {
                    aSupprimer.push_back(pair.first);
                }
            }
            for (const auto& code : aSupprimer) {
                avionsVisuels.erase(code);
                if (avionSelectionneCode == code) {
                    avionSelectionneCode = "";
                }
            }

            for (const auto& entry : simulation.shared_data->avions_positions) {
                const string& code = entry.first;
                const Coord& pos = entry.second;

                if (avionsVisuels.find(code) == avionsVisuels.end()) {
                    AvionVisuel av;
                    av.nom = code;
                    av.id = avionsVisuels.size() + 1;
                    av.sprite = Sprite(avionTexture);

                    FloatRect bounds = av.sprite->getLocalBounds();
                    av.sprite->setOrigin(Vector2f(bounds.size.x / 2.f, bounds.size.y / 2.f));
                    av.sprite->setScale(Vector2f(0.05f, 0.05f));
                    av.selectionne = false;
                    av.couleurBase = Color::White;
                    av.sprite->setColor(Color::White);
                    av.tempsVol = 0.f;
                    av.distanceParcourue = 0.f;

                    avionsVisuels[code] = av;
                }

                // MODIFIÉ: Interpolation des positions
                avionsVisuels[code].positionPrecedente = avionsVisuels[code].position;
                avionsVisuels[code].position = pos;
                avionsVisuels[code].tempsDepuisMaj = 0.f;

                avionsVisuels[code].tempsVol += deltaTime;
            }
        }

        // Incrémenter le temps d'interpolation pour tous les avions
        for (auto& pair : avionsVisuels) {
            pair.second.tempsDepuisMaj += deltaTime;
        }

        // Récupérer infos détaillées
        for (auto& avion_ptr : simulation.avions) {
            string code = avion_ptr->get_code();
            if (avionsVisuels.find(code) != avionsVisuels.end()) {
                avionsVisuels[code].vitesse = avion_ptr->get_vitesse();
                avionsVisuels[code].carburant = avion_ptr->get_carburant();
                avionsVisuels[code].altitude = avion_ptr->get_altitude();
                avionsVisuels[code].villeDepart = determinerVilleFromCoord(avion_ptr->get_position(), villes);
                avionsVisuels[code].villeDestination = determinerVilleFromCoord(avion_ptr->get_destination(), villes);

                calculerTrajectoireSimple(avionsVisuels[code], avion_ptr->get_destination());
            }
        }

        // Détecter proximités (optimisé)
        for (auto& pair1 : avionsVisuels) {
            pair1.second.enEvitement = false;
            Vector2f pos1(pair1.second.position.get_x(), pair1.second.position.get_y());

            for (auto& pair2 : avionsVisuels) {
                if (pair1.first >= pair2.first) continue;

                Vector2f pos2(pair2.second.position.get_x(), pair2.second.position.get_y());
                float dist = distance(pos1, pos2);

                if (dist < 150.f) {
                    pair1.second.enEvitement = true;
                    pair2.second.enEvitement = true;
                }
            }
        }

        // Rotation et couleurs
        for (auto& pair : avionsVisuels) {
            auto& av = pair.second;
            if (!av.sprite.has_value()) continue;

            for (auto& avion_ptr : simulation.avions) {
                if (avion_ptr->get_code() == pair.first) {
                    Coord dest = avion_ptr->get_destination();
                    int dx = dest.get_x() - av.position.get_x();
                    int dy = dest.get_y() - av.position.get_y();

                    if (dx != 0 || dy != 0) {
                        float angle = atan2(dy, dx) * 180.f / 3.14159265f;
                        av.sprite->setRotation(degrees(angle + 90.f));
                    }
                    break;
                }
            }

            if (av.selectionne) {
                av.sprite->setColor(Color::Green);
            }
            else if (av.enEvitement) {
                av.sprite->setColor(Color::Yellow);
            }
            else {
                av.sprite->setColor(Color::White);
            }
        }

        // Dessin
        app.clear();
        app.draw(backgroundSprite);

        for (const auto& pair : avionsVisuels) {
            if (pair.second.selectionne) {
                dessinerTrajectoire(app, pair.second);
            }
        }

        for (const auto& ville : villes) {
            app.draw(ville.marqueur);
            app.draw(ville.texte);
        }

        // MODIFIÉ: Dessin avec interpolation
        for (const auto& pair : avionsVisuels) {
            if (pair.second.sprite.has_value()) {
                const auto& av = pair.second;

                // Interpolation de position
                float ratio = min(av.tempsDepuisMaj / 0.1f, 1.0f);
                float x = av.positionPrecedente.get_x() * (1.0f - ratio) +
                    av.position.get_x() * ratio;
                float y = av.positionPrecedente.get_y() * (1.0f - ratio) +
                    av.position.get_y() * ratio;

                Sprite spriteTemp = *av.sprite;
                spriteTemp.setPosition(Vector2f(x, y));
                app.draw(spriteTemp);
            }
        }

        if (!avionSelectionneCode.empty() &&
            avionsVisuels.find(avionSelectionneCode) != avionsVisuels.end()) {
            afficherInfosAvion(app, avionsVisuels[avionSelectionneCode], font);
        }

        app.display();
    }

    simulation.stopSimulation();
    return 0;
}
