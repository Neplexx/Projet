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
                VideoMode({ 1200, 800 }),
                "TWR - " + airportName
            );
            window->setFramerateLimit(60);

            if (backgroundLoaded && backgroundSprite) {
                float scaleX = 1200.f / backgroundTexture.getSize().x;
                float scaleY = 800.f / backgroundTexture.getSize().y;
                float scale = std::min(scaleX, scaleY);
                backgroundSprite->setScale(Vector2f(scale, scale));

                float posX = (1200.f - backgroundSprite->getGlobalBounds().size.x) / 2.f;
                float posY = (800.f - backgroundSprite->getGlobalBounds().size.y) / 2.f;
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
    void drawHeader() {
        RectangleShape headerBg(Vector2f(1200.f, 100.f));
        headerBg.setPosition(Vector2f(0.f, 0.f));
        headerBg.setFillColor(Color(25, 30, 45, 240));
        window->draw(headerBg);

        RectangleShape separatorLine(Vector2f(1200.f, 3.f));
        separatorLine.setPosition(Vector2f(0.f, 100.f));
        separatorLine.setFillColor(Color(0, 180, 240));
        window->draw(separatorLine);

        Text title(font);
        title.setString("TOUR DE CONTROLE - " + airportName);
        title.setCharacterSize(32);
        title.setFillColor(Color(0, 200, 255));
        title.setStyle(Text::Bold);
        title.setPosition(Vector2f(40.f, 25.f));
        window->draw(title);

        Text status(font);
        status.setString("OPERATIONAL");
        status.setCharacterSize(18);
        status.setFillColor(Color(0, 255, 100));
        status.setPosition(Vector2f(40.f, 65.f));
        window->draw(status);
    }

    void drawNearbyAircraft(const std::map<std::string, AvionVisuel>& avions) {
        RectangleShape approachPanel(Vector2f(560.f, 280.f));
        approachPanel.setPosition(Vector2f(20.f, 120.f));
        approachPanel.setFillColor(Color(20, 25, 35, 220));
        approachPanel.setOutlineThickness(2.f);
        approachPanel.setOutlineColor(Color(0, 180, 240));
        window->draw(approachPanel);

        Text panelTitle(font);
        panelTitle.setString("AVIONS EN APPROCHE");
        panelTitle.setCharacterSize(22);
        panelTitle.setFillColor(Color(0, 200, 255));
        panelTitle.setStyle(Text::Bold);
        panelTitle.setPosition(Vector2f(40.f, 135.f));
        window->draw(panelTitle);

        RectangleShape tableHeader(Vector2f(520.f, 35.f));
        tableHeader.setPosition(Vector2f(40.f, 170.f));
        tableHeader.setFillColor(Color(30, 40, 60, 200));
        window->draw(tableHeader);

        std::vector<std::string> headers = { "CODE", "DISTANCE", "ALTITUDE", "VITESSE" };
        std::vector<float> headerPositions = { 50.f, 180.f, 310.f, 440.f };

        for (size_t i = 0; i < headers.size(); ++i) {
            Text headerText(font);
            headerText.setString(headers[i]);
            headerText.setCharacterSize(16);
            headerText.setFillColor(Color(150, 160, 180));
            headerText.setStyle(Text::Bold);
            headerText.setPosition(Vector2f(headerPositions[i], 178.f));
            window->draw(headerText);
        }

        // Liste des avions
        int yOffset = 215;
        int count = 0;
        const int MAX_DISPLAY = 6;

        for (const auto& pair : avions) {
            if (count >= MAX_DISPLAY) break;

            const auto& av = pair.second;
            int dx = av.position.get_x() - airportX;
            int dy = av.position.get_y() - airportY;
            float distance = std::sqrt(static_cast<float>(dx * dx + dy * dy));

            if (distance < 400.f) {
                if (count % 2 == 0) {
                    RectangleShape rowBg(Vector2f(520.f, 30.f));
                    rowBg.setPosition(Vector2f(40.f, yOffset - 5.f));
                    rowBg.setFillColor(Color(15, 20, 30, 150));
                    window->draw(rowBg);
                }

                Color textColor = Color::White;
                if (distance < 50 && av.altitude < 2000) {
                    textColor = Color(255, 50, 50);
                }
                else if (distance < 150) {
                    textColor = Color(255, 200, 0);
                }

                Text codeText(font);
                codeText.setString(av.nom);
                codeText.setCharacterSize(16);
                codeText.setFillColor(textColor);
                codeText.setPosition(Vector2f(50.f, yOffset));
                window->draw(codeText);

                Text distText(font);
                distText.setString(std::to_string(static_cast<int>(distance)) + " px");
                distText.setCharacterSize(16);
                distText.setFillColor(textColor);
                distText.setPosition(Vector2f(180.f, yOffset));
                window->draw(distText);

                Text altText(font);
                altText.setString(std::to_string(av.altitude) + " ft");
                altText.setCharacterSize(16);
                altText.setFillColor(textColor);
                altText.setPosition(Vector2f(310.f, yOffset));
                window->draw(altText);

                Text speedText(font);
                speedText.setString(std::to_string(av.vitesse) + " km/h");
                speedText.setCharacterSize(16);
                speedText.setFillColor(textColor);
                speedText.setPosition(Vector2f(440.f, yOffset));
                window->draw(speedText);

                yOffset += 30;
                count++;
            }
        }

        if (count == 0) {
            Text noAircraft(font);
            noAircraft.setString("Aucun avion en approche");
            noAircraft.setCharacterSize(18);
            noAircraft.setFillColor(Color(120, 130, 150));
            noAircraft.setPosition(Vector2f(220.f, 260.f));
            window->draw(noAircraft);
        }
    }

    void drawParkings() {
        RectangleShape parkingPanel(Vector2f(560.f, 380.f));
        parkingPanel.setPosition(Vector2f(620.f, 120.f));
        parkingPanel.setFillColor(Color(20, 25, 35, 220));
        parkingPanel.setOutlineThickness(2.f);
        parkingPanel.setOutlineColor(Color(0, 180, 240));
        window->draw(parkingPanel);

        Text panelTitle(font);
        panelTitle.setString("POSITIONS DE PARKING");
        panelTitle.setCharacterSize(22);
        panelTitle.setFillColor(Color(0, 200, 255));
        panelTitle.setStyle(Text::Bold);
        panelTitle.setPosition(Vector2f(640.f, 135.f));
        window->draw(panelTitle);

        std::lock_guard<std::mutex> lock(shared_data->parkingsMutex);

        if (shared_data->parkingsLibres.find(airportName) == shared_data->parkingsLibres.end()) {
            return;
        }

        auto& parkings = shared_data->parkingsLibres[airportName];
        auto& avionsAuParking = shared_data->avionsAuParking[airportName];

        int placesLibres = std::count(parkings.begin(), parkings.end(), true);

        Text statsText(font);
        std::ostringstream statsStream;
        statsStream << "Disponibles: " << placesLibres << " / " << parkings.size();
        statsText.setString(statsStream.str());
        statsText.setCharacterSize(18);
        statsText.setFillColor(placesLibres > 0 ? Color(0, 255, 100) : Color(255, 100, 0));
        statsText.setPosition(Vector2f(640.f, 170.f));
        window->draw(statsText);

        float startX = 650.f;
        float startY = 220.f;
        float parkingWidth = 160.f;
        float parkingHeight = 100.f;
        float spacingX = 20.f;
        float spacingY = 20.f;

        int parkingsPerRow = 3;

        for (size_t i = 0; i < parkings.size(); ++i) {
            int row = i / parkingsPerRow;
            int col = i % parkingsPerRow;

            float posX = startX + col * (parkingWidth + spacingX);
            float posY = startY + row * (parkingHeight + spacingY);

            bool estLibre = parkings[i];

            RectangleShape parking(Vector2f(parkingWidth, parkingHeight));
            parking.setPosition(Vector2f(posX, posY));

            if (estLibre) {
                parking.setFillColor(Color(20, 80, 40, 180));
                parking.setOutlineColor(Color(0, 200, 100));
            }
            else {
                parking.setFillColor(Color(80, 40, 40, 180));
                parking.setOutlineColor(Color(255, 100, 100));
            }
            parking.setOutlineThickness(3.f);
            window->draw(parking);

            Text label(font);
            label.setString("P" + std::to_string(i + 1));
            label.setCharacterSize(24);
            label.setFillColor(Color::White);
            label.setStyle(Text::Bold);
            label.setPosition(Vector2f(posX + 10.f, posY + 10.f));
            window->draw(label);

            if (!estLibre) {
                for (const auto& avion : avionsAuParking) {
                    if (avion.numeroPlace == static_cast<int>(i)) {
                        auto now = std::chrono::steady_clock::now();
                        auto tempsParking = std::chrono::duration_cast<std::chrono::seconds>(
                            now - avion.heureArrivee).count();

                        Text avionCode(font);
                        avionCode.setString(avion.code);
                        avionCode.setCharacterSize(20);
                        avionCode.setFillColor(Color(255, 220, 0));
                        avionCode.setStyle(Text::Bold);
                        avionCode.setPosition(Vector2f(posX + 10.f, posY + 45.f));
                        window->draw(avionCode);

                        Text tempsText(font);
                        std::ostringstream timeStream;
                        timeStream << tempsParking << "s / " << avion.tempsParkingSecondes << "s";
                        tempsText.setString(timeStream.str());
                        tempsText.setCharacterSize(14);

                        if (tempsParking >= avion.tempsParkingSecondes) {
                            tempsText.setFillColor(Color(0, 255, 100));
                        }
                        else {
                            tempsText.setFillColor(Color(200, 200, 200));
                        }

                        tempsText.setPosition(Vector2f(posX + 10.f, posY + 70.f));
                        window->draw(tempsText);
                        break;
                    }
                }
            }
            else {
                Text libreText(font);
                libreText.setString("LIBRE");
                libreText.setCharacterSize(18);
                libreText.setFillColor(Color(100, 200, 100, 180));
                libreText.setPosition(Vector2f(posX + 45.f, posY + 55.f));
                window->draw(libreText);
            }
        }
    }

    void drawStatusBar() {
        RectangleShape statusBar(Vector2f(1200.f, 70.f));
        statusBar.setPosition(Vector2f(0.f, 730.f));
        statusBar.setFillColor(Color(25, 30, 45, 240));
        window->draw(statusBar);

        RectangleShape separatorLine(Vector2f(1200.f, 3.f));
        separatorLine.setPosition(Vector2f(0.f, 727.f));
        separatorLine.setFillColor(Color(0, 180, 240));
        window->draw(separatorLine);

        std::lock_guard<std::mutex> lock(shared_data->parkingsMutex);

        if (shared_data->avionsAuParking.find(airportName) != shared_data->avionsAuParking.end()) {
            auto& avionsAuParking = shared_data->avionsAuParking[airportName];

            Text infoText(font);
            std::ostringstream infoStream;
            infoStream << "Avions au parking: " << avionsAuParking.size();
            infoText.setString(infoStream.str());
            infoText.setCharacterSize(20);
            infoText.setFillColor(Color(0, 200, 255));
            infoText.setPosition(Vector2f(40.f, 745.f));
            window->draw(infoText);

            auto now = std::chrono::steady_clock::now();
            int pretsDecollage = 0;
            for (const auto& avion : avionsAuParking) {
                auto tempsParking = std::chrono::duration_cast<std::chrono::seconds>(
                    now - avion.heureArrivee).count();
                if (tempsParking >= avion.tempsParkingSecondes) {
                    pretsDecollage++;
                }
            }

            if (pretsDecollage > 0) {
                Text readyText(font);
                readyText.setString("Prets au decollage: " + std::to_string(pretsDecollage));
                readyText.setCharacterSize(20);
                readyText.setFillColor(Color(0, 255, 100));
                readyText.setPosition(Vector2f(400.f, 745.f));
                window->draw(readyText);
            }
        }
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

    window->clear(Color(15, 20, 30));

    if (backgroundLoaded && backgroundSprite) {
        window->draw(*backgroundSprite);
    }

    drawHeader();
    drawNearbyAircraft(avions);
    drawParkings();
    drawStatusBar();

    window->display();
}

#endif