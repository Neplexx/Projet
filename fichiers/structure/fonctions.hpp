#include <iostream>
#include <vector>

class Coord {
private:
	int x;
	int y;
public:
	void set_x(int X);
	void set_y(int Y);
	int get_x();
	int get_y();
	Coord Coord(int X, int Y);
	Coord Coord();
};
class Avion {
private:
	std::string code;
	int altitude;
	int vitesse;
	Coord position;
	std::string destination;
	int place_parking;
	int carburant;
public:
	Coord get_position(); //pour le CCR et l'APP pour calculer les trajectoires ainsi que pour les enregistrer
};

class CCR { //vérificateur à l'internationnal
public:
	void calcul_trajectoire(Avion &avion); //permet de suivre une trajectoire entre 2 aéroports sans collisions
	void planning(); //gère le planning aérien dans un .json

};

class APP { // Abord d'un aéroport
public:
	void envoie_infos_avion(); // envoie la trajectoire/altitude à suivre à un avion après réception de son signal
	void piste_libre(); //demande à TWR si la piste d'attérissage est libre
	void urgence();

};

class TWR { // circulation dans l'aéroport
private:
	bool piste; //Vrai si libre, False sinon
	std::vector<bool> parking; //index = la place et l'élément si la place est libre ou non 
public:
	bool get_piste(); // pour voie_libre() de l'APP
	int trouver_place_libre();
	void décollage();

};