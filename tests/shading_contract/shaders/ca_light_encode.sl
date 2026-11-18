/* Spec 057 contract shader: Cl encodes one light-surface global, chosen by `which`:
   0 P, 1 N, 2 Ng, 3 dPdu, 4 dPdv, 5 (u, v, 0), 6 (s, t, 0), 7 Ps (the receiver's point). */
light ca_light_encode(float which = 0)
{
    illuminate(P) {
        color c = 0;
        if (which == 0)
            c = color(comp(P, 0), comp(P, 1), comp(P, 2));
        else if (which == 1)
            c = color(comp(N, 0), comp(N, 1), comp(N, 2));
        else if (which == 2)
            c = color(comp(Ng, 0), comp(Ng, 1), comp(Ng, 2));
        else if (which == 3)
            c = color(comp(dPdu, 0), comp(dPdu, 1), comp(dPdu, 2));
        else if (which == 4)
            c = color(comp(dPdv, 0), comp(dPdv, 1), comp(dPdv, 2));
        else if (which == 5)
            c = color(u, v, 0);
        else if (which == 6)
            c = color(s, t, 0);
        else
            c = color(comp(Ps, 0), comp(Ps, 1), comp(Ps, 2));
        Cl = c;
    }
}
