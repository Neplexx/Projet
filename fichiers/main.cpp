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
#include <fstream>
#include <chrono>

using namespace std;
using namespace sf;

#ifdef _MSC_VER
#pragma comment(linker, "/SUBSYSTEM:windows /ENTRY:mainCRTStartup")
#endif

#ifndef PATH_IMG
#define PATH_IMG "assets/"
#endif

const std::string path_image = PATH_IMG;

struct SimClock {
    int hours = 0;
    int minutes = 0;

    void advance(int mins) {
        minutes += mins;
        if (minutes >= 60) {
            hours = (hours + 1) % 24;
            minutes = 0;
        }
    }

    string toString() const {
        ostringstream oss;
        oss << setfill('0') << setw(2) << hours << "h" << setw(2) << minutes;
        return oss.str();
    }

    string toPlanningKey() const {
        ostringstream oss;
        oss << hours << "h" << setfill('0') << setw(2) << minutes;
        return oss.str();
    }
};

struct AvionVisuel {
    string nom;
    int id;
    optional<Sprite> sprite;
    bool selectionne;
    Color couleurBase;

    Coord position;
    Coord positionPrecedente;
    float tempsDepuisMaj;
    int altitude;
    int vitesse;
    int carburant;
    string villeDepart;
    string villeDestination;
    float distanceParcourue;
    float tempsVol;

    vector<Vector2f> trajectoirePredite;
    bool enEvitement;

    AvionVisuel() : nom(""), id(0), selectionne(false), couleurBase(Color::White), position(Coord(0, 0)), positionPrecedente(Coord(0, 0)), tempsDepuisMaj(0.f), altitude(0), vitesse(0), carburant(0), villeDepart(""), villeDestination(""), distanceParcourue(0.f), tempsVol(0.f), enEvitement(false) {}
};

#include "structure/AeroportWindow.hpp"

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
        img.resize(Vector2u(32, 32), Color::Blue);
        for (unsigned int y = 0; y < 32; ++y) {
            for (unsigned int x = 0; x < 32; ++x) {
                if ((x >= 14 && x <= 18 && y >= 8 && y <= 24) || (x >= 10 && x <= 22 && y >= 16 && y <= 18) || (x >= 12 && x <= 20 && y >= 8 && y <= 12)) {
                    img.setPixel(Vector2u(x, y), Color::White);
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
    villes.push_back({ "Lille", Vector2f(0.54f, 0.05f), Coord(0, 0), CircleShape(8.f), Text(font) });
    villes.push_back({ "Paris", Vector2f(0.5f, 0.25f), Coord(0, 0), CircleShape(8.f), Text(font) });
    villes.push_back({ "Strasbourg", Vector2f(0.84f, 0.27f), Coord(0, 0), CircleShape(8.f), Text(font) });
    villes.push_back({ "Rennes", Vector2f(0.23f, 0.34f), Coord(0, 0), CircleShape(8.f), Text(font) });
    villes.push_back({ "Nice", Vector2f(0.83f, 0.83f), Coord(0, 0), CircleShape(8.f), Text(font) });
    villes.push_back({ "Toulouse", Vector2f(0.40f, 0.85f), Coord(0, 0), CircleShape(8.f), Text(font) });
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

Coord trouverPositionVille(const string& nomVille, const vector<Ville>& villes) {
    for (const auto& ville : villes) {
        if (ville.nom == nomVille) return ville.position;
    }
    return Coord(0, 0);
}

string determinerVilleFromCoord(const Coord& coord, const vector<Ville>& villes) {
    const int TOLERANCE = 50;
    for (const auto& ville : villes) {
        if (abs(coord.get_x() - ville.position.get_x()) <= TOLERANCE && abs(coord.get_y() - ville.position.get_y()) <= TOLERANCE) {
            return ville.nom;
        }
    }
    return "Inconnu";
}

void dessinerTrajectoire(RenderWindow& app, const AvionVisuel& avion) {
    if (avion.trajectoirePredite.size() < 2) return;
    VertexArray ligne(PrimitiveType::LineStrip, avion.trajectoirePredite.size());
    for (size_t i = 0; i < avion.trajectoirePredite.size(); ++i) {
        ligne[i].position = avion.trajectoirePredite[i];
        float alpha = 255.f * (1.0f - static_cast<float>(i) / avion.trajectoirePredite.size());
        ligne[i].color = avion.enEvitement ? Color(255, 200, 0, static_cast<uint8_t>(alpha)) : Color(100, 200, 255, static_cast<uint8_t>(alpha));
    }
    app.draw(ligne);
}

void afficherInfosAvion(RenderWindow& app, const AvionVisuel& avion, const Font& font) {
    ostringstream oss;
    oss << fixed << setprecision(1);
    oss << "=== " << avion.nom << " ===\n";
    oss << "Depart: " << avion.villeDepart << "\n";
    oss << "Dest: " << avion.villeDestination << "\n";
    oss << "Vitesse: " << avion.vitesse << " km/h\n";
    oss << "Altitude: " << avion.altitude << " ft\n";
    oss << "Carburant: " << avion.carburant << " L";

    Text infoText(font);
    infoText.setString(oss.str());
    infoText.setCharacterSize(16);
    infoText.setFillColor(Color::White);

    RectangleShape fond(Vector2f(200.f, 130.f));
    fond.setFillColor(Color(0, 0, 0, 200));
    fond.setPosition(Vector2f(20.f, 80.f));
    infoText.setPosition(Vector2f(35.f, 95.f));

    app.draw(fond);
    app.draw(infoText);
}

void calculerTrajectoireSimple(AvionVisuel& av, const Coord& destination) {
    av.trajectoirePredite.clear();
    Vector2f pos(static_cast<float>(av.position.get_x()), static_cast<float>(av.position.get_y()));
    Vector2f dest(static_cast<float>(destination.get_x()), static_cast<float>(destination.get_y()));
    for (int i = 0; i <= 20; i+=1) {
        av.trajectoirePredite.push_back(pos + (dest - pos) * (i / 20.f));
    }
}


int main() {
    VideoMode desktop = VideoMode::getDesktopMode();
    RenderWindow app(desktop, "Simulation ATC", State::Fullscreen);
    app.setFramerateLimit(60);

    Texture backgroundImage = loadBackgroundImage(path_image + "France.jpg");
    Texture avionTexture = loadAvionTexture(path_image + "avion.png");
    Font font = loadFont(path_image + "arial.ttf");

    Sprite backgroundSprite(backgroundImage);
    float scaleX = static_cast<float>(app.getSize().x) / backgroundImage.getSize().x;
    float scaleY = static_cast<float>(app.getSize().y) / backgroundImage.getSize().y;
    float scale = min(scaleX, scaleY);

    backgroundSprite.setScale(Vector2f(scale, scale));
    backgroundSprite.setPosition(Vector2f((app.getSize().x - backgroundSprite.getGlobalBounds().size.x) / 2.f, (app.getSize().y - backgroundSprite.getGlobalBounds().size.y) / 2.f));

    vector<Ville> villes = createVilles(font);
    initMarkersTexts(villes, backgroundSprite);

    SimulationManager simulation;
    simulation.addCCR();
    for (const auto& ville : villes) {
        simulation.addAPP(ville.nom, ville.position);
        simulation.addTWR(5, ville.nom);
    }
    AeroportWindowManager aeroportWindows(font, path_image + "aeroport.png", simulation.shared_data);
    for (const auto& ville : villes) {
        aeroportWindows.addAeroport(ville.nom,ville.position.get_x(),ville.position.get_y());
    }

    CCR ccr_logic;
    ccr_logic.planning();

    std::ifstream file("planning.json");
    nlohmann::json planningData;
    if (!(file >> planningData)) {
        cerr << "Erreur critique: Impossible de lire planning.json" << endl;
        return -1;
    }

    SimClock simClock;
    float timeAccumulator = 0.f;

    Text clockText(font);
    clockText.setString("Heure: 00h00");
    clockText.setCharacterSize(35);
    clockText.setFillColor(Color::Cyan);
    clockText.setOutlineThickness(2.f);
    clockText.setPosition(Vector2f(20.f, 20.f));

    simulation.startSimulation();

    map<string, AvionVisuel> avionsVisuels;
    string avionSelectionneCode = "";
    Clock deltaClock;

    while (app.isOpen()) {
        float dt = deltaClock.restart().asSeconds();
        timeAccumulator += dt;

        if (timeAccumulator >= 2.0f) {
            simClock.advance(15);
            timeAccumulator = 0.f;

            string currentKey = simClock.toPlanningKey();
            for (const auto& flight : planningData["flights"]) {
                if (flight["time"] == currentKey) {
                    string code = flight["code"];
                    string departure = flight["departure"];
                    string arrival = flight["arrival"];
                    if (departure == arrival) {
                        cout << "[PLANNING] Vol " << code << " ignoré : " << departure << " → " << arrival << " (même ville)" << endl;
                        continue;
                    }
                    Coord posDep = trouverPositionVille(departure, villes);
                    Coord posArr = trouverPositionVille(arrival, villes);

                    string codeDisponible = "";
                    string aeroportSource = "";
                    Coord positionReelle;
                    {
                        lock_guard<mutex> lock(simulation.shared_data->parkingsMutex);
                        auto now = chrono::steady_clock::now();

                        for (auto& aeroportPair : simulation.shared_data->avionsAuParking) {
                            auto& avionsAuParking = aeroportPair.second;

                            for (auto it = avionsAuParking.begin(); it != avionsAuParking.end(); ++it) {
                                auto tempsParking = chrono::duration_cast<chrono::seconds>(now - it->heureArrivee).count();
                                if (tempsParking >= it->tempsParkingSecondes) {
                                    codeDisponible = it->code;
                                    aeroportSource = it->aeroport;
                                    int placeLibre = it->numeroPlace;
                                    positionReelle = trouverPositionVille(aeroportSource, villes);

                                    cout << "\n[RÉUTILISATION] Code avion " << codeDisponible << " libéré du parking P" << (placeLibre + 1) << " de " << aeroportSource << endl;

                                    simulation.shared_data->parkingsLibres[aeroportSource][placeLibre] = true;

                                    avionsAuParking.erase(it);
                                    goto code_trouve;
                                }
                            }
                        }
                    code_trouve:;
                    }
                    string codeAvion = codeDisponible.empty() ? code : codeDisponible;
                    Coord positionDepart = codeDisponible.empty() ? posDep : positionReelle;

                    if (!codeDisponible.empty() && aeroportSource == arrival) {
                        cout << "[PLANNING] Vol " << code << " ignoré : avion " << codeDisponible << " déjà à destination (" << arrival << ")" << endl;
                        continue;
                    }

                    if (!codeDisponible.empty()) {
                        cout << "[PLANNING] Vol " << code << " : Réutilisation du code " << codeDisponible << " (" << aeroportSource << " → " << arrival << ")" << endl;

                        for (auto it = simulation.avions.begin(); it != simulation.avions.end(); ++it) {
                            if ((*it)->get_code() == codeDisponible) {
                                (*it)->stop();
                                (*it)->join();
                                simulation.avions.erase(it);
                                break;
                            }
                        }
                        {
                            lock_guard<mutex> lock(simulation.shared_data->avionsAtterrisMutex);
                            simulation.shared_data->avionsAtterris.erase(codeDisponible);
                        }
                    }
                    else {
                        cout << "[PLANNING] Vol " << code << " : Création d'un nouvel avion (" << departure << " → " << arrival << ")" << endl;
                    }
                    auto avion = make_unique<ThreadedAvion>(simulation.shared_data);
                    avion->set_code(codeAvion);
                    avion->set_altitude(0);
                    avion->set_vitesse(0);
                    avion->set_position(positionDepart);
                    avion->set_destination(posArr);
                    avion->set_carburant(1000);
                    avion->start();
                    simulation.avions.push_back(move(avion));
                }
            }
        }
        clockText.setString("Heure Simulation : " + simClock.toString());

        while (const optional<Event> event = app.pollEvent()) {
            if (event->is<Event::Closed>()) app.close();
            if (event->is<Event::KeyPressed>() && event->getIf<Event::KeyPressed>()->code == Keyboard::Key::Escape) app.close();

            if (event->is<Event::MouseButtonPressed>() && event->getIf<Event::MouseButtonPressed>()->button == Mouse::Button::Left) {
                Vector2f clickPos(static_cast<float>(event->getIf<Event::MouseButtonPressed>()->position.x), static_cast<float>(event->getIf<Event::MouseButtonPressed>()->position.y));
                bool avionClique = false;
                for (auto& pair : avionsVisuels) {
                    if (pair.second.sprite && pair.second.sprite->getGlobalBounds().contains(clickPos)) {
                        if (!avionSelectionneCode.empty()) avionsVisuels[avionSelectionneCode].selectionne = false;
                        pair.second.selectionne = true;
                        avionSelectionneCode = pair.first;
                        avionClique = true;
                        break;
                    }
                }
                if (!avionClique) {
                    for (const auto& ville : villes) {
                        if (ville.marqueur.getGlobalBounds().contains(clickPos)) {
                            cout << "Ouverture fenetre TWR pour " << ville.nom << endl;
                            aeroportWindows.toggleWindow(ville.nom);
                            avionClique = true;
                            break;
                        }
                    }
                }
                if (!avionClique) {
                    if (!avionSelectionneCode.empty()) avionsVisuels[avionSelectionneCode].selectionne = false;
                    avionSelectionneCode = "";
                }
            }
        }
        {
            lock_guard<mutex> lock(simulation.shared_data->avions_positionsMutex);

            vector<string> aSupprimer;
            for (auto& pair : avionsVisuels) {
                if (simulation.shared_data->avions_positions.find(pair.first) == simulation.shared_data->avions_positions.end()) aSupprimer.push_back(pair.first);
            }
            for (const auto& code : aSupprimer) {
                avionsVisuels.erase(code);
                if (avionSelectionneCode == code) avionSelectionneCode = "";
            }

            for (const auto& entry : simulation.shared_data->avions_positions) {
                const string& code = entry.first;
                if (avionsVisuels.find(code) == avionsVisuels.end()) {
                    AvionVisuel av;
                    av.nom = code;
                    av.sprite = Sprite(avionTexture);
                    av.sprite->setOrigin(Vector2f(av.sprite->getLocalBounds().size.x / 2.f, av.sprite->getLocalBounds().size.y / 2.f));
                    av.sprite->setScale(Vector2f(0.05f, 0.05f));
                    avionsVisuels[code] = av;
                }
                avionsVisuels[code].positionPrecedente = avionsVisuels[code].position;
                avionsVisuels[code].position = entry.second;
                avionsVisuels[code].tempsDepuisMaj = 0.f;
            }
        }
        for (auto& avion_ptr : simulation.avions) {
            string code = avion_ptr->get_code();
            if (avionsVisuels.count(code)) {
                auto& av = avionsVisuels[code];
                av.vitesse = avion_ptr->get_vitesse();
                av.altitude = avion_ptr->get_altitude();
                av.carburant = avion_ptr->get_carburant();
                av.villeDepart = determinerVilleFromCoord(avion_ptr->get_position(), villes);
                av.villeDestination = determinerVilleFromCoord(avion_ptr->get_destination(), villes);
                calculerTrajectoireSimple(av, avion_ptr->get_destination());

                int dx = avion_ptr->get_destination().get_x() - av.position.get_x();
                int dy = avion_ptr->get_destination().get_y() - av.position.get_y();
                if (dx != 0 || dy != 0) av.sprite->setRotation(degrees(atan2(static_cast<float>(dy), static_cast<float>(dx)) * 180.f / 3.14159f + 90.f));
            }
        }
        aeroportWindows.updateAll(avionsVisuels);

        app.clear();
        app.draw(backgroundSprite);

        for (const auto& ville : villes) {
            app.draw(ville.marqueur);
            app.draw(ville.texte);
        }

        for (auto& pair : avionsVisuels) {
            auto& av = pair.second;
            av.tempsDepuisMaj += dt;
            float ratio = min(av.tempsDepuisMaj / 0.1f, 1.0f);
            float x = static_cast<float>(av.positionPrecedente.get_x()) * (1.0f - ratio) + static_cast<float>(av.position.get_x()) * ratio;
            float y = static_cast<float>(av.positionPrecedente.get_y()) * (1.0f - ratio) + static_cast<float>(av.position.get_y()) * ratio;

            if (av.sprite) {
                av.sprite->setPosition(Vector2f(x, y));
                av.sprite->setColor(av.selectionne ? Color::Green : (av.enEvitement ? Color::Yellow : Color::White));
                if (av.selectionne) dessinerTrajectoire(app, av);
                app.draw(*av.sprite);
            }
        }

        app.draw(clockText);
        if (!avionSelectionneCode.empty() && avionsVisuels.count(avionSelectionneCode)) afficherInfosAvion(app, avionsVisuels[avionSelectionneCode], font);

        app.display();
    }
    aeroportWindows.closeAll();
    simulation.stopSimulation();
    return 0;
}