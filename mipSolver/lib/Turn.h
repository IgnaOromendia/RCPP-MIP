#ifndef TURN_H
#define TURN_H

struct Turn {
	// Es un giro desde v que usa la intersección w para girar hacia u
	int v,w,u;
	Turn(int v, int w, int u): v(v), w(w), u(u) {}

	bool operator<(const Turn& otro) const {
		if (v != otro.v) return v < otro.v;
		if (w != otro.w) return w < otro.w;
		return u < otro.u;
	}
};


#endif