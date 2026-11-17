defmodule Campaign.MixProject do
  use Mix.Project
  def project, do: [app: :campaign, version: "0.1.0", elixir: "~> 1.14", deps: [{:jason, "~> 1.4"}]]
  def application, do: [extra_applications: [:logger]]
end
