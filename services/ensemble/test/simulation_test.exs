defmodule Ensemble.SimulationTest do
  use ExUnit.Case
  test "seeded population stays consistent over unordered and repeated times" do
    input = %{"log10_half_life_s" => 0, "times_s" => [2, 0, 1, 1], "population" => 10000, "seed" => 42}
    assert {:ok, result} = Ensemble.Simulation.run(input)
    assert {:ok, ^result} = Ensemble.Simulation.run(input)
    assert [a, 10000, b, b] = result.sampled_survivors
    assert a <= b and abs(b - 5000) < 250
    assert [quarter, 10000.0, half, half] = result.expected_survivors
    assert_in_delta quarter, 2500, 0.001
    assert_in_delta half, 5000, 0.001
  end
  test "extreme half lives and validation" do
    for {h, expected} <- [{1.0e6, [12, 12]}, {-1.0e6, [12, 0]}] do
      assert {:ok, r} = Ensemble.Simulation.run(%{"log10_half_life_s" => h, "times_s" => [0, 1], "population" => 12, "seed" => -1})
      assert r.sampled_survivors == expected
    end
    assert {:error, _} = Ensemble.Simulation.run(%{"log10_half_life_s" => 0, "times_s" => [-1], "population" => 1, "seed" => 1})
    assert {:error, _} = Ensemble.Simulation.run(%{"log10_half_life_s" => 0, "times_s" => [], "population" => 0, "seed" => 1})
  end

  test "JSON floating-point zero preserves the initial population" do
    for half <- [0, 1.0e6, -1.0e6] do
      input = Jason.decode!(~s({"log10_half_life_s": #{half}, "times_s": [0.0, 0, 1], "population": 12, "seed": 42}))
      assert {:ok, result} = Ensemble.Simulation.run(input)
      assert [12, 12, _] = result.sampled_survivors
      assert [12.0, 12.0, _] = result.expected_survivors
    end
  end
end
