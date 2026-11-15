FROM golang:1.24-alpine AS build
WORKDIR /src
COPY go.mod main.go main_test.go ./
RUN CGO_ENABLED=0 go test ./... && CGO_ENABLED=0 go build -o /gateway .
FROM alpine:3.21
RUN adduser -D -H app
USER app
COPY --from=build /gateway /gateway
EXPOSE 8080
ENTRYPOINT ["/gateway"]
