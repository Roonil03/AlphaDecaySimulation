[dir, manifest, scheduler, core | rest] = System.argv()
workers = case rest do [] -> 1; [n] -> String.to_integer(n) end
{:ok, pid} = DynamicSupervisor.start_child(Campaign.Supervisor,
  {Campaign, [dir: dir, manifest: manifest, scheduler: scheduler, core: core, workers: workers]})
Process.monitor(pid)
receive do
  {:DOWN, _, :process, _, :normal} -> :ok
  {:DOWN, _, :process, _, reason} -> IO.inspect(reason); System.halt(1)
end


