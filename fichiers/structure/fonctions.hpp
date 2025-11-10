#include <iostream>
class Coord {
private:
	int x;
	int y;
public:
	void setx(int x);
	void sety(int y);
	void getx();
	void gety();
	Coord Coord(int x, int y);
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

};