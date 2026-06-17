FROM python:3.12-slim

WORKDIR /app

COPY apps/approver/ apps/approver/

EXPOSE 8082

CMD ["python3", "apps/approver/server.py"]
