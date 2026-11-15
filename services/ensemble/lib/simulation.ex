defmodule Ensemble.Simulation do
  @max_population 100_000
  @max_times 1_000
  @ln2 :math.log(2.0)
  @ln10 :math.log(10.0)

  def run(%{"log10_half_life_s" => half, "times_s" => times, "population" => population, "seed" => seed})
      when is_number(half) and is_list(times) and is_integer(population) and
             population > 0 and population <= @max_population and is_integer(seed) and
             seed >= -9_223_372_036_854_775_808 and seed <= 9_223_372_036_854_775_807 do
    if length(times) <= @max_times and abs(half) <= 1.0e6 and
         Enum.all?(times, &(is_number(&1) and &1 >= 0 and &1 <= 1.0e300)) do
      simulate(half, times, population, seed)
    else
      {:error, "invalid half-life or times"}
    end
  end
  def run(_), do: {:error, "invalid population, seed, or required fields"}

  defp survival(0, _half), do: 1.0
  defp survival(t, half) do
    # log(lambda*t) avoids overflow/underflow for extreme half-lives.
    log_x = :math.log(@ln2) + :math.log(t) - half * @ln10
    cond do
      log_x > 6.62 -> 0.0
      log_x < -36.0 -> 1.0
      true -> :math.exp(-:math.exp(log_x))
    end
  end

  defp simulate(half, times, population, seed) do
    ordered = times |> Enum.with_index() |> Enum.sort_by(fn {t, i} -> {t, i} end)
    probabilities = ordered |> Enum.map(fn {t, _} -> survival(t, half) end) |> List.to_tuple()
    count = tuple_size(probabilities)
    rng = :rand.seed_s(:exsss, {seed_word(seed, 1), seed_word(seed, 2), seed_word(seed, 3)})
    buckets = :array.new(count + 1, default: 0)
    {buckets, _rng} = Enum.reduce(1..population, {buckets, rng}, fn _, {acc, state} ->
      {u, state} = :rand.uniform_s(state)
      n = surviving_prefix(probabilities, u, 0, count)
      {:array.set(n, :array.get(n, acc) + 1, acc), state}
    end)
    {values, _} = Enum.map_reduce(0..count, population, fn i, remaining ->
      next = remaining - :array.get(i, buckets)
      {next, next}
    end)
    sampled = values |> Enum.take(count) |> Enum.zip(ordered) |> Enum.sort_by(fn {_, {_, i}} -> i end) |> Enum.map(&elem(&1, 0))
    expected = ordered |> Enum.map(fn {t, i} -> {i, population * survival(t, half)} end) |> Enum.sort_by(&elem(&1, 0)) |> Enum.map(&elem(&1, 1))
    {:ok, %{expected_survivors: expected, sampled_survivors: sampled}}
  end

  defp seed_word(seed, salt), do: :erlang.phash2({seed, salt}, 4_294_967_295) + 1
  defp surviving_prefix(_p, _u, lo, hi) when lo >= hi, do: lo
  defp surviving_prefix(p, u, lo, hi) do
    mid = div(lo + hi, 2)
    if u <= elem(p, mid), do: surviving_prefix(p, u, mid + 1, hi), else: surviving_prefix(p, u, lo, mid)
  end
end
