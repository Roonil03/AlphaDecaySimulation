#include <cmath>
extern "C" double barrier_potential(double, double, double);

/* x86-64 SSE2 is baseline; other architectures use the standard library. */
static inline double fast_sqrt(double x) {
#if defined(__x86_64__) && (defined(__GNUC__) || defined(__clang__))
    double result;
    __asm__("sqrtsd %1, %0" : "=x"(result) : "x"(x));
    return result;
#else
    return std::sqrt(x);
#endif
}

static double integrate(double radius, double outer, double q,
                                  double coulomb, double centrifugal, double mu, bool portable, int n) {
    if (radius >= outer) return 0.0;
    /* Logarithmic radius resolves small R/b; 2t-t^2 smooths the outer endpoint. */

    const double log_ratio = std::log(outer / radius);
    double sum = 0.0;
    for (int i = 0; i <= n; ++i) {
        double t = static_cast<double>(i) / n;
        double r = radius * std::exp(log_ratio * t * (2.0 - t));
        double difference = barrier_potential(r, coulomb, centrifugal) - q;
        double arg = 2.0 * mu * (difference > 0.0 ? difference : 0.0);
        double value = (portable ? std::sqrt(arg) : fast_sqrt(arg))
                       * (2.0 * r * log_ratio * (1.0 - t));
        sum += (i == 0 || i == n ? 1.0 : (i % 2 ? 4.0 : 2.0)) * value;
    }
    return sum / (3.0 * n * 197.3269804);
}

extern "C" double barrier_action(double r, double b, double q, double c, double d, double mu) {
    return integrate(r, b, q, c, d, mu, false, 4096);
}
extern "C" double barrier_action_portable(double r, double b, double q, double c, double d, double mu) {
    return integrate(r, b, q, c, d, mu, true, 4096);
}
extern "C" double barrier_action_refined(double r, double b, double q, double c, double d, double mu) {
    return integrate(r, b, q, c, d, mu, true, 8192);
}
