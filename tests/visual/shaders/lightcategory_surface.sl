/* GitHub #199: Ci is the sum of the colours of the lights that illuminance(cat) selects, so one pixel tells which lights
 * a category picks out (an .rslo surface: the .slo illuminance() ignores its category argument, GitHub #200). */
surface lightcategory_surface(string cat = "")
{
    color s = 0;
    illuminance(cat, P) { s += Cl; }
    Ci = s;
    Oi = 1;
}
