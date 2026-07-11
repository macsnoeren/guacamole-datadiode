FROM python:3.12-slim

WORKDIR /app

COPY proxies/nettest nettest

EXPOSE 8081

CMD ["python3", "/app/nettest/server.py"]