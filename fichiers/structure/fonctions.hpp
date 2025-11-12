#include <iostream>
#include <vector>
#include <string>

class coord {
private:
	int x;
	int y;
public:
	void set_x(int X) { x = X; } // définit la coordonnée x
	void set_y(int Y) { y = Y; } // définit la coordonnée y
	int get_x() { return x; } // retourne la coordonnée x
	int get_y() { return y; } // retourne la coordonnée y
	coord coord(int X, int Y) {
		coord c;
		c.x = X;
		c.y = Y;
		return c;
	}
	coord() : x(0), y(0) {}
};

class avion {
private:
	std::string code;
	int altitude;
	int vitesse;
	coord position;
	std::string destination;
	int place_parking;
	int carburant;
public:
	coord get_position() { return position; } // pour le ccr et l'app pour calculer les trajectoires ainsi que pour les enregistrer

	// ajout de setters et getters utiles
	void set_code(const std::string& c) { code = c; }
	std::string get_code() { return code; }

	void set_altitude(int alt) { altitude = alt; }
	int get_altitude() { return altitude; }

	void set_vitesse(int vit) { vitesse = vit; }
	int get_vitesse() { return vitesse; }

	void set_position(const coord& pos) { position = pos; }

	void set_destination(const std::string& dest) { destination = dest; }
	std::string get_destination() { return destination; }

	void set_place_parking(int place) { place_parking = place; }
	int get_place_parking() { return place_parking; }

	void set_carburant(int carb) { carburant = carb; }
	int get_carburant() { return carburant; }
};

class ccr { // vérificateur à l'international
public:
	void calcul_trajectoire(avion& avion) {
		// calcul d'une trajectoire sans collision pour l'avion
		// à faire
	}

	void planning() {
		// gère le planning aérien dans un fichier .json
		// à faire
	}
};

class app { // abord d'un aéroport
public:
	void envoie_infos_avion() {
		// envoie la trajectoire et altitude à suivre à l'avion après réception du signal
		// à faire
	}

	void piste_libre() {
		// demande à twr si la piste d'atterrissage est libre
		// à faire
	}

	void urgence() {
		// gère la situation d'urgence pour un avion
		// à faire
	}
};

class twr { // circulation dans l'aéroport
private:
	bool piste; // vrai si libre, false sinon
	std::vector<bool> parking; // index = la place et l'élément indique si la place est libre ou non 
public:
	twr(int nb_places) : piste(true), parking(nb_places, true) {}

	bool get_piste() { return piste; } // pour voie_libre() de l'app

	int trouver_place_libre() {
		for (int i = 0; i < parking.size(); ++i) {
			if (parking[i]) return i; // place libre trouvée
		}
		return -1; // pas de place libre
	}

	void décollage() {
		// logique du décollage : libérer la piste et la place de parking
		piste = false; // piste occupée
		// mettre à jour la place de parking libre
		// à faire
	}
};
