#ifndef AEROPORT_WINDOW_HPP
#define AEROPORT_WINDOW_HPP

#include <SFML/Graphics.hpp>
#include <memory>
#include <map>
#include <string>
#include <sstream>
#include <iomanip>
#include <cmath>
#include <iostream>
#include <optional>
#include <vector>
#include <chrono>
#include "Thread.hpp"

using namespace sf;

struct AvionVisuel;

class AeroportWindow {
private:
    std::unique_ptr<RenderWindow> window;
    std::string airportName;
    int airportX;
    int airportY;
    Font& font;
    bool isOpen;
    Texture backgroundTexture;
    std::unique_ptr<Sprite> backgroundSprite;
    bool backgroundLoaded;

    std::shared_ptr<DataHub> shared_data;

public:
    AeroportWindow(const std::string& name, int posX, int posY, Font& f,
        const std::string& imagePath, std::shared_ptr<DataHub> data)
        : airportName(name), airportX(posX), airportY(posY), font(f), isOpen(false),
        backgroundLoaded(false), shared_data(data) {

        if (backgroundTexture.loadFromFile(imagePath)) {
            backgroundSprite = std::make_unique<Sprite>(backgroundTexture);
            backgroundLoaded = true;
        }
        else {
            std::cerr << "Erreur: Impossible de charger " << imagePath << std::endl;
        }
    }

    void open() {
        if (!isOpen) {
            window = std::make_unique<RenderWindow>(
                VideoMode({ 1000, 700 }),
                "TWR - " + airportName
            );
            window->setFramerateLimit(60);

            if (backgroundLoaded && backgroundSprite) {
                float scaleX = 1000.f / backgroundTexture.getSize().x;
                float scaleY = 700.f / backgroundTexture.getSize().y;
                float scale = std::min(scaleX, scaleY);
                backgroundSprite->setScale(Vector2f(scale, scale));

                float posX = (1000.f - backgroundSprite->getGlobalBounds().size.x) / 2.f;
                float posY = (700.f - backgroundSprite->getGlobalBounds().size.y) / 2.f;
                backgroundSprite->setPosition(Vector2f(posX, posY));
            }

            isOpen = true;
        }
    }

    void close() {
        if (window && window->isOpen()) {
            window->close();
        }
        isOpen = false;
    }

    bool checkEvents() {
        if (!window || !window->isOpen()) return false;

        while (std::optional<Event> event = window->pollEvent()) {
            if (event->is<Event::Closed>()) {
                close();
                return false;
            }
            if (const auto* keyPress = event->getIf<Event::KeyPressed>()) {
                if (keyPress->code == Keyboard::Key::Escape) {
                    close();
                    return false;
                }
            }
        }
        return true;
    }

    void render(const std::map<std::string, AvionVisuel>& avions);

    bool getIsOpen() const { return isOpen && window && window->isOpen(); }

private:
    void drawNearbyAircraft(const std::map<std::string, AvionVisuel>& avions);

    void drawAirportInfo() {
        RectangleShape infoPanel(Vector2f(1000.f, 80.f));
        infoPanel.setPosition(Vector2f(0.f, 620.f));
        infoPanel.setFillColor(Color(0, 0, 0, 200));
        window->draw(infoPanel);

        std::lock_guard<std::mutex> lock(shared_data->parkingsMutex);

        if (shared_data->parkingsLibres.find(airportName) == shared_data->parkingsLibres.end()) {
            return;
        }

        auto& parkings = shared_data->parkingsLibres[airportName];
        auto& avionsAuParking = shared_data->avionsAuParking[airportName];

        int placesLibres = std::count(parkings.begin(), parkings.end(), true);

        Text pisteText(font);
        pisteText.setString("Piste: OPERATIONNELLE");
        pisteText.setCharacterSize(18);
        pisteText.setFillColor(Color::Green);
        pisteText.setOutlineThickness(1.f);
        pisteText.setOutlineColor(Color::Black);
        pisteText.setPosition(Vector2f(20.f, 630.f));
        window->draw(pisteText);

        Text parkingText(font);
        parkingText.setString("Parkings: " + std::to_string(placesLibres) + "/" +
            std::to_string(parkings.size()) + " libres");
        parkingText.setCharacterSize(18);
        parkingText.setFillColor(placesLibres > 0 ? Color::Green : Color::Red);
        parkingText.setOutlineThickness(1.f);
        parkingText.setOutlineColor(Color::Black);
        parkingText.setPosition(Vector2f(20.f, 660.f));
        window->draw(parkingText);

        if (!avionsAuParking.empty()) {
            int xOffset = 400;
            int yOffset = 630;
            Text parkingHeader(font);
            parkingHeader.setString("Avions au parking:");
            parkingHeader.setCharacterSize(16);
            parkingHeader.setFillColor(Color::Cyan);
            parkingHeader.setOutlineThickness(1.f);
            parkingHeader.setOutlineColor(Color::Black);
            parkingHeader.setPosition(Vector2f(xOffset, yOffset));
            window->draw(parkingHeader);

            yOffset += 25;
            auto now = std::chrono::steady_clock::now();

            for (const auto& avion : avionsAuParking) {
                auto tempsParking = std::chrono::duration_cast<std::chrono::seconds>(
                    now - avion.heureArrivee).count();

                Text avionText(font);
                std::ostringstream ossAvion;
                ossAvion << "P" << (avion.numeroPlace + 1) << ": " << avion.code
                    << " (" << tempsParking << "s/" << avion.tempsParkingSecondes << "s)";
                avionText.setString(ossAvion.str());
                avionText.setCharacterSize(14);

                if (tempsParking >= avion.tempsParkingSecondes) {
                    avionText.setFillColor(Color::Yellow);
                }
                else {
                    avionText.setFillColor(Color::White);
                }

                avionText.setOutlineThickness(1.f);
                avionText.setOutlineColor(Color::Black);
                avionText.setPosition(Vector2f(xOffset, yOffset));
                window->draw(avionText);

                yOffset += 20;
                if (yOffset > 680) break;
            }
        }
    }

    void drawParkings() {
        float startX = 100.f;
        float startY = 480.f;
        float spacing = 140.f;

        std::lock_guard<std::mutex> lock(shared_data->parkingsMutex);

        if (shared_data->parkingsLibres.find(airportName) == shared_data->parkingsLibres.end()) {
            return;
        }

        auto& parkings = shared_data->parkingsLibres[airportName];
        auto& avionsAuParking = shared_data->avionsAuParking[airportName];

        for (size_t i = 0; i < parkings.size(); i+=1) {
            bool estLibre = parkings[i];

            RectangleShape parking(Vector2f(100.f, 80.f));
            parking.setPosition(Vector2f(startX + i * spacing, startY));

            parking.setFillColor(estLibre ? Color(40, 80, 40, 150) : Color(120, 40, 40, 150));
            parking.setOutlineThickness(3.f);
            parking.setOutlineColor(estLibre ? Color::Green : Color::Red);
            window->draw(parking);

            Text label(font);
            label.setString("P" + std::to_string(i + 1));
            label.setCharacterSize(22);
            label.setFillColor(Color::White);
            label.setOutlineThickness(2.f);
            label.setOutlineColor(Color::Black);
            label.setPosition(Vector2f(startX + i * spacing + 35.f, startY + 15.f));
            window->draw(label);

            if (!estLibre) {
                for (const auto& avion : avionsAuParking) {
                    if (avion.numeroPlace == static_cast<int>(i)) {
                        Text avionCode(font);
                        avionCode.setString(avion.code);
                        avionCode.setCharacterSize(14);
                        avionCode.setFillColor(Color::Yellow);
                        avionCode.setOutlineThickness(1.f);
                        avionCode.setOutlineColor(Color::Black);
                        avionCode.setPosition(Vector2f(startX + i * spacing + 15.f, startY + 45.f));
                        window->draw(avionCode);
                        break;
                    }
                }
            }
        }

        RectangleShape piste(Vector2f(700.f, 100.f));
        piste.setPosition(Vector2f(150.f, 330.f));
        piste.setFillColor(Color(60, 60, 60, 180));
        piste.setOutlineThickness(4.f);
        piste.setOutlineColor(Color::Green);
        window->draw(piste);

        Text pisteLabel(font);
        pisteLabel.setString("PISTE");
        pisteLabel.setCharacterSize(24);
        pisteLabel.setFillColor(Color::White);
        pisteLabel.setOutlineThickness(2.f);
        pisteLabel.setOutlineColor(Color::Black);
        pisteLabel.setPosition(Vector2f(450.f, 365.f));
        window->draw(pisteLabel);
    }
};

class AeroportWindowManager {
private:
    std::map<std::string, std::unique_ptr<AeroportWindow>> windows;
    Font& font;
    std::string imagePath;
    std::shared_ptr<DataHub> shared_data;

public:
    AeroportWindowManager(Font& f, const std::string& imgPath, std::shared_ptr<DataHub> data)
        : font(f), imagePath(imgPath), shared_data(data) {
    }

    void addAeroport(const std::string& name, int posX, int posY) {
        windows[name] = std::make_unique<AeroportWindow>(name, posX, posY, font, imagePath, shared_data);
    }

    void toggleWindow(const std::string& airportName) {
        auto it = windows.find(airportName);
        if (it != windows.end()) {
            if (it->second->getIsOpen()) {
                it->second->close();
            }
            else {
                it->second->open();
            }
        }
    }

    void updateAll(const std::map<std::string, AvionVisuel>& avions) {
        for (auto& pair : windows) {
            if (pair.second->getIsOpen()) {
                pair.second->checkEvents();
                pair.second->render(avions);
            }
        }
    }

    void closeAll() {
        for (auto& pair : windows) {
            pair.second->close();
        }
    }
};

inline void AeroportWindow::render(const std::map<std::string, AvionVisuel>& avions) {
    if (!window || !window->isOpen()) return;

    window->clear(Color(20, 20, 40));

    if (backgroundLoaded && backgroundSprite) {
        window->draw(*backgroundSprite);
    }

    drawParkings();

    RectangleShape overlay(Vector2f(1000.f, 150.f));
    overlay.setPosition(Vector2f(0.f, 0.f));
    overlay.setFillColor(Color(0, 0, 0, 180));
    window->draw(overlay);

    Text title(font);
    title.setString("Tour de Controle - " + airportName);
    title.setCharacterSize(28);
    title.setFillColor(Color::White);
    title.setOutlineThickness(2.f);
    title.setOutlineColor(Color::Black);
    title.setPosition(Vector2f(20.f, 20.f));
    window->draw(title);

    drawNearbyAircraft(avions);
    drawAirportInfo();

    window->display();
}

inline void AeroportWindow::drawNearbyAircraft(const std::map<std::string, AvionVisuel>& avions) {
    int yOffset = 70;
    int count = 0;

    Text header(font);
    header.setString("Avions en approche :");
    header.setCharacterSize(20);
    header.setFillColor(Color::Cyan);
    header.setOutlineThickness(1.f);
    header.setOutlineColor(Color::Black);
    header.setPosition(Vector2f(20.f, yOffset - 10.f));
    window->draw(header);

    yOffset += 20;

    for (const auto& pair : avions) {
        const auto& av = pair.second;

        int dx = av.position.get_x() - airportX;
        int dy = av.position.get_y() - airportY;
        float distance = std::sqrt(static_cast<float>(dx * dx + dy * dy));

        if (distance < 400.f && count < 4) {
            Text avionInfo(font);
            std::ostringstream oss;
            oss << std::left << std::setw(8) << av.nom << " | "
                << std::setw(5) << static_cast<int>(distance) << "px | "
                << std::setw(6) << av.altitude << "ft | "
                << av.vitesse << " km/h";
            avionInfo.setString(oss.str());
            avionInfo.setCharacterSize(16);

            if (distance < 50 && av.altitude < 2000) {
                avionInfo.setFillColor(Color::Red);
            }
            else if (distance < 150) {
                avionInfo.setFillColor(Color::Yellow);
            }
            else {
                avionInfo.setFillColor(Color::White);
            }

            avionInfo.setOutlineThickness(1.f);
            avionInfo.setOutlineColor(Color::Black);
            avionInfo.setPosition(Vector2f(30.f, yOffset + count * 20.f));
            window->draw(avionInfo);
            count++;
        }
    }

    if (count == 0) {
        Text noAircraft(font);
        noAircraft.setString("Aucun avion en approche");
        noAircraft.setCharacterSize(16);
        noAircraft.setFillColor(Color(150, 150, 150));
        noAircraft.setOutlineThickness(1.f);
        noAircraft.setOutlineColor(Color::Black);
        noAircraft.setPosition(Vector2f(30.f, yOffset));
        window->draw(noAircraft);
    }
}

#endif