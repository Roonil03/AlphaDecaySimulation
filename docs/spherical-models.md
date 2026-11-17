# Spherical potential models and Te-104 sources

The optional spherical models use the alpha-daughter reduced mass in MeV/c²,
Q in MeV and radii in fm. The nuclear attraction is either a square well
`-V0` inside its radius and zero outside, or a Woods–Saxon
`-V0/(1+exp((r-R)/a))`. The electrostatic interaction is that of an alpha
particle and a uniformly charged daughter sphere inside its charge radius,
continuing as `2 Z_d e²/r` outside. The physical centrifugal term is
`(ħc)² l(l+1)/(2 μ r²)`. The separately named Langer WKB option replaces
`l(l+1)` by `(l+1/2)²`; it is an approximation for WKB, not the physical
potential. Neither model adjusts parameters to reproduce measured lifetimes.

The solver finds a single allowed pocket followed by an outgoing forbidden
barrier. A square-well jump may itself be the inner barrier boundary. It uses
bracketed roots and adaptive action quadrature, and rejects unsupported
multiple-pocket topologies. Its probability is `P = exp(-2 action)` and
`T½ = ln(2)/(assault_hz * preformation * P)`; both preformation and assault
frequency are explicit assumptions, not measured values. Results are in log
space. Zero preformation means infinite half-life, represented as a status and
null value. WKB, nuclear geometry, preformation and frequency all limit
physical accuracy; numerical tolerances do not represent experimental error.

The supported well-depth range is `0 < V0 <= 10000 MeV`. Greater depths are
rejected as invalid input before topology classification because the fixed
fine scan through `max(R_charge, R_nuclear + 40a)` is not validated for their
extended nuclear tails. This bound describes numerical scope, not a physical
fit. Q = 0 returns `closed_barrier`; positive Q below 0.01 MeV uses the spherical
solver without an external/Qi/Ren comparison. The CLI and viewer share this
calculation and reference-availability path.

The Te-104 entry is separate from the 15 prior catalog entries and their
legacy validation groups. Cox et al., [Nature 654, 52–56 (2026)](https://doi.org/10.1038/s41586-026-10581-w),
report a direct half-life of 7.2 (+2.3, −1.5) ns. Their [preprint](https://doi.org/10.21203/rs.3.rs-7991707/v1)
reports total decay energy Qα = 5.23(9) MeV, not the alpha-particle kinetic
energy alone. The 2018 [Auranen et al.](https://doi.org/10.1103/PhysRevLett.121.182501)
<18 ns result and the 2019 <4 ns limit (see [ENSDF references](https://www.nndc.bnl.gov/ensdf/getrefs.jsp?dsid=104TE+A+DECAY%3ATENTATIVE&recid=100050007))
are historical upper limits, not equality measurements. Their tension with
newer data is not resolved here. [Mohr (2007)](https://doi.org/10.1140/epja/i2006-10168-7)
predicted Qα = 5.42 ± 0.07 MeV and approximately 5 ns; that is a theoretical
prediction, not an observation. The 100% alpha branch and binding energy are
older evaluation inputs, not determinations from Cox et al. Upper limits and
predictions must not be treated as equality references in MAE calculations.

The [AME2020 unrounded mass table](https://www-nds.iaea.org/amdc/ame2020/mass_1.mas20.txt)
row for `52 52 104 Te` lists binding energy per nucleon 8157.3256 ± 3.0539 keV,
without the table's `#` estimated-value marker. Multiplying both values by
104/1000 gives 848.3618624 ± 0.3176056 MeV. This older input is retained with
its source and uncertainty; it is not a binding energy measured by Cox et al.
