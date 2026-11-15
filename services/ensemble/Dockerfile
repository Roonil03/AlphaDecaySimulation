FROM elixir:1.18-alpine AS build
WORKDIR /app
RUN mix local.hex --force && mix local.rebar --force
COPY mix.exs ./
RUN mix deps.get
COPY lib ./lib
ENV MIX_ENV=prod
RUN mix compile
EXPOSE 8082
CMD ["mix", "run", "--no-halt"]
