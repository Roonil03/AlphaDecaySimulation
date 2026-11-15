defmodule Ensemble.Router do
  use Plug.Router
  plug :match
  plug :dispatch

  get "/health" do
    send_json(conn, 200, %{status: "ok"})
  end

  post "/v1/ensemble" do
    with {:ok, body, conn} <- Plug.Conn.read_body(conn, length: 1_048_576, read_length: 1_048_576, read_timeout: 5_000),
         {:ok, input} <- Jason.decode(body) do
      try do
        task = Task.Supervisor.async_nolink(Ensemble.Tasks, fn -> Ensemble.Simulation.run(input) end)
        case Task.yield(task, 10_000) || Task.shutdown(task, :brutal_kill) do
          {:ok, {:ok, result}} -> send_json(conn, 200, result)
          {:ok, {:error, message}} -> send_json(conn, 400, %{error: message})
          nil -> send_json(conn, 504, %{error: "simulation timed out"})
          _ -> send_json(conn, 500, %{error: "simulation failed"})
        end
      rescue
        RuntimeError -> send_json(conn, 503, %{error: "simulation capacity reached"})
      end
    else
      {:more, _, conn} -> send_json(conn, 413, %{error: "request too large"})
      _ -> send_json(conn, 400, %{error: "invalid JSON or request body"})
    end
  end

  match _ do
    send_json(conn, 404, %{error: "not found"})
  end

  defp send_json(conn, status, data) do
    conn |> put_resp_content_type("application/json") |> send_resp(status, Jason.encode!(data))
  end
end
