FROM python:3.12-alpine

WORKDIR /app

COPY requirements.txt .
RUN pip install --no-cache-dir -r requirements.txt

COPY app.py .
COPY entrypoint.sh /entrypoint.sh
RUN chmod +x /entrypoint.sh

# UDP listener (default 1111) and web viewer (default 80)
EXPOSE 1111/udp
EXPOSE 80/tcp

ENTRYPOINT ["/entrypoint.sh"]
