defmodule Ensemble.Application do
  use Application
  def start(_type, _args) do
    children = [
      {Task.Supervisor, name: Ensemble.Tasks, max_children: 32},
      {Plug.Cowboy, scheme: :http, plug: Ensemble.Router, options: [port: 8082]}
    ]
    Supervisor.start_link(children, strategy: :one_for_one, name: Ensemble.Supervisor)
  end
end
