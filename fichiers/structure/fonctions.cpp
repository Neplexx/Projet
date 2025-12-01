#include "fonctions.hpp"

void Coord::set_x(int X) {
	x = X;
}
void Coord::set_y(int Y) {
	y = Y;
}

int Coord::get_x() const {
	return x;
}
int Coord::get_y() const {
	return y;
}

Coord::Coord(int X, int Y) : x(X) , y(Y) {}

bool Coord::operator==(const Coord& other) const { 
	return x == other.x && y == other.y; 
}
std::ostream& operator<<(std::ostream& os, const Coord& c) {
	os << "(" << c.x << ", " << c.y << ")";
	return os;
};


//fonctions class Avion

Coord Avion::get_position() const {
	return position;
}
std::string Avion::get_code() const {
	return code;
}
int Avion::get_altitude() const {
	return altitude;
}
int Avion::get_vitesse() const {
	return vitesse;
}
Coord Avion::get_destination() const {
	return destination;
}
int Avion::get_place_parking() const {
	return place_parking;
}
int Avion::get_carburant() const {
	return carburant;
}

void Avion::set_position(const Coord& pos) {
	position = pos;
}
void Avion::set_code(const std::string& c) {
	code = c;
}
void Avion::set_altitude(int alt) {
	altitude = alt;
}
void Avion::set_vitesse(int vit) {
	vitesse = vit;
}
void Avion::set_destination(const Coord& dest) {
	destination = dest;
}
void Avion::set_place_parking(int place) {
	place_parking = place;
}
void Avion::set_carburant(int carb) {
	carburant = carb;
}

Avion::Avion(const std::string& code_init, int alt_init, int vit_init, const Coord& pos_init, const Coord& dest_init, int parking_init, int carb_init) : code(code_init), altitude(alt_init), vitesse(vit_init), position(pos_init), destination(dest_init), place_parking(parking_init), carburant(carb_init) {}

// Fonctions CCR

void CCR::planning() {
	using json = nlohmann::json;
	json planning;
	planning["Days"] = { "Lundi", "Mardi", "Mercredi", "Jeudi", "Vendredi", "Samedi", "Dimanche" };
	planning["Arrival"] = { "Nice", "Rennes", "Paris", "Strasbourg", "Lille", "Toulouse" };
	planning["Departure"] = planning["Arrival"];
	planning["Lenght"] = { {"1h45", "1h45", "1h30", "1h30", "1h15","0h00"}, {"1h45","0h45","1h45","1h15","1h30"}, {"1h45","0h45","1h00","0h30","1h15","0h00"}, {"1h30","1h45","1h00","1h00","2h00","0h00"},{"1h30", "1h15", "0h30", "1h00", "1h30","0h00" },{"1h15", "1h30", "1h15", "2h00", "1h30","0h00"}, {"0h00","0h00","0h00","0h00","0h00","0h00"} };
	planning["Time"] = { "0h00", "0h15", "0h30", "0h45", "1h00", "1h15", "1h30", "1h45", "2h00", "2h15", "2h30", "2h45", "3h00", "3h15", "3h30", "3h45", "4h00", "4h15", "4h30", "4h45", "5h00", "5h15", "5h30", "5h45", "6h00", "6h15", "6h30", "6h45", "7h00", "7h15", "7h30", "7h45", "8h00", "8h15", "8h30", "8h45", "9h00", "9h15", "9h30", "9h45", "10h00", "10h15", "10h30", "10h45", "11h00", "11h15", "11h30", "11h45", "12h00", "12h15", "12h30", "12h45", "13h00", "13h15", "13h30", "13h45", "14h00", "14h15", "14h30", "14h45", "15h00", "15h15", "15h30", "15h45", "16h00", "16h15", "16h30", "16h45", "17h00", "17h15", "17h30", "17h45", "18h00", "18h15", "18h30", "18h45", "19h00", "19h15", "19h30", "19h45", "20h00", "20h15", "20h30", "20h45", "21h00", "21h15", "21h30", "21h45", "22h00", "22h15", "22h30", "22h45", "23h00", "23h15", "23h30", "23h45" };
	planning["Delay"] = { "0h30" };
	planning["flights"] = {};
	for (int i = 0; i < planning["Days"].size(); i += 1) {
		for (int j = 0; j < planning["Departure"].size(); j += 1) {
			planning["flights"].push_back({
				{"day", planning["Days"][i]},
				{"departure", planning["Departure"][j]},
				{"arrival", planning["Arrival"][j]},
				{"length", planning["Lenght"][i][j]},
				{"time", planning["Time"][(i + j) % planning["Time"].size()]},
				{"delay", planning["Delay"][0]}
				});
		}
	}
	std::fstream file("schedules.json", std::ios::in | std::ios::trunc);
	if (file.is_open()) {
		file << planning.dump(4); // écrit le JSON avec indentation
		file.close();             // facultatif, mais propre
	}
	else {
		std::cerr << "Erreur : impossible d'ouvrir le fichier planning.json\n";
	}
}

void journal(const std::string& data) {
	std::ofstream journal_f("journal.json", std::ios::app);
	if (journal_f.is_open()) {
		journal_f << data << std::endl;
		journal_f.close();
	}
	else {
		std::cerr << "Erreur : impossible d'ouvrir le fichier journal.log\n";
	}
}

void CCR::calcul_trajectoire(Avion& avion) {
	// Utiliser A* pour trouver le chemin optimal
	// 1. Créer une grille de l'espace aérien
	// 2. Marquer les zones interdites (météo, autres avions)
	// 3. Calculer le coût f(n) = g(n) + h(n)
	//    - g(n) = distance parcourue
	//    - h(n) = estimation distance restante
	// 4. Suivre le chemin optimal trouvé
	// Parcoure toutes les cases
	int nb_lignes = 300;    // nombre de lignes (hauteur)
	int nb_colonnes = 500; // nombre de colonnes (largeur)

	//Grille initialisée à zéro (0 = libre, 1 = obstacle, 2 = avion, 3 = arrivée)
	std::vector<std::vector<int>> grille(nb_lignes, std::vector<int>(nb_colonnes, 0));
	//grille[100][150] = 1; // Exemple d'obstacle
	//définir chaque aéroport à une case
	grille[avion.get_position().get_y()][avion.get_position().get_x()] = 2; // position actuelle
	grille[avion.get_destination().get_y()][avion.get_destination().get_x()] = 3; // destination
	
	int x1 = avion.get_position().get_x();
	int y1 = avion.get_position().get_y();
	int x2 = avion.get_destination().get_x();
	int y2 = avion.get_destination().get_y();
	std::vector<Coord> path;
	int dx = (x2 - x1);
	int dy = (y2 - y1);
	while (x1 != x2 || y1 != y2) {
		path.push_back(Coord(x1, y1));
		if (x1 != x2) x1 += dx;
		if (y1 != y2) y1 += dy;
	}
	int steps = std::max(std::abs(dx), std::abs(dy));
	
	float stepX = static_cast<float>(dx) / steps;
	float stepY = static_cast<float>(dy) / steps;

	float current_x = x1;
	float current_y = y1;

	for (int i = 0; i <= steps; ++i) {
		path.push_back(Coord(static_cast<int>(current_x), static_cast<int>(current_y)));
		current_x += stepX;
		current_y += stepY;
	}

	std::cout << std::endl;
}

//Fonctions TWR

bool TWR::get_piste() const {
	return piste;
}
void TWR::set_piste(bool facteur) {
	piste = facteur;
}

int TWR::trouver_place_libre() {
	for (int i = 0; i < parking.size(); i+=1) {
		if (parking[i]) return i;
	}
	return -1;
}

TWR::TWR(int nb_places) : piste(true), parking(nb_places, true){}



