# Generate the fixed Coulomb reference values used by the Rust unit tests.
# mpmath's F_l and G_l use the DLMF Coulomb normalization (33.2).
import mpmath as mp
mp.mp.dps = 80
for eta, ell, rho, kin in [(0.5, 0, 3, 0.7), (5, 1, 2, 1), (30, 0, 1, 1)]:
    eta, rho, kin = map(mp.mpf, (eta, rho, kin))
    h = lambda x: mp.coulombg(ell, eta, x) + 1j * mp.coulombf(ell, eta, x)
    wave, derivative = h(rho), mp.diff(h, rho)
    incident = (wave + derivative / (1j * kin)) / 2
    print(eta, ell, rho, kin, mp.nstr(mp.log10(1 / kin / abs(incident)**2), 40))
