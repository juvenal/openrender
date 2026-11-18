/* Spec 057 contract shader (tests/shading_contract, case_area_light.cpp): a light whose Cl is the constant c. */
light ca_light_const(color c = 1)
{
    illuminate(P) {
        Cl = c;
    }
}
