#ifndef ALPHA_NUMEROV_H
#define ALPHA_NUMEROV_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Version 1: radii fm, energies MeV, mass MeV/c^2. kind: 0 square, 1 Woods-Saxon. */
typedef struct {
    uint32_t version, kind; int32_t l; uint32_t reserved;
    double daughter_z, reduced_mass_mev, q_mev, depth_mev, radius_fm, diffuseness_fm, charge_radius_fm;
    double lead_radius_fm, match_radius_fm, step_fm;
} alpha_numerov_request;
typedef struct {
    uint32_t version, status; /* 0 converged, 1 unconverged, 2 invalid, 3 numerical failure */
    double log10_transmission, step_error_decades, match_error_decades;
    char message[192];
} alpha_numerov_result;
/* Caller owns both structs. Returns 0 if a result was written, -1 for null pointers.
   No allocation crosses the ABI; no Rust unwind crosses it. */
int32_t alpha_numerov_solve(const alpha_numerov_request*, alpha_numerov_result*);
#ifdef __cplusplus
}
#endif
#endif
