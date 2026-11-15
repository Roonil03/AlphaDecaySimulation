FROM golang:1.26-alpine AS build
WORKDIR /src
COPY services/viewer/go.mod services/viewer/go.sum ./
RUN go mod download
COPY services/viewer/ ./
RUN CGO_ENABLED=0 go build -o /viewer .
FROM alpine:3.22
WORKDIR /app
COPY --from=build /viewer /viewer
COPY observatory/isotopes.json /app/observatory/isotopes.json
ENV GATEWAY_URL=http://gateway:8080
EXPOSE 8090
USER 65532:65532
ENTRYPOINT ["/viewer"]
