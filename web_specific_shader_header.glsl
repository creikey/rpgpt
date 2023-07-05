@block clamp_to_border
	//WebGL does not support GL_CLAMP_TO_BORDER, or border colors at all it seems, so we have to check explicitly.
	if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0)
		return 1.0;
@end