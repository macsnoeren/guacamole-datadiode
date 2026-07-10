FROM python:3.11-slim

WORKDIR /app
COPY lbridgeadapter.py /app/lbridgeadapter.py

# For logging in containers
ENV PYTHONUNBUFFERED=1

CMD ["python", "/app/lbridgeadapter.py"]
