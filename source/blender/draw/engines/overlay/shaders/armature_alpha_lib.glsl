uniform vec2 wireAlpha = vec2(1.0, 0.0);

float wire_alpha(float z)
{
	float alpha = wireAlpha.x;

	if (wireAlpha.y > 0) {
		/* Linearize perspective depth. */
		if (ProjectionMatrix[3][3] == 0.0) {
			float near_far_ratio = (ProjectionMatrix[2][2] + 1.0) / (ProjectionMatrix[2][2] - 1.0);
			z = 1 - (1 - 1/mix(1/near_far_ratio, 1, z)) / (1 - near_far_ratio);
		}

		float k = wireAlpha.y;

		if (k >= 1) {
			alpha *= mix(pow(1 - z, k), 1 - z, pow(z, 1/k)/k);
		}
		else {
			alpha *= mix(1 - pow(z, 1/k), 1 - z, pow(1 - z, k)*k);
		}
	}

	return clamp(alpha, 0, 1);
}
