/* External potential in MeV, with r in fm. */
double barrier_potential(double r, double coulomb, double centrifugal) {
    return coulomb / r + centrifugal / (r * r);
}
