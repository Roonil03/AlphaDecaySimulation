defmodule Campaign do
  @moduledoc "Local campaign runner. Use campaign.sh for exclusive OS-backed locking."
  use GenServer
  def start_link(opts), do: GenServer.start_link(__MODULE__, opts)
  def pause(dir), do: File.write!(Path.join(dir, "paused"), "paused\n")
  def resume(dir), do: File.rm(Path.join(dir, "paused"))
  def init(opts) do
    dir = Keyword.fetch!(opts, :dir)
    File.mkdir_p!(Path.join(dir, "results"))
    manifest = Jason.decode!(File.read!(Keyword.fetch!(opts, :manifest)))
    cases = Map.fetch!(manifest, "cases")
    ids = Enum.map(cases, &Map.fetch!(&1, "id"))
    if Enum.any?(ids, &(not is_binary(&1) or not Regex.match?(~r/\A[a-zA-Z0-9_-]+\z/, &1))) or length(ids) != length(Enum.uniq(ids)), do: raise("IDs must be unique safe strings")
    snapshot = Path.join(dir, "manifest.json")
    canonical = Jason.encode!(manifest)
    if File.exists?(snapshot) and File.read!(snapshot) != canonical, do: raise("campaign manifest changed")
    unless File.exists?(snapshot), do: atomic_write(snapshot, canonical)
    recover_history(dir, ids)
    send(self(), :next)
    {:ok, %{dir: dir, cases: cases, scheduler: Keyword.fetch!(opts, :scheduler), core: Keyword.fetch!(opts, :core), active: nil}}
  end
  def handle_info(:next, %{active: nil} = state) do
    if File.exists?(Path.join(state.dir, "paused")) do
      Process.send_after(self(), :next, 250); {:noreply, state}
    else
      case Enum.find(state.cases, fn c -> not File.exists?(result_path(state.dir, c["id"])) end) do
        nil -> event(state.dir, %{event: "complete"}); {:stop, :normal, state}
        c ->
          event(state.dir, %{event: "started", id: c["id"]})
          task = Task.async(fn ->
            path = Path.join(state.dir, "input.json")
            atomic_write(path, Jason.encode!(%{cases: [c]}))
            System.cmd("sh", ["-c", "exec \"$1\" -core \"$2\" < \"$3\"", "campaign", state.scheduler, state.core, path], stderr_to_stdout: true)
          end)
          {:noreply, %{state | active: {task.ref, c}}}
      end
    end
  end
  def handle_info({ref, {output, status}}, %{active: {ref, c}} = state) do
    Process.demonitor(ref, [:flush])
    result = if status == 0 do
      case Jason.decode(output) do
        {:ok, [%{"id" => id, "response" => _} = r]} -> if id == c["id"], do: {:ok, r}, else: :error
        _ -> :error
      end
    else :error end
    case result do
      {:ok, r} -> atomic_write(result_path(state.dir, c["id"]), Jason.encode!(r)); event(state.dir, %{event: "succeeded", id: c["id"]}); send(self(), :next); {:noreply, %{state | active: nil}}
      :error -> event(state.dir, %{event: "failed", id: c["id"], detail: output}); {:stop, {:case_failed, c["id"]}, state}
    end
  end
  def handle_info({:DOWN, ref, :process, _, reason}, %{active: {ref, c}} = state), do: {:stop, {:worker_failed, c["id"], reason}, state}
  # Results are the durable commit record. Reconcile a crash between rename and
  # event append, and discard a torn final event before adding recovery events.
  defp recover_history(dir, ids) do
    path = Path.join(dir, "events.jsonl")
    lines = if File.exists?(path), do: String.split(File.read!(path), "\n", trim: true), else: []
    decoded = Enum.map(lines, &Jason.decode/1)
    valid = Enum.take_while(decoded, &match?({:ok, _}, &1))
    if length(valid) != length(decoded) do
      if length(valid) != length(decoded) - 1, do: raise("corrupt campaign event history")
      content = Enum.map_join(valid, "", fn {:ok, item} -> Jason.encode!(item) <> "\n" end)
      atomic_write(path, content)
    end
    succeeded = for {:ok, %{"event" => "succeeded", "id" => id}} <- valid, do: id
    for id <- ids, File.exists?(result_path(dir, id)), id not in succeeded do
      event(dir, %{event: "succeeded", id: id, recovered: true})
    end
  end
  defp result_path(dir, id), do: Path.join([dir, "results", id <> ".json"])
  defp event(dir, value), do: File.write!(Path.join(dir, "events.jsonl"), Jason.encode!(value) <> "\n", [:append, :sync])
  defp atomic_write(path, content) do
    temp = path <> ".tmp"
    File.write!(temp, content, [:sync]); File.rename!(temp, path)
  end
end

