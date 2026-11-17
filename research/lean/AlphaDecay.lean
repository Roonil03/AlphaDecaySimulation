import Mathlib

/- Small invariants for the rate equation used by the native model. These results
   do not establish numerical accuracy or the physical adequacy of the model. -/
namespace AlphaDecay

theorem rate_positive {assault preformation penetration : ℝ}
    (ha : 0 < assault) (hp : 0 < preformation) (ht : 0 < penetration) :
    0 < assault * preformation * penetration := by positivity

theorem closed_channel_zero {assault preformation : ℝ} :
    assault * preformation * 0 = 0 := by ring

theorem finite_branches_sum_one {ι : Type*} (channels : Finset ι) (rate : ι → ℝ)
    (h : 0 < ∑ i ∈ channels, rate i) :
    (∑ i ∈ channels, rate i / (∑ j ∈ channels, rate j)) = 1 := by
  rw [← Finset.sum_div]
  exact div_self (ne_of_gt h)

theorem branches_sum_one {a b : ℝ} (ha : 0 ≤ a) (hb : 0 ≤ b)
    (h : 0 < a + b) : a / (a + b) + b / (a + b) = 1 := by
  have hn : a + b ≠ 0 := ne_of_gt h
  field_simp [hn]

theorem coulomb_turning_point {c q : ℝ} (hc : 0 < c) (hq : 0 < q) :
    0 < c / q ∧ c / (c / q) = q := by
  constructor
  · positivity
  · field_simp [ne_of_gt hc, ne_of_gt hq]

theorem turning_point_outside_radius {c q r : ℝ} (hq : 0 < q)
    (h : r * q < c) : r < c / q := by
  exact (lt_div_iff₀ hq).2 h
end AlphaDecay



