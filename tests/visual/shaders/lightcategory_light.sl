/* GitHub #199: a light whose category is given per instance in the RIB ("string __category"). The shader declares an empty
 * default, so a light that reaches illuminance("a") through this shader got its category from the RIB. Each light is a
 * distant light of colour c, so the receiver's position does not matter. */
light lightcategory_light(color c = color(1, 1, 1); string __category = "")
{
    solar(vector(0, 0, 1), 0) Cl = c;
}
