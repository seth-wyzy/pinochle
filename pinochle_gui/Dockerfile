# Stage 1: Build the React Frontend
FROM node:18-alpine as build-stage
WORKDIR /app
# Note: Render builds from the "Root Directory" if set. 
# These COPY commands expect to be in the pinochle_gui directory.
COPY frontend/package*.json ./frontend/
RUN cd frontend && npm install
COPY frontend/ ./frontend/
RUN cd frontend && npm run build

# Stage 2: Build the FastAPI Backend
FROM python:3.11-slim
WORKDIR /app

# Install dependencies
COPY backend/requirements.txt ./backend/
RUN pip install --no-cache-dir -r backend/requirements.txt

# Copy backend code
COPY backend/ ./backend/

# Copy the frontend build to multiple locations just to be safe
# 1. To the static folder in backend (where main.py looks)
COPY --from=build-stage /app/frontend/build /app/backend/static
# 2. To the original location for backward compatibility
COPY --from=build-stage /app/frontend/build /app/frontend/build

# Final directory for execution
WORKDIR /app/backend

EXPOSE 8000
CMD ["uvicorn", "main:app", "--host", "0.0.0.0", "--port", "8000"]
