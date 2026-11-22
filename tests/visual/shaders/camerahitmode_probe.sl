/* GitHub #117: camerahitmode is a hint about the opacity of a camera-visible
 * object, not a switch for the shader. PRMan runs the shader of every
 * camera-visible object and, with "primitive", takes the opacity from the
 * Opacity attribute instead of the shader's Oi.
 *
 * The probe returns a colour no attribute carries and an opacity that
 * differs from the Opacity attribute (1 by default), so one pixel tells
 * both things apart: the colour is the shader's whenever the shader ran,
 * and the alpha is the attribute's (1) under "primitive", the shader's
 * (0.25) under "shader".
 */
surface camerahitmode_probe()
{
    Ci = color(0.2, 0.4, 0.6);
    Oi = color(0.25, 0.25, 0.25);
}
