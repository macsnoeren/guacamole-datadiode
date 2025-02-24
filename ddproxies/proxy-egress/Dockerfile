FROM alpine:latest

RUN apk update
RUN apk add gcompat
RUN apk add libc6-compat
RUN apk add build-base

WORKDIR /bin

COPY . .

ENTRYPOINT ["guac-proxy-egress"]
