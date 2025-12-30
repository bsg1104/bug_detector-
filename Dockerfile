FROM alpine:3.18 AS build
RUN apk add --no-cache build-base linux-headers git curl \
    libmicrohttpd-dev hiredis-dev
WORKDIR /src
COPY . .
RUN make

FROM alpine:3.18
RUN apk add --no-cache libmicrohttpd hiredis
WORKDIR /app
COPY --from=build /src/ratelimiter /app/ratelimiter
EXPOSE 8080 9090
ENTRYPOINT ["/app/ratelimiter"]
