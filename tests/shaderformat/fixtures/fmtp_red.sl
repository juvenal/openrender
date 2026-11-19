/* Spec 033 ShaderFormat_Precedence fixture: the colour of the .rslo of a pair. The .slo of the same name is
   compiled from fmtp_green.sl, so the pixel tells which backend rendered. */
surface fmtp()
{
    Ci = color(1, 0, 0);
    Oi = 1;
}
