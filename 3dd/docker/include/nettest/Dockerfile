FROM python:3.12-slim

WORKDIR /app

COPY apps/nettest/ apps/nettest/

EXPOSE 8081

CMD ["python3", "apps/nettest/server.py"]
