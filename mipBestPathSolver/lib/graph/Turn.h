#ifndef TURN_H
#define TURN_H

#include <utility>

// Es un giro desde v que usa la intersección w para girar hacia u
struct Turn {
	int v,w,u;
	Turn(int v, int w, int u): v(v), w(w), u(u) {}

	bool operator<(const Turn& otro) const {
		if (v != otro.v) return v < otro.v;
		if (w != otro.w) return w < otro.w;
		return u < otro.u;
	}
};

// Es un giro extendido desde v que usa la intersección neutra w_i -> w_j para girar hacia u
struct ExtendedTurn{
	std::pair<int, int> from, to;
	ExtendedTurn(int v, int w_i, int w_j, int u): from({v, w_i}), to({w_j, u}) {}

	bool operator<(const ExtendedTurn& other) const {
		if (from == other.from) return to < other.to;
		return from < other.from;
	}
};

#endif
