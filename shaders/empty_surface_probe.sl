/* GitHub #60 regression: a surface shader that writes neither Ci nor Oi.
 * Ci and Oi must then be Cs and Os, whatever the other bound shaders
 * reference. Unlike Surface "null", which binds no shader at all (the
 * renderer then shades with |I.N|), this is a real, empty instance. See
 * examples/rib/tests/sphere-volume-ci-default-reyes-rslo.rib.
 */
surface empty_surface_probe()
{
}
