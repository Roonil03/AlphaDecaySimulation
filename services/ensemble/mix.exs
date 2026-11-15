defmodule Ensemble.MixProject do
  use Mix.Project
  def project, do: [app: :ensemble, version: "0.1.0", elixir: "~> 1.16", start_permanent: Mix.env() == :prod, deps: deps()]
  def application, do: [extra_applications: [:logger], mod: {Ensemble.Application, []}]
  defp deps, do: [{:plug_cowboy, "~> 2.7"}, {:jason, "~> 1.4"}]
end
