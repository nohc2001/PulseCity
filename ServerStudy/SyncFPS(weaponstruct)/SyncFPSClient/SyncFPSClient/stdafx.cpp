#include "stdafx.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define TTF_FONT_PARSER_IMPLEMENTATION
#include "ttfParser.h"

bool bPointInPolygonRange(XMVECTOR p, std::vector<XMFLOAT3>& polygon) {
	int h[4] = {}; // x+ x- y+ y-

	float px = p.m128_f32[0];
	float py = p.m128_f32[1];
	for (int o = 0; o < polygon.size(); o++) {
		XMFLOAT3 p1;
		XMFLOAT3 p2;
		if (o == polygon.size() - 1) {
			p1 = polygon.at(o);
			p2 = polygon.at(0);
		}
		else {
			p1 = polygon.at(o);
			p2 = polygon.at(o + 1);
		}

		float xrate, yrate;
		XMFLOAT3 inDot;
		xrate = p2.x - p1.x;
		yrate = p2.y - p1.y;
		inDot = p1;
		float xx = xrate * (py - inDot.y) / yrate + inDot.x;
		float yy = yrate * (px - inDot.x) / xrate + inDot.y;

		if (!_isnanf(xx) && !__is_infinityf(xx)) {
			if (px <= xx &&
				((p1.y <= py && py <= p2.y) || (p2.y <= py && py <= p1.y))) {
				h[0]++;
			}

			if (px >= xx &&
				((p1.y <= py && py <= p2.y) || (p2.y <= py && py <= p1.y))) {
				h[1]++;
			}
		}

		if (!_isnanf(yy) && !__is_infinityf(xx)) {
			if (py <= yy &&
				((p1.x <= px && px <= p2.x) || (p2.x <= px && px <= p1.x))) {
				h[2]++;
			}

			if (py >= yy &&
				((p1.x <= px && px <= p2.x) || (p2.x <= px && px <= p1.x))) {
				h[3]++;
			}
		}
	}

	for (int i = 0; i < 4; ++i) {
		if (h[i] % 2 == 1) {
			return true;
		}
	}

	return false;
}
