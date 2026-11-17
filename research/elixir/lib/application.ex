defmodule Campaign.Application do
  use Application
  def start(_type, _args) do
    Supervisor.start_link([{Task.Supervisor, name: Campaign.Tasks}, {DynamicSupervisor, strategy: :one_for_one, name: Campaign.Supervisor}],
      strategy: :one_for_one, name: Campaign.Root)
  end
end

