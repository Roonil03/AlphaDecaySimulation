defmodule CampaignTest do
  use ExUnit.Case
  test "pauses, resumes and preserves successful results on replay" do
    dir = Path.join(System.tmp_dir!(), "campaign-#{System.unique_integer([:positive])}")
    File.mkdir_p!(dir)
    on_exit(fn -> File.rm_rf!(dir) end)
    manifest = Path.join(dir, "input-manifest.json")
    File.write!(manifest, Jason.encode!(%{cases: [%{id: "one", request: %{schema_version: 1}}]}))
    scheduler = Path.join(dir, "scheduler")
    File.write!(scheduler, "#!/bin/sh\nprintf '[{\"id\":\"one\",\"response\":{\"schema_version\":1}}]'\n")
    File.chmod!(scheduler, 0o755)
    Campaign.pause(dir)
    {:ok, pid} = Campaign.start_link(dir: dir, manifest: manifest, scheduler: scheduler, core: "unused")
    ref = Process.monitor(pid)
    Process.sleep(300)
    refute File.exists?(Path.join([dir, "results", "one.json"]))
    Campaign.resume(dir)
    assert_receive {:DOWN, ^ref, :process, ^pid, :normal}, 5000
    result = File.read!(Path.join([dir, "results", "one.json"]))
    File.write!(Path.join(dir, "events.jsonl"), "{\"event\":\"started\",\"id\":\"one\"}
{\"event\":")
    {:ok, pid2} = Campaign.start_link(dir: dir, manifest: manifest, scheduler: "/missing", core: "unused")
    ref2 = Process.monitor(pid2)
    assert_receive {:DOWN, ^ref2, :process, ^pid2, :normal}, 5000
    assert File.read!(Path.join([dir, "results", "one.json"])) == result
    events = File.read!(Path.join(dir, "events.jsonl"))
    assert length(Regex.scan(~r/"event":"succeeded"/, events)) == 1
    assert events =~ "recovered"
  end
  test "retries an infrastructure crash and records attempts" do
    dir = Path.join(System.tmp_dir!(), "retry-#{System.unique_integer([:positive])}")
    File.mkdir_p!(dir)
    on_exit(fn -> File.rm_rf!(dir) end)
    manifest = Path.join(dir, "manifest.json")
    File.write!(manifest, Jason.encode!(%{cases: [%{id: "one", request: %{}}]}))
    scheduler = Path.join(dir, "scheduler")
    marker = Path.join(dir, "marker")
    File.write!(scheduler, "#!/bin/sh\nif [ ! -f '#{marker}' ]; then touch '#{marker}'; exit 1; fi\nprintf '[{\"id\":\"one\",\"response\":{}}]'\n")
    File.chmod!(scheduler, 0o755)
    {:ok, pid} = Campaign.start_link(dir: dir, manifest: manifest, scheduler: scheduler, core: "unused")
    ref = Process.monitor(pid)
    assert_receive {:DOWN, ^ref, :process, ^pid, :normal}, 5000
    assert Jason.decode!(File.read!(Path.join([dir, "attempts", "one.json"]))) == %{"count" => 2}
    assert File.exists?(Path.join([dir, "results", "one.json"]))
  end
end



