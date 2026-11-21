defmodule Campaign do
  @moduledoc "Local bounded campaign runner with durable per-case commits."
  use GenServer
  def child_spec(opts), do: %{id: __MODULE__, start: {__MODULE__, :start_link, [opts]}, restart: :temporary}
  def start_link(opts), do: GenServer.start_link(__MODULE__, opts)
  def pause(dir), do: File.write!(Path.join(dir, "paused"), "paused\n")
  def resume(dir), do: File.rm(Path.join(dir, "paused"))

  def init(opts) do
    dir = Keyword.fetch!(opts, :dir)
    File.mkdir_p!(Path.join(dir, "results"))
    File.mkdir_p!(Path.join(dir, "attempts"))
    File.mkdir_p!(Path.join(dir, "inputs"))
    manifest = Jason.decode!(File.read!(Keyword.fetch!(opts, :manifest)))
    cases = Map.fetch!(manifest, "cases")
    ids = Enum.map(cases, &Map.fetch!(&1, "id"))
    if Enum.any?(ids, &(not is_binary(&1) or not Regex.match?(~r/\A[a-zA-Z0-9_-]+\z/, &1))) or length(ids) != length(Enum.uniq(ids)), do: raise("IDs must be unique safe strings")
    snapshot = Path.join(dir, "manifest.json")
    canonical = Jason.encode!(manifest)
    if File.exists?(snapshot) and File.read!(snapshot) != canonical, do: raise("campaign manifest changed")
    unless File.exists?(snapshot), do: atomic_write(snapshot, canonical)
    workers = Keyword.get(opts, :workers, 1)
    if workers < 1, do: raise("workers must be positive")
    recover_history(dir, ids)
    send(self(), :next)
    {:ok, %{dir: dir, cases: cases, scheduler: Keyword.fetch!(opts, :scheduler), core: Keyword.fetch!(opts, :core), workers: workers, active: %{}, failure: nil}}
  end

  def handle_info(:next, state) do
    active_ids = Enum.map(state.active, fn {_, c} -> c["id"] end)
    pending = Enum.reject(state.cases, fn c -> File.exists?(result_path(state.dir, c["id"])) or c["id"] in active_ids end)
    cond do
      state.failure && map_size(state.active) == 0 -> {:stop, state.failure, state}
      state.failure -> {:noreply, state}
      File.exists?(Path.join(state.dir, "paused")) -> Process.send_after(self(), :next, 250); {:noreply, state}
      pending == [] and map_size(state.active) == 0 -> event(state.dir, %{event: "complete"}); {:stop, :normal, state}
      true ->
        selected = Enum.take(pending, state.workers - map_size(state.active))
        state = Enum.reduce_while(selected, state, fn c, acc ->
          id = c["id"]
          attempt_file = Path.join([acc.dir, "attempts", id <> ".json"])
          attempt = if File.exists?(attempt_file), do: Jason.decode!(File.read!(attempt_file))["count"] + 1, else: 1
          if File.exists?(Path.join([acc.dir, "attempts", id <> ".rejected"])) or attempt > 3 do
            {:halt, %{acc | failure: {:attempts_exhausted, id}}}
          else
            atomic_write(attempt_file, Jason.encode!(%{count: attempt}))
            event(acc.dir, %{event: "started", id: id, attempt: attempt})
            path = Path.join([acc.dir, "inputs", id <> ".json"])
            atomic_write(path, Jason.encode!(%{cases: [c]}))
            task = Task.Supervisor.async_nolink(Campaign.Tasks, fn ->
              System.cmd(acc.scheduler, ["-core", acc.core, "-input", path], stderr_to_stdout: true)
            end)
            {:cont, %{acc | active: Map.put(acc.active, task.ref, c)}}
          end
        end)
        if state.failure && map_size(state.active) == 0, do: send(self(), :next)
        {:noreply, state}
    end
  end
  def handle_info({ref, {output, status}}, state) when is_reference(ref) do
    Process.demonitor(ref, [:flush])
    {c, active} = Map.pop(state.active, ref)
    state = %{state | active: active}
    expected = c["id"]
    parsed = if status == 0, do: Jason.decode(output), else: :error
    state = case parsed do
      {:ok, [%{"id" => ^expected, "response" => _} = r]} ->
        atomic_write(result_path(state.dir, expected), Jason.encode!(r)); event(state.dir, %{event: "succeeded", id: expected}); state
      {:ok, [%{"id" => ^expected, "error_kind" => "domain"} = r]} ->
        atomic_write(Path.join([state.dir, "attempts", expected <> ".rejected"]), Jason.encode!(r)); event(state.dir, %{event: "rejected", id: expected, detail: r["error"]}); %{state | failure: {:domain_rejection, expected}}
      _ -> event(state.dir, %{event: "failed", id: c["id"], detail: output}); state
    end
    send(self(), :next); {:noreply, state}
  end
  def handle_info({:DOWN, ref, :process, _, reason}, state) do
    {c, active} = Map.pop(state.active, ref)
    if c, do: event(state.dir, %{event: "failed", id: c["id"], detail: inspect(reason)})
    send(self(), :next); {:noreply, %{state | active: active}}
  end
  defp recover_history(dir, ids) do
    path = Path.join(dir, "events.jsonl")
    content = if File.exists?(path), do: File.read!(path), else: ""
    lines = String.split(content, "\n", trim: true)
    decoded = Enum.map(lines, &Jason.decode/1)
    valid = Enum.take_while(decoded, &match?({:ok, _}, &1))
    if length(valid) != length(decoded) do
      if length(valid) != length(decoded) - 1, do: raise("corrupt campaign event history")
      atomic_write(path, Enum.map_join(valid, "", fn {:ok, item} -> Jason.encode!(item) <> "\n" end))
    else
      if content != "" and not String.ends_with?(content, "\n"), do: atomic_write(path, content <> "\n")
    end
    succeeded = for {:ok, %{"event" => "succeeded", "id" => id}} <- valid, do: id
    for id <- ids, File.exists?(result_path(dir, id)), id not in succeeded, do: event(dir, %{event: "succeeded", id: id, recovered: true})
  end
  defp result_path(dir, id), do: Path.join([dir, "results", id <> ".json"])
  defp event(dir, value), do: File.write!(Path.join(dir, "events.jsonl"), Jason.encode!(value) <> "\n", [:append, :sync])
  defp atomic_write(path, content) do
    temp = path <> ".tmp"
    File.write!(temp, content, [:sync]); File.rename!(temp, path)
  end
end

