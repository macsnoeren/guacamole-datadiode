FROM python:3.12-alpine

WORKDIR /app

COPY requirements.txt .
RUN pip install --no-cache-dir -r requirements.txt

COPY app.py .
COPY entrypoint.sh /entrypoint.sh
RUN chmod +x /entrypoint.sh

# Web UI / upload endpoint
EXPOSE 80/tcp
# UDP diode link (sender out / receiver in)
EXPOSE 40000/udp

ENTRYPOINT ["/entrypoint.sh"]
