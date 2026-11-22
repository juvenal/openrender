/* Spec 057 contract shader: a light that WRITES the global N (the shipped arealight does), then returns the constant c. The
   receiver's N and the lightSideN snapshot must be unchanged by it. */
light ca_light_writen(color c = 1)
{
    N = normal(0, 0, -7);
    illuminate(P) {
        Cl = c;
    }
}
