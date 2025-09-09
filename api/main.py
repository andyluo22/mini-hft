import os
import httpx
from fastapi import FastAPI
from fastapi.responses import PlainTextResponse
from fastapi.middleware.cors import CORSMiddleware  # <-- add this

ENGINE_URL = os.getenv("ENGINE_URL", "http://engine:8080")
app = FastAPI(title="mini-hft API", version="0.0.1")

# Enable CORS so the web app (localhost:3000) can talk to this API
app.add_middleware(
    CORSMiddleware,
    allow_origins=["http://localhost:3000"],  # later add your Vercel URL too
    allow_methods=["*"],
    allow_headers=["*"],
)

@app.get("/health")
def health():
    return {"status": "ok"}

@app.get("/metrics-proxy")
async def metrics_proxy():
    url = f"{ENGINE_URL}/metrics"
    async with httpx.AsyncClient(timeout=2.0) as client:
        r = await client.get(url)
        r.raise_for_status()
        return PlainTextResponse(r.text, media_type="text/plain; version=0.0.4")